# PS4 Games Installer — Development manual

Build, test and package the native C++17 OpenOrbis application on Linux.
[README.md](README.md) covers normal use; [EXPERT.md](EXPERT.md) covers catalog
hosting, advanced settings, storage, data files and troubleshooting.

## Repository layout

| Location | Purpose |
| --- | --- |
| [src/](src/) | Native application, UI, renderer, catalog and PS4 service adapters |
| [assets/](assets/) | Editable icon SVG, packaged PNG, font and notices |
| [third_party/](third_party/) | Pinned vendored libraries and their licenses |
| [tests/](tests/) | Host regressions, native service fakes and Python integrations |
| [scripts/](scripts/) | Packaging, metadata, import validation, deployment and capture helpers |
| [Makefile](Makefile) / [Dockerfile](Dockerfile) | Build entry point and managed toolchain |
| [Screenshots/](Screenshots/) | Console captures used by the consumer README |
| [docs/images/](docs/images/) | Desktop preview images used in the expert manual |
| `build/`, `dist/` | Ignored generated files and release packages |

## Build on Linux

Install Docker and GNU Make on the host; the compiler, SDK, publishing tools, font
and image utilities are managed by the Docker build.

```sh
make image
make
make test
make validate
```

Output: `dist/PS4Game_installer-rN.pkg`, plus a JSON manifest with revision, input
fingerprint and SHA-256. The build uses LLVM 18, checksum-verified OpenOrbis 0.5.4,
pinned libjbc and a pinned LibOrbisPkg publisher built with .NET 8. Unity and Sony's
SDK are not used.

A successful build with changed inputs increments the revision. No-change builds
reuse the package. Failed builds do not consume a revision, concurrent builds are
serialized, and `make clean` retains the committed revision record. Rebuilding a
clean checkout with identical inputs retains that checkout's recorded revision.
Commit `revision.json` along with a release's source changes.

To validate your catalog from the build container without installing anything:

```sh
# Edit config.example.json with your address first.
make check CONFIG=/work/config.example.json
```

The Linux check shares parsing with the console but uses libcurl; it cannot validate
the PS4's TLS support, installed-game services or BGFT behavior.

### Deploy to your PS4

Enable the PS4's FTP server, then configure your target once:

```sh
cp .env.example .env
# Edit .env with the PS4 address, FTP credentials and destination directory.
make deploy
```

```dotenv
DEPLOY_HOST=192.168.1.50
DEPLOY_PORT=2121
DEPLOY_USER=anonymous
DEPLOY_PASS=anonymous
DEPLOY_PATH=/data/pkg
```

`make deploy` reads the repository's `.env`, builds the current revision using
Docker, verifies its SHA-256 manifest, and uploads `PS4Game_installer-rN.pkg` by
passive FTP. Create `DEPLOY_PATH` on the PS4 beforehand. The server must support
file upload and rename; uploads use a temporary name until the transfer completes.
The remote size is checked when the server supports `SIZE`. Install the uploaded
package using your PS4 package installer.

`.env` is ignored by Git and excluded from Docker build contexts. Deployment reads
it through the container's repository mount; credentials are not passed on command
lines or printed. Values may be single/double quoted, and comments are supported.
Shell commands and variable substitutions in `.env` remain literal text. Plain FTP
is intended for your trusted local network.

### Desktop preview

```sh
make preview       # Interactive X11 window when DISPLAY is available; otherwise screenshots.
make screenshots   # Always generate build/screenshots/*.png without a display.
```

Interactive preview uses the same UI and renderer with a sample library and disables
PS4 launch/install operations. It forwards your X11 socket and XAUTHORITY when set;
ensure those environment variables identify your current desktop session.
Keyboard controls: arrows = D-pad, Enter/X = Cross, Escape/O = Circle, S = Square,
T = Triangle, Tab = Options, Q/E = L1/R1, Page Up/Page Down = L2/R2.
Enter accepts text; Escape cancels it.
An SDL-compatible controller can also navigate the preview.

## Testing changes

Run `make test` for the host and native-adapter suites plus Python integrations.
Native-adapter tests compile against the pinned SDK and fake PS4 service calls;
they check parameters and error handling without running services on a console.
The App tests exercise settings, queue/session persistence, cancellation, launch
handoff ordering and recovery. HTTP integrations use local fixture servers.

For a targeted C++ test, invoke the container-owned Make target rather than
requiring its SDK and libraries on the host:

```sh
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/work" \
  -e SDL_VIDEODRIVER=dummy ps4-games-installer:build \
  make build/host/features-test

docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/work" \
  -e SDL_VIDEODRIVER=dummy ps4-games-installer:build \
  ./build/host/features-test
```

