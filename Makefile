SHELL := /bin/bash
.DEFAULT_GOAL := default
IMAGE ?= ps4-games-installer:build
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT := 1
.PHONY: default all image test check preview screenshots clean validate package package-internal assets deploy compile-commands

default: package

image:
	docker build --label org.psgi.dockerfile="$$(sha256sum Dockerfile | cut -d ' ' -f1)" -t $(IMAGE) .

.PHONY: lsp-ps4 lsp-host
lsp-ps4:
	@printf '%s\n' \
		'CompileFlags:' \
		'  CompilationDatabase: .cdb/ps4' \
		'---' \
		'If:' \
		'  PathMatch: (src/desktop\.cpp|tests/.*)' \
		'CompileFlags:' \
		'  CompilationDatabase: .cdb/host' > .clangd
lsp-host:
	@printf '%s\n' \
		'CompileFlags:' \
		'  CompilationDatabase: .cdb/host' \
		'---' \
		'If:' \
		'  PathMatch: (src/(console|install|lifecycle|storage_native|filesystem_native)\.cpp|third_party/cjson/.*)' \
		'CompileFlags:' \
		'  CompilationDatabase: .cdb/ps4' > .clangd

ifneq ($(PSGI_IN_CONTAINER),1)
all package test check screenshots validate deploy:
	@test "$$(docker image inspect $(IMAGE) --format '{{index .Config.Labels "org.psgi.dockerfile"}}' 2>/dev/null)" = "$$(sha256sum Dockerfile | cut -d ' ' -f1)" || $(MAKE) image
	docker run --rm --user "$$(id -u):$$(id -g)" -v "$(CURDIR):/work" \
    	-e SDL_VIDEODRIVER=dummy -e PSGI_IN_CONTAINER=1 $(IMAGE) make $@ CONFIG="$(CONFIG)"
preview:
	@test "$$(docker image inspect $(IMAGE) --format '{{index .Config.Labels "org.psgi.dockerfile"}}' 2>/dev/null)" = "$$(sha256sum Dockerfile | cut -d ' ' -f1)" || $(MAKE) image
	@if [[ -z "$$DISPLAY" ]]; then $(MAKE) screenshots; else \
		preview_args=(--rm --user "$$(id -u):$$(id -g)" -v "$(CURDIR):/work" -v /tmp/.X11-unix:/tmp/.X11-unix:ro -e DISPLAY -e SDL_VIDEODRIVER=x11 -e PSGI_IN_CONTAINER=1); \
		if [[ -n "$$XAUTHORITY" && -f "$$XAUTHORITY" ]]; then preview_args+=(-v "$$XAUTHORITY:/tmp/psgi-xauth:ro" -e XAUTHORITY=/tmp/psgi-xauth); fi; \
		docker run "$${preview_args[@]}" $(IMAGE) make preview; fi
clean:
	rm -rf build dist

ifneq ($(and $(OO_PS4_TOOLCHAIN),$(wildcard $(OO_PS4_TOOLCHAIN)/link.x)),)
HOST_SDK := $(patsubst %/,%,$(OO_PS4_TOOLCHAIN))
SDK_DEP :=
else
HOST_SDK := $(CURDIR)/.sdk
SDK_DEP := .sdk
endif

all: compile-commands

.sdk:
	@test "$$(docker image inspect $(IMAGE) --format '{{index .Config.Labels "org.psgi.dockerfile"}}' 2>/dev/null)" = "$$(sha256sum Dockerfile | cut -d ' ' -f1)" || $(MAKE) image
	@id=$$(docker create $(IMAGE)) && rm -rf .sdk && docker cp $$id:/opt/openorbis .sdk; docker rm $$id >/dev/null

compile-commands: $(SDK_DEP)
	@test "$$(docker image inspect $(IMAGE) --format '{{index .Config.Labels "org.psgi.dockerfile"}}' 2>/dev/null)" = "$$(sha256sum Dockerfile | cut -d ' ' -f1)" || $(MAKE) image
	docker run --rm --user "$$(id -u):$$(id -g)" -v "$(CURDIR):/work" \
		-e PSGI_IN_CONTAINER=1 -e HOST_ROOT="$(CURDIR)" -e HOST_SDK="$(HOST_SDK)" $(IMAGE) make compile-commands-internal
