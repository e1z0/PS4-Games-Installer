# Credits and licenses

Application source is distributed under GPLv2; see LICENSE.

- OpenOrbis PS4 Toolchain 0.5.4: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain
- OpenOrbis LibOrbisPkg (commit pinned in Dockerfile): https://github.com/OpenOrbis/LibOrbisPkg
- ps4-libjbc (commit pinned in Dockerfile): https://github.com/bucanero/ps4-libjbc
- cJSON 1.7.19: MIT, license in `third_party/cjson/LICENSE`.
- stb_image (commit 2c980bb59875b0d32144a71867fbdebb2f77cd20): public domain/MIT, `third_party/stb/LICENSE`.
- Noto Sans: SIL Open Font License, `assets/FONT-LICENSE.txt`.
- Native prototype and BGFT ABI reference: https://github.com/ItsJokerZz/UnityOrbisBridge (GPLv2).
- Installation-location registry research reference: https://github.com/ReSo7200/DirectPackageInstaller-Enhanced/blob/DN8/Payload/main_experimental.c
- Packed launcher import mapping: https://github.com/OpenOrbis/create-fself/blob/master/pkg/oelf/OELFStrangeLibs.go
- BGFT task stop/unregister reference: https://github.com/flatz/ps4_remote_pkg_installer/blob/master/installer.c
- Mozilla CA certificate store, curl conversion dated 2026-09-25: MPL 2.0,
  `assets/cacert.pem` and `assets/cacert-LICENSE.txt`.
  Source: https://curl.se/ca/cacert-2026-09-25.pem (121 certificates).
  SHA-256: `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`.
  Provenance and conversion license: https://curl.se/docs/caextract.html.
  License text: https://www.mozilla.org/media/MPL/2.0/index.txt.
  Update the dated source, checksum, and certificate-count test together when
  replacing this bundle. Updates ship with application packages; the application
  does not download trust anchors or accept server-provided certificates as roots.
- GoDaddy Secure Certificate Authority - G2 public intermediate certificate:
  `assets/godaddy-secure-g2.pem`, downloaded unchanged on 2026-10-19 from
  https://certs.godaddy.com/repository/gdig2.crt.pem.
  GoDaddy publishes the certificate and DER fingerprint in its repository,
  https://certs.godaddy.com/repository/. CP/CPS section 9.5.1 permits complete
  certificate reproduction and distribution, subject to the Relying Party
  Agreement: https://github.com/godaddy/cp-cps/blob/main/cpcps/TLS_CPCPS.md.
  This public certificate data is separate from the MPL-licensed Mozilla
  conversion; no third-party source code or CP/CPS text is included in the PEM.
  DER SHA-256: `973a41276ffd01e027a2aad49e34c37846d3e976ff6a620b6712e33832041aa6`.
  PEM SHA-256: `708c1477f5e2b2ae402ba1020a696cdd0051d44859a7e1423c900826d09053f4`.
  The signature verifies against the GoDaddy G2 root already in `cacert.pem`.
  This exact intermediate was also served by archive.org during investigation.
  Validity ends 2031-05-03 07:00:00 UTC; review and replace or remove it before
  expiry, updating both pins and tests together. Its critical constraints are
  CA:true and certificate/CRL signing; it has no path-length, name, or extended
  key-usage constraint. Loading it as a compatibility trust anchor trusts this
  issuer for all hostnames, not only archive.org, and shortens the verified path
  by bypassing ancestor checks. Hostname, validity, and signature verification
  remain enabled. The root bundle stays unchanged. The pinned SSPI reference
  below uses the same intermediate to avoid legacy firmware choosing the
  server's cross-signed root path; that internal cause is a hypothesis until
  verified on hardware. No third-party implementation code is copied.
- Native HTTPS certificate ABI and PEM format research reference:
  https://github.com/Xyhlo/SSPI/blob/dc6cb98ee0b1c68d67bfee581fcbcb241246f456/app/NativeHttp.cs.
  OpenOrbis 0.5.4 declares `SslPem` and `sceHttpsLoadCert`; the reference confirms
  an array of pointers to PEM records, with CRLF line endings and retained storage.
  The implementation here uses the existing OpenOrbis declarations and a separate
  small parser; no code is copied from that reference.

The new application icon and SVG artwork were created for this project.
