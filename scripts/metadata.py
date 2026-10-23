"""Stage package resources and create metadata using OpenOrbis publishing tools."""
from pathlib import Path
import shutil
import subprocess
import sys

sdk, revision, content_id, title_id = sys.argv[1:]
sdk = Path(sdk)
pkg = Path("build/pkg")
tool = str(sdk / "bin/linux/PkgTool.Core")
files = {
    "assets/font.ttf": Path("assets/font.ttf"),
    "assets/icon.png": Path("assets/icon.png"),
    "assets/FONT-LICENSE.txt": Path("assets/FONT-LICENSE.txt"),
    "assets/cacert.pem": Path("assets/cacert.pem"),
    "assets/cacert-LICENSE.txt": Path("assets/cacert-LICENSE.txt"),
    "assets/godaddy-secure-g2.pem": Path("assets/godaddy-secure-g2.pem"),
    "assets/letsencrypt-gen-y.pem": Path("assets/letsencrypt-gen-y.pem"),
    "assets/LICENSE.txt": Path("LICENSE"),
    "assets/cJSON-LICENSE.txt": Path("third_party/cjson/LICENSE"),
    "assets/stb-LICENSE.txt": Path("third_party/stb/LICENSE"),
    "assets/THIRD_PARTY.txt": Path("THIRD_PARTY.md"),
    "sce_sys/icon0.png": Path("assets/icon.png"),
    "sce_sys/about/right.sprx": sdk / "samples/hello_world/sce_sys/about/right.sprx",
    "sce_module/libc.prx": sdk / "samples/hello_world/sce_module/libc.prx",
    "sce_module/libSceFios2.prx": sdk / "samples/hello_world/sce_module/libSceFios2.prx",
}
for target, source in files.items():
    dest = pkg / target
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, dest)
sfo = str(pkg / "sce_sys/param.sfo")
subprocess.run([tool, "sfo_new", sfo], check=True)
version = f"{int(revision) // 100:02d}.{int(revision) % 100:02d}"
for name, value, kind, size in [
    ("APP_TYPE", "1", "Integer", 4), ("APP_VER", version, "Utf8", 8),
    # Common dialogs use Circle when the Cross-assignment flag is absent.
    # Keep the native IME consistent with the application's X confirm / O cancel.
    ("ATTRIBUTE", "2", "Integer", 4), ("ATTRIBUTE2", "2", "Integer", 4),
    ("CATEGORY", "gd", "Utf8", 4),
    ("CONTENT_ID", content_id, "Utf8", 48), ("DOWNLOAD_DATA_SIZE", "0", "Integer", 4),
    ("SYSTEM_VER", "0", "Integer", 4), ("TITLE", "PS4 Games Installer", "Utf8", 128),
    ("TITLE_ID", title_id, "Utf8", 12), ("VERSION", version, "Utf8", 8),
]:
    subprocess.run([tool, "sfo_setentry", sfo, name, "--type", kind,
                    "--maxsize", str(size), "--value", value], check=True)
Path("build/package").mkdir(exist_ok=True)
subprocess.run([str(sdk / "bin/linux/create-gp4"), "-out", "pkg.gp4",
                "--content-id=" + content_id, "--files",
                " ".join(["eboot.bin", "sce_sys/param.sfo", *files.keys()])], cwd=pkg, check=True)