else
SDK := $(OO_PS4_TOOLCHAIN)
TOOLS := $(SDK)/bin/linux
TITLE_ID := PGIN00001
CONTENT_ID := IV0000-$(TITLE_ID)_00-PS4GAMEINSTALLER
CROSSFLAGS := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -D_POSIX_C_SOURCE=200809L \
	-isysroot $(SDK) -isystem $(SDK)/include -Isrc -Ithird_party/cjson -Ithird_party/stb -Ibuild/generated -O2 -g -Wall -Wextra -MMD -MP
COMMON := catalog metadata settings archive_auth library sfo installed storage userdata package queue backup source maintenance ui ui_features ui_browsing ui_details browser log
UI_SOURCES := controller canvas artwork render app download demo video main
CONSOLE := $(UI_SOURCES) http install console lifecycle storage_native filesystem_native
PS4_OBJECTS := $(addprefix build/ps4/,$(addsuffix .o,$(COMMON) $(CONSOLE))) build/ps4/cJSON.o
PS4_CDB_SRCS := $(addprefix src/,$(addsuffix .cpp,$(COMMON) $(CONSOLE))) third_party/cjson/cJSON.c
HOST_CDB_SRCS := $(addprefix src/,$(addsuffix .cpp,$(COMMON) $(UI_SOURCES) http desktop)) $(wildcard tests/*.cpp)
LIBS := -ljbc -lc++ -lSceVideoOut -lSceSysmodule -lSceLncUtil -lSceSystemService -lSceFreeType -lScePad -lSceUserService -lSceNet -lSceNetCtl -lSceSsl -lSceCommonDialog -lSceImeDialog -lc -lkernel
HOSTFLAGS := -std=c++17 -DPSGI_HOST -Isrc -Ithird_party/cjson -Ithird_party/stb -Ibuild/generated -Wall -Wextra -Werror -O1 -g -pthread
HEADERS := $(wildcard src/*.hpp) third_party/cjson/cJSON.h third_party/stb/stb_image.h
HOST_COMMON := $(addprefix build/host/common/,$(addsuffix .o,$(COMMON)))
CDB := .cdb
CDB_CROSS := $(filter-out -MMD -MP,$(CROSSFLAGS))
CDB_HOST := $(filter-out -Werror,$(HOSTFLAGS))
CDB_MAP := /work=$(HOST_ROOT) $(SDK)=$(HOST_SDK)

all package: assets
	python3 scripts/package.py

deploy:
	python3 scripts/deploy.py

assets: assets/icon.png
assets/icon.png: assets/icon.svg
	rsvg-convert $< -o build-icon.tmp
	python3 -c 'from PIL import Image; Image.open("build-icon.tmp").convert("RGB").save("assets/icon.png")'
	rm build-icon.tmp

build/ps4/%.o: src/%.cpp
	@mkdir -p $(@D)
	clang++ $(CROSSFLAGS) -MJ $(@:.o=.json) -std=c++17 -isystem $(SDK)/include/c++/v1 -c $< -o $@
build/ps4/http.o: CROSSFLAGS += -Wframe-larger-than=8192 -Werror=frame-larger-than
build/ps4/cJSON.o: third_party/cjson/cJSON.c
	@mkdir -p $(@D)
	clang $(CROSSFLAGS) -MJ $(@:.o=.json) -c $< -o $@

build/app.elf: $(PS4_OBJECTS) Makefile
	ld.lld -m elf_x86_64 -pie --script $(SDK)/link.x --eh-frame-hdr -L$(SDK)/lib -o $@ $(SDK)/lib/crt1.o $(PS4_OBJECTS) $(LIBS)

build/pkg/eboot.bin: build/app.elf scripts/validate_imports.py
	@mkdir -p $(@D)
	$(TOOLS)/create-fself -in=$< -out=build/app.oelf --eboot $@ --paid 0x3800000000000011
	python3 scripts/validate_imports.py build/app.oelf

package-internal: build/pkg/eboot.bin assets
	python3 scripts/metadata.py "$(SDK)" "$(REVISION)" "$(CONTENT_ID)" "$(TITLE_ID)"
	cd build/pkg && $(TOOLS)/PkgTool.Core pkg_build pkg.gp4 ../package
	$(TOOLS)/PkgTool.Core pkg_validate build/package/$(CONTENT_ID).pkg

compile-commands-internal: build/generated/revision.hpp
	@rm -rf $(CDB) && mkdir -p $(CDB)/frag-ps4 $(CDB)/frag-host
	@set -e; for f in $(PS4_CDB_SRCS); do \
	  	j=$(CDB)/frag-ps4/$$(echo $$f | tr / _).json; \
	  	case $$f in \
	    	*.c) clang $(CDB_CROSS) -fsyntax-only -MJ $$j $$f ;; \
	    	*)   clang++ $(CDB_CROSS) -std=c++17 -isystem $(SDK)/include/c++/v1 -fsyntax-only -MJ $$j $$f ;; \
	  	esac; done
	@for f in $(HOST_CDB_SRCS); do \
	  	clang++ $(CDB_HOST) -idirafter $(SDK)/include $$(pkg-config --cflags freetype2 sdl2) \
	    	-fsyntax-only -MJ $(CDB)/frag-host/$$(echo $$f | tr / _).json $$f 2>/dev/null || true; done
	@python3 scripts/cdb.py $(CDB)/frag-ps4  $(CDB)/ps4/compile_commands.json  $(CDB_MAP)
	@python3 scripts/cdb.py $(CDB)/frag-host $(CDB)/host/compile_commands.json $(CDB_MAP)
	@rm -rf $(CDB)/frag-ps4 $(CDB)/frag-host

# Reuse identical host model/UI objects across tests and preview builds.
# Sanitized/native adapters retain their separate compile recipes below.
build/host/common/%.o: src/%.cpp $(HEADERS)
	@mkdir -p $(@D)
	g++ $(HOSTFLAGS) -c $< -o $@

build/host/cJSON.o: third_party/cjson/cJSON.c
	@mkdir -p $(@D)
	gcc -O1 -g -c $< -o $@
build/host/catalog-test: tests/catalog_test.cpp src/catalog.cpp src/metadata.cpp build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) tests/catalog_test.cpp src/catalog.cpp src/metadata.cpp build/host/cJSON.o -o $@
build/host/catalog-check: src/check.cpp src/catalog.cpp src/metadata.cpp src/settings.cpp src/archive_auth.cpp src/http.cpp build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) $(filter %.cpp %.o,$^) -lcurl -o $@

build/generated/revision.hpp:
	@mkdir -p $(@D)
	python3 -c 'import json; from pathlib import Path; r=json.load(open("revision.json"))["revision"]; Path("$@").write_text("#pragma once\n#define PSGI_REVISION %d\n#define PSGI_VERSION \"r%d\"\n" % (r,r))'

build/host/preview: $(HOST_COMMON) $(addprefix src/,$(addsuffix .cpp,$(UI_SOURCES) http desktop)) build/host/cJSON.o build/generated/revision.hpp $(HEADERS)
	g++ $(HOSTFLAGS) -Wno-unused-function $$(pkg-config --cflags freetype2 sdl2) $(filter %.cpp %.o,$^) -lcurl $$(pkg-config --libs freetype2 sdl2) -o $@
build/demo/0.png: scripts/demo_assets.py assets/font.ttf
	python3 scripts/demo_assets.py
preview: assets build/host/preview build/demo/0.png
	./build/host/preview
screenshots: assets build/host/preview build/demo/0.png
	./build/host/preview --capture
	python3 scripts/screenshots.py
	@echo 'Screenshots: build/screenshots/ — see README for interactive preview controls.'

build/host/model-test: tests/model_test.cpp tests/sfo_fixture.hpp $(HOST_COMMON) build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) $(filter %.cpp %.o,$^) -o $@
build/host/cJSON-shrink-test.o: third_party/cjson/cJSON.c
	@mkdir -p $(@D)
	gcc -O1 -g -Drealloc=psgiTestRealloc -c $< -o $@
build/host/json-allocator-test: tests/json_allocator_test.cpp src/catalog.cpp src/metadata.cpp src/userdata.cpp src/sfo.cpp src/settings.cpp build/host/cJSON-shrink-test.o $(HEADERS)
	g++ $(HOSTFLAGS) $(filter %.cpp %.o,$^) -o $@
build/host/metadata-test: tests/metadata_test.cpp tests/metadata_fixture.hpp $(HOST_COMMON) build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) $(filter %.cpp %.o,$^) -o $@
build/host/browser-native-test: tests/browser_native_test.cpp src/browser.cpp src/catalog.cpp src/metadata.cpp build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) -UPSGI_HOST -idirafter $(SDK)/include $(filter %.cpp %.o,$^) -o $@
build/host/userdata-test: tests/userdata_test.cpp $(HOST_COMMON) build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) $(filter %.cpp %.o,$^) -o $@
build/host/features-test: tests/features_test.cpp $(HOST_COMMON) build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) $(filter %.cpp %.o,$^) -o $@
build/host/browsing-test: tests/browsing_test.cpp $(HOST_COMMON) build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) $(filter %.cpp %.o,$^) -o $@
build/host/maintenance-test: tests/maintenance_test.cpp $(HOST_COMMON) build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) $(filter %.cpp %.o,$^) -o $@
build/host/filesystem-native-test: tests/filesystem_native_test.cpp src/filesystem_native.cpp $(HEADERS)
	g++ $(HOSTFLAGS) -fsanitize=address,undefined $(filter %.cpp,$^) -o $@
build/host/thread-native-test: tests/thread_native_test.cpp $(HEADERS)
	g++ $(HOSTFLAGS) -UPSGI_HOST -idirafter $(SDK)/include $(filter %.cpp,$^) -o $@
build/host/ui-test: tests/ui_test.cpp $(HOST_COMMON) build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) $(filter %.cpp %.o,$^) -o $@
build/host/artwork-test: tests/artwork_test.cpp tests/sfo_fixture.hpp tests/fixtures/screenshot.jpg src/artwork.cpp src/canvas.cpp src/catalog.cpp src/metadata.cpp src/http.cpp src/settings.cpp src/archive_auth.cpp src/installed.cpp src/sfo.cpp build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) -Wno-unused-function -fsanitize=address,undefined -fno-omit-frame-pointer $$(pkg-config --cflags freetype2) $(filter %.cpp %.o,$^) -lcurl $$(pkg-config --libs freetype2) -o $@
build/host/render-animation-test: tests/render_animation_test.cpp $(HOST_COMMON) $(addprefix src/,$(addsuffix .cpp,canvas artwork render http)) build/host/cJSON.o build/generated/revision.hpp $(HEADERS)
	g++ $(HOSTFLAGS) -Wno-unused-function $$(pkg-config --cflags freetype2) $(filter %.cpp %.o,$^) -lcurl $$(pkg-config --libs freetype2) -o $@
build/host/startup-test: tests/startup_fixtures.cpp $(HOST_COMMON) $(addprefix src/,$(addsuffix .cpp,canvas artwork render app download demo main http)) build/host/cJSON.o build/generated/revision.hpp $(HEADERS)
	g++ $(HOSTFLAGS) -Wno-unused-function $$(pkg-config --cflags freetype2) $(filter %.cpp %.o,$^) -lcurl $$(pkg-config --libs freetype2) -o $@
build/host/lifecycle-test: tests/lifecycle_test.cpp src/lifecycle.cpp $(HEADERS)
	g++ $(HOSTFLAGS) -I/opt/libjbc -idirafter $(SDK)/include $(filter %.cpp,$^) -o $@
build/host/https-trust-test: tests/https_trust_test.cpp src/https_trust.hpp assets/cacert.pem
	@mkdir -p $(@D)
	g++ $(HOSTFLAGS) $< -o $@
build/host/archive-auth-test: tests/archive_auth_test.cpp src/archive_auth.cpp src/settings.cpp src/catalog.cpp src/metadata.cpp build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) $(filter %.cpp %.o,$^) -o $@
build/host/http-native.o: src/http.cpp $(HEADERS)
	g++ $(HOSTFLAGS) -DPSGI_CA_BUNDLE_PATH='"build/host/test-cacert.pem"' -Wframe-larger-than=8192 -UPSGI_HOST -idirafter $(SDK)/include -c $< -o $@
build/host/http-native-test: tests/http_native_test.cpp build/host/http-native.o src/catalog.cpp src/metadata.cpp src/settings.cpp src/archive_auth.cpp build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) -UPSGI_HOST -idirafter $(SDK)/include $(filter %.cpp %.o,$^) -o $@
build/host/launch-native-test: tests/launch_native_test.cpp src/console.cpp src/installed.cpp src/sfo.cpp src/catalog.cpp src/metadata.cpp build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) -idirafter $(SDK)/include $(filter %.cpp %.o,$^) -o $@
build/host/uninstall-native-test: tests/uninstall_native_test.cpp src/console.cpp src/installed.cpp src/sfo.cpp src/catalog.cpp src/metadata.cpp build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) -idirafter $(SDK)/include $(filter %.cpp %.o,$^) -o $@
build/host/storage-native-test: tests/storage_native_test.cpp src/storage_native.cpp $(HEADERS)
	g++ $(HOSTFLAGS) -idirafter $(SDK)/include $(filter %.cpp,$^) -o $@
build/host/install-native-test: tests/install_native_test.cpp tests/package_fixture.hpp src/install.cpp src/library.cpp src/package.cpp src/sfo.cpp src/catalog.cpp src/metadata.cpp build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) -idirafter $(SDK)/include $(filter %.cpp %.o,$^) -o $@
build/host/download-runner: tests/download_runner.cpp src/download.cpp src/http.cpp src/catalog.cpp src/metadata.cpp src/settings.cpp src/archive_auth.cpp build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) $(filter %.cpp %.o,$^) -lcurl -o $@
build/host/app-test: tests/app_test.cpp $(HOST_COMMON) $(addprefix src/,$(addsuffix .cpp,app artwork download)) build/host/cJSON.o build/generated/revision.hpp $(HEADERS)
	g++ $(HOSTFLAGS) -Wno-unused-function $$(pkg-config --cflags freetype2) $(filter %.cpp %.o,$^) -o $@
build/host/controller-test: tests/controller_test.cpp src/controller.cpp src/video.cpp $(HOST_COMMON) build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) -idirafter $(SDK)/include $$(pkg-config --cflags freetype2 sdl2) $(filter %.cpp %.o,$^) $$(pkg-config --libs sdl2) -o $@
test: assets build/host/archive-auth-test build/host/https-trust-test build/host/json-allocator-test build/host/browser-native-test build/host/metadata-test build/host/catalog-test build/host/catalog-check build/host/model-test build/host/userdata-test build/host/features-test build/host/maintenance-test build/host/browsing-test build/host/filesystem-native-test build/host/thread-native-test build/host/ui-test build/host/artwork-test build/host/render-animation-test build/host/startup-test build/host/lifecycle-test build/host/http-native-test build/host/launch-native-test build/host/uninstall-native-test build/host/storage-native-test build/host/install-native-test build/host/download-runner build/host/app-test build/host/controller-test
	./build/host/archive-auth-test
	./build/host/https-trust-test
	./build/host/json-allocator-test
	./build/host/browser-native-test
	./build/host/metadata-test
	./build/host/catalog-test
	./build/host/model-test
	./build/host/userdata-test
	./build/host/features-test
	./build/host/maintenance-test
	./build/host/browsing-test
	./build/host/filesystem-native-test
	./build/host/thread-native-test
	./build/host/ui-test
	./build/host/artwork-test
	./build/host/render-animation-test
	./build/host/http-native-test
	./build/host/launch-native-test
	./build/host/uninstall-native-test
	./build/host/storage-native-test
	./build/host/install-native-test
	./build/host/app-test
	./build/host/controller-test
	python3 tests/http_test.py ./build/host/catalog-check
	python3 -m unittest discover -s tests -p '*_test.py'
check: build/host/catalog-check
	@test -n "$(CONFIG)" || { echo 'Use make check CONFIG=/work/config.json'; exit 1; }
	./build/host/catalog-check "$(CONFIG)"
validate:
	python3 scripts/package.py --validate
	python3 scripts/validate_imports.py build/app.oelf
clean:
	rm -rf build dist
-include $(wildcard build/ps4/*.d)
endif