Keep production sources and generated revision headers stable while compilers run.
If inputs change during a build, rebuild the affected targets before relying on
its results. Run the relevant tests, then build and validate the native package.
For UI changes, also generate screenshots and inspect the changed screens.

## PS4 service integration

Installed discovery combines registered Title IDs with local app metadata and
icons. Keep UI/catalog code separate from the native service adapters. Console
data belongs under `/data/PS4-Game-Installer/`; see the
[expert data-file reference](EXPERT.md#data-and-troubleshooting).

The launcher imports `libSceLncUtil` packed inside SystemService. Dynamic lookup
through the module's default library cannot resolve every packed export.
`scripts/validate_imports.py` checks the generated executable during packaging.
Launch parameters use the foreground user, and the App saves the browsing session
before handing control to the PS4. A failed native call preserves the app and
rolls back the successful-launch record.

Downloader/native installation behavior is covered by fake-service tests and
[download recovery notes](EXPERT.md#downloads-and-offline-use). Preserve task
ownership and staged-package leases when a service cannot confirm release. A
completed file transfer is separate from installation confirmation. Local Title ID
records must remain independent of remote catalog changes.

The shell exit path restores the original homebrew credentials and requests exit
through SystemService. Returning through the ordinary C runtime caused a console
exit crash in earlier revisions; retain lifecycle regression coverage.

## Hardware verification

The user confirmed the application runs on **PS4 11.00 + GoldHEN**, with working
source entry/settings saving, artwork, complete installed discovery, analog
navigation, game launching, installation after r28, uninstall and the exit-crash
fix. These confirmations do not establish acceptance of every later change.

r33 introduced a reported console crash when bypassing the launch-close prompt;
r34 restores the r32 native launch call and its normal confirmation. The user
confirmed launching works after this rollback (2026-10-17). Keep the
skip-launch-check flag disabled. Remaining session and verification checks are
tracked in [TODO.md](TODO.md). A validated PKG,
host tests and desktop captures do not establish PS4 service behavior, USB behavior
or console frame rate. Record the exact revision, firmware and observed result
when testing on hardware; keep historical failures separate from current evidence.

Use **Options > Export diagnostics to USB** for a shareable report. Raw `app.log`,
settings and catalogs can contain private URLs or other sensitive information;
inspect them locally and sanitize evidence before including it in a public issue.
Never commit `.env`, private console snapshots, logs or downloaded packages.

## Release workflow

1. Run the relevant host tests and compile the PS4 application with `make`.
2. Run `make validate`; inspect the PKG metadata, import checks and manifest.
3. For UI changes, run `make screenshots` and inspect the affected captures.
4. Update [CHANGELOG.md](CHANGELOG.md), [PROGRESS.md](PROGRESS.md) and
   [TODO.md](TODO.md) with actual results and remaining console acceptance checks.
5. Commit the successful `revision.json` alongside the corresponding source.
   Push or deploy only when requested.

Documentation-only edits do not change packaged inputs and do not need a new
application revision. Validate Markdown links and image references instead of
rebuilding an unchanged application. README images use linked `<img width="30%">`
elements in Markdown so the pictures stay compact and open at full size.
Preserve the supplied console captures in `Screenshots/`; generated desktop
samples remain under `docs/images/` and must be described as preview images.

Edit `assets/icon.svg` when changing the icon, then regenerate the package PNG
through Make. Keep dependencies pinned and retain their licenses. The project
workflow is recorded in [AGENTS.md](AGENTS.md).

## License and credits

Created by **e1z0 — (c) 2026 Vintage2000.org**. Application source is GPLv2;
see [LICENSE](LICENSE) and [THIRD_PARTY.md](THIRD_PARTY.md). Component versions and
licenses belong in the credits file rather than the consumer guide.

## Embedded metadata and details changes

`src/metadata.cpp` reads the companion catalog's IGDB tree and resolves display
fallbacks without changing personal records. `Entry::igdbJson` retains the object
for offline round trips. Package classification uses explicit `package_type`
before legacy source categories, including saved queue entries.

`src/ui_details.cpp` owns structured rows, collapsible sections, media navigation
and dynamic genre sidebar rows. `src/browser.cpp` is a separate PS4 browser ABI
adapter; it does not change native game-launch flags. Host metadata and browser
ABI tests cover malformed fields/URLs, overrides, cache/session persistence and
package routing. Desktop captures 85–92 exercise the new layouts. Real console
acceptance for gallery, browser/provider playback and mixed-feed add-ons remains
required; host mocks do not establish these behaviors.

## Large JSON allocator regression (r37)

A read-only GoldHEN kernel-log capture identified r36's worker SIGSEGV in the
linked `memcpy` at runtime PC 0x511483 (ELF offset 0x111483, module base 0x400000).
The SDK allocator's failed-mremap fallback allocates the requested smaller block
and copies the former block's full size. cJSON's ordinary Print functions finish
by shrinking their output, which exposes that path for rich catalog JSON.

Use `src/json.hpp` / `printJson` for all production JSON output. It calls cJSON's
buffered printer, which grows buffers without performing a final shrink. It uses
no process-wide allocator-hook changes and leaves pinned dependencies intact.
The dedicated allocator regression builds cJSON with a shim rejecting large
shrinks, reproduces the ordinary printer failure, then round-trips large catalogs
and personal records through the buffered policy. Do not replace this helper with
`cJSON_Print` or `cJSON_PrintUnformatted`.

The same saved PS4 catalog/session/personal state passed an ASan/UBSan host replay;
that did not reproduce the console allocator behavior. Kernel/disassembly evidence
and the emulated shrink-path regression provide the platform-specific coverage.
Actual PS4 startup/refresh acceptance still requires installing the new package.

Host model/UI objects are cached under `build/host/common/` and reused by ordinary
tests and the preview. Sanitizer builds and native adapters keep separate compile
recipes, so their instrumentation/SDK flags are preserved.

## Screenshot transport and diagnostics (r38)

Artwork downloads use `httpGetArtwork`; catalog/package requests use their original
clients. Only PS4 artwork URLs starting with the exact public IGDB image origin
and upload path use a certificate-acceptance callback. This follows the ABI and
success convention in the pinned SDK's
[net_http sample](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/samples/net_http/net_http/main.c).
It relaxes certificate validation for those public images. Prefix tests exclude
lookalike hosts, credentials and unrelated paths; catalogs/packages never activate
the exception, including when pointed at that same host. Redirects now use the
bounded policy below and re-evaluate the exception on each hop.

The artwork worker keeps bounded failure text per key and at most 32 diagnostic
messages for the main thread to write to app.log. Screenshot successes include
decoded dimensions. Triangle refresh clears the image's failure and disk entry;
generation checks discard stale in-flight results. Host regressions cover the
native callback ABI/isolation, progressive JPEG decoding and error/retry recovery.
Capture 96 exercises a real refused local connection rather than a hardcoded error.

Read-only r37 console evidence showed no screenshot cache entries, but no native
image error had been logged. HTTPS compatibility addresses a likely cause; actual
PS4 acceptance is required to establish that it resolves this console's failure.

## HTTPS trust and redirect transport

`prepareHttpTrust()` parses the packaged, pinned Mozilla CA bundle before
`prepareConsole()` changes the filesystem root with libjbc. The cached certificates
remain available after `/app0` becomes inaccessible. Native HTTP initialization
registers full CRLF PEM records using `sceHttpsLoadCert` on the same module/context
as every other HTTP operation. The pointer array, PEM records and strings remain
alive until context teardown. Initialization failures clean up HTTP, SSL and pool
resources and allow a later retry. Certificates are never fetched from a server
or silently accepted when verification fails.

Both native and host clients use the same bounded redirect URL/header policy:
five hops, HTTP(S) only, no HTTPS downgrade, no URL credentials, and no redirect
bodies delivered to parsers or download sinks. Relative locations retain escaped
paths and query strings. Catalog size/cancellation/deadline limits still apply.
The host checker honors `CURL_CA_BUNDLE` for local TLS fixtures; the console has
no runtime trust override. `THIRD_PARTY.md` records the bundle pin and license.

`HttpTransferInfo::effectiveUrl` identifies the final response. Only resolved HTTP
range responses use remote BGFT registration. HTTPS packages reuse the existing
bounded-memory staging, free-space checks, lease handling and local BGFT path,
because BGFT has an independent trust store. Original catalog URLs remain queue
and staging-cache keys. HTTPS probes avoid Range so cache validation sees the full
length, including when an HTTP probe first redirects to HTTPS.

Native fixtures verify CA registration ABI/lifetime/failure cleanup and redirect
policy. Local TLS integration tests verify trust/hostname failures and download
routing without external package servers. These checks and PS4 package validation
do not prove real-console TLS/cipher or BGFT acceptance. Test the resulting package
on PS4 11.00 + GoldHEN, including HTTPS catalogs, redirected HTTPS packages, local
installation, cancellation/retry and the existing direct HTTP path.

### PS4 9.00 follow-up

A user tested r42 on firmware 9.00: artwork appeared, but the same package request
still failed with `0x8095F00C`. Artwork can use the existing IGDB exception and is
not evidence that verified package TLS succeeded. Desktop TLS 1.2 handshakes with
the packaged roots verify the reported host and its CDN, including the supplied
GoDaddy G2 intermediate/cross-signed chain. This does not reproduce firmware path
building or prove why the console rejects it.

The native client now requests keepalive when creating each connection. SSPI's
[initial public native client](https://github.com/Xyhlo/SSPI/blob/4984f2a1fd3a45f83c58c4f4c3dcb1d8b90cd439/app/NativeHttp.cs#L1141)
documents loss of custom HTTPS trust in the non-keepalive path and uses this mode.
This is a focused compatibility candidate, not a confirmed diagnosis on this PS4.
Connections still close through the existing per-request/per-hop RAII ownership.
The root bundle, certificate verification and redirect policies are unchanged.

The native fixture models that documented trust-loss behavior and fails before
the connection-mode change. It validates the intended API contract; it does not
emulate PS4 cryptography. Send failures include only the host and redirect count,
never signed paths/query strings. The existing error logger also records native
SSL/verification flags, errno, CA count/load result and connection mode. Diagnostic
getters run before cleanup; a failed getter cannot replace the request error.
Unknown-CA failures are described as certificate
trust failures. The installation-error dialog no longer claims that a package was
downloaded. Firmware 9.00 retesting is needed before considering the TLS issue
resolved; investigate chain construction separately if the same request fails.

### GoDaddy G2 compatibility anchor

The r44 firmware 9.00 log identifies `archive.org`, redirect 0, unknown CA
(`send=0x8095F00C`, `ssl=0x8095F00A`, `verify=0x20`) with `ca=121`, successful
`ca_load=0` and `keepalive=1`. Keepalive alone did not resolve this console's error.
The served chain includes the G2 issuing intermediate, a cross-signed G2 root and
a legacy Class2 root. The packaged Mozilla store contains the self-signed G2 root;
modern OpenSSL builds a valid alternate path, but this does not prove that older
PS4 firmware makes the same selection.

The app now preloads the exact GoDaddy-published G2 issuing certificate alongside
the unchanged Mozilla roots, before the filesystem-root transition. SSPI documents
this compatibility anchor for the same cross-signed chain. The asset tests pin its
PEM and DER hashes, verify its CA/signing constraints and its signature against the
bundled roots, and check package inclusion. `THIRD_PARTY.md` records provenance.
The firmware can use the issuing CA directly while retaining its leaf certificate
checks. No server-delivered certificate is added to trust and no verification
callback is used for catalogs/packages.

This promotes the intermediate to a trust anchor: ancestor-chain checks above it
are no longer needed. It is a public issuing CA, not restricted to one hostname;
its root was already trusted. Review/replace this packaged compatibility anchor
before its May 3, 2031 expiry. A successful console test establishes compatibility
for that request, not the precise internal firmware chain-selection algorithm.

Hardware result: on the user's firmware 9.00 console, the same package request
that failed with unknown CA on r44 reached HTTP 401 on r45. A desktop probe follows
the HTTPS redirect and reproduces the CDN's authorization response; another file
returns HTTP 500. This confirms the certificate compatibility change for that
request. Complete package transfer/local installation still needs an accessible
file host and is not established by these error responses.

### Internet Archive authentication

The user reproduced a successful download only after signing into Archive.org.
Anonymous full/ranged GETs with installer and browser headers returned HTTP 401.
The client now uses the official account login endpoint documented by the
[Internet Archive client](https://github.com/jjjake/internetarchive/blob/master/internetarchive/config.py):
form-encoded email/password are sent to `https://archive.org/services/xauthn/?op=login`.
Login redirects are rejected; the reply is bounded and server error text is never
shown or logged. Only the two login cookies are retained; returned S3 keys are ignored.

The existing background worker owns sign-in. Its result becomes active only after
the separate session file is saved; failed replacement login preserves the prior
session. Each download captures a session snapshot and uses it for the header
probe and full staged download. The existing HTTP adapters attach cookies only
after checking every hop's HTTPS scheme and exact Archive.org domain boundary.
HTTP, unrelated hosts and nonstandard ports never receive the credentials.
Catalog/artwork fetches and remote BGFT registration do not receive the session.
The existing staging/local BGFT path and public catalog format are unchanged.

The account screen masks password entry, disables IME learning/resume for that
field, and clears temporary password buffers after submission/cancel. Sessions
use a separate mode-0600 file, excluded from existing backup/diagnostic exports.
Sign-out requires the current worker operation to finish and deletes the local
session. This is local sign-out, not revocation of all Archive.org sessions.
There is no authentication bypass, automatic retry on rejected credentials,
new networking dependency, or claim that host download speed is improved.
