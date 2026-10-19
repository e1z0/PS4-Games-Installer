"""Keep the reviewed trust bundle and its license in the produced package."""
import hashlib
import os
from pathlib import Path
import runpy
import ssl
import subprocess
import tempfile
import unittest
from unittest.mock import patch

PROJECT = Path(__file__).resolve().parents[1]
BUNDLE_SHA256 = "a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505"
GODADDY_PEM_SHA256 = "708c1477f5e2b2ae402ba1020a696cdd0051d44859a7e1423c900826d09053f4"
GODADDY_DER_SHA256 = "973a41276ffd01e027a2aad49e34c37846d3e976ff6a620b6712e33832041aa6"


class HttpsTrustPackage(unittest.TestCase):
    def test_reviewed_bundle_and_license(self):
        bundle = (PROJECT / "assets/cacert.pem").read_bytes()
        self.assertEqual(hashlib.sha256(bundle).hexdigest(), BUNDLE_SHA256)
        self.assertEqual(bundle.count(b"-----BEGIN CERTIFICATE-----"), 121)
        self.assertEqual(bundle.count(b"-----END CERTIFICATE-----"), 121)
        license_text = (PROJECT / "assets/cacert-LICENSE.txt").read_text()
        self.assertIn("Mozilla Public License Version 2.0", license_text)
        self.assertIn(BUNDLE_SHA256, (PROJECT / "THIRD_PARTY.md").read_text())

    def test_reviewed_godaddy_intermediate(self):
        certificate = PROJECT / "assets/godaddy-secure-g2.pem"
        pem = certificate.read_bytes()
        self.assertEqual(hashlib.sha256(pem).hexdigest(), GODADDY_PEM_SHA256)
        self.assertEqual(pem.count(b"-----BEGIN CERTIFICATE-----"), 1)
        self.assertEqual(pem.count(b"-----END CERTIFICATE-----"), 1)
        der = ssl.PEM_cert_to_DER_cert(pem.decode("ascii"))
        self.assertEqual(hashlib.sha256(der).hexdigest(), GODADDY_DER_SHA256)
        credits = (PROJECT / "THIRD_PARTY.md").read_text()
        self.assertIn(GODADDY_PEM_SHA256, credits)
        self.assertIn(GODADDY_DER_SHA256, credits)
        details = subprocess.run(
            ["openssl", "x509", "-in", str(certificate), "-noout", "-text", "-dates"],
            check=True, capture_output=True, text=True).stdout
        self.assertRegex(details, r"X509v3 Basic Constraints: critical\s+CA:TRUE\s*\n")
        self.assertRegex(details, r"X509v3 Key Usage: critical\s+Certificate Sign, CRL Sign")
        # The published intermediate has no path-length or domain restriction.
        self.assertNotIn("pathlen", details.lower())
        self.assertNotIn("X509v3 Name Constraints", details)
        self.assertNotIn("X509v3 Extended Key Usage", details)
        self.assertRegex(details, r"notBefore=May\s+3 07:00:00 2011 GMT")
        self.assertRegex(details, r"notAfter=May\s+3 07:00:00 2031 GMT")
        # Verify its signature and current validity against the existing roots.
        subprocess.run(["openssl", "verify", "-trusted", str(PROJECT / "assets/cacert.pem"),
                        str(certificate)],
                       check=True, capture_output=True, text=True)

    def test_metadata_stages_bundle_and_license(self):
        original = Path.cwd()
        with tempfile.TemporaryDirectory() as temporary:
            try:
                os.chdir(temporary)
                with patch("sys.argv", ["metadata.py", "/fixture-sdk", "1", "CONTENT", "TITLE"]), \
                     patch("shutil.copyfile") as copy_file, patch("subprocess.run") as run:
                    runpy.run_path(str(PROJECT / "scripts/metadata.py"), run_name="__main__")
                for name in ("assets/cacert.pem", "assets/cacert-LICENSE.txt",
                             "assets/godaddy-secure-g2.pem"):
                    copy_file.assert_any_call(Path(name), Path("build/pkg") / name)
                    package_files = run.call_args_list[-1].args[0][-1].split()
                    self.assertIn(name, package_files)
            finally:
                os.chdir(original)


if __name__ == "__main__":
    unittest.main()
