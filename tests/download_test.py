"""Exercise real HTTP streaming and staging through libcurl, including bad servers."""
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import os
import ssl
import subprocess
import tempfile
import threading
import unittest

RUNNER = Path("build/host/download-runner")
CHECKER = Path("build/host/catalog-check")
BODY = b"\x7fCNT" + bytes(i % 251 for i in range(4, 1024 * 1024 + 4))


class DownloadTests(unittest.TestCase):
    def test_redirects_and_verified_https_staging(self):
        if not RUNNER.exists() or not CHECKER.exists():
            self.skipTest("build the download runner and catalog checker with make test")
        calls = Counter()
        suffix = "/A%20Game%20%5BUS%5D.pkg?token=kept"

        with tempfile.TemporaryDirectory(prefix="psgi-tls-") as temporary:
            certs = Path(temporary)
            commands = [
                ["req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                 "-subj", "/CN=PSGI fixture CA", "-addext", "basicConstraints=critical,CA:TRUE",
                 "-keyout", "ca.key", "-out", "ca.pem"],
                ["req", "-new", "-newkey", "rsa:2048", "-nodes", "-subj", "/CN=localhost",
                 "-keyout", "server.key", "-out", "server.csr"],
                ["x509", "-req", "-in", "server.csr", "-CA", "ca.pem", "-CAkey", "ca.key",
                 "-CAcreateserial", "-days", "1", "-extfile", "server.ext", "-out", "server.pem"],
            ]
            (certs / "server.ext").write_text(
                "subjectAltName=DNS:localhost\nbasicConstraints=critical,CA:FALSE\n"
                "keyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\n")
            for arguments in commands:
                subprocess.run(["openssl", *arguments], cwd=certs, check=True, capture_output=True)

            class Handler(BaseHTTPRequestHandler):
                def log_message(self, *_args):
                    pass

                def do_HEAD(self):
                    raise AssertionError("Package probes must use GET")

                def do_GET(self):
                    ranged = self.headers.get("Range") == "bytes=0-3"
                    calls[self.server.secure, self.path, ranged] += 1
                    if self.path == "/catalog-upgrade":
                        self.send_response(302)
                        self.send_header("Location", secure_url + "/catalog")
                        self.end_headers()
                        return
                    if self.path == "/catalog":
                        data = b'{"DATA":{"http://fixture/package.pkg":{"name":"Test package"}}}'
                        self.send_response(200)
                        self.send_header("Content-Length", str(len(data)))
                        self.end_headers()
                        self.wfile.write(data)
                        return
                    if self.path.startswith("/redirect/"):
                        self.send_response(302)
                        self.send_header("Location", "/range" + suffix)
                        self.end_headers()
                        return
                    if self.path.startswith("/upgrade/"):
                        self.send_response(302)
                        self.send_header("Location", secure_url + "/upgraded" + suffix)
                        self.end_headers()
                        return
                    data = BODY[:4] if ranged else BODY
                    self.send_response(206 if ranged else 200)
                    self.send_header("Content-Length", str(len(data)))
                    self.send_header("Accept-Ranges", "bytes")
                    if ranged:
                        self.send_header("Content-Range", f"bytes 0-3/{len(BODY)}")
                    self.end_headers()
                    try:
                        for pos in range(0, len(data), 8192):
                            self.wfile.write(data[pos:pos + 8192])
                    except (ConnectionError, OSError):
                        pass  # Header probes intentionally stop after four bytes.

            plain = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
            plain.secure = False
            secure = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
            secure.secure = True
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(certs / "server.pem", certs / "server.key")
            secure.socket = context.wrap_socket(secure.socket, server_side=True)
            plain_url = f"http://127.0.0.1:{plain.server_port}"
            secure_url = f"https://localhost:{secure.server_port}"
            servers = (plain, secure)
            threads = [threading.Thread(target=server.serve_forever, daemon=True) for server in servers]
            for thread in threads:
                thread.start()
            try:
                cases = [
                    (plain_url + "/redirect" + suffix, "redirect", plain_url + "/range" + suffix, True),
                    (secure_url + "/https-range" + suffix, "https-range", None, True),
                    (plain_url + "/upgrade" + suffix, "upgrade", None, True),
                    (secure_url + "/untrusted" + suffix, "tls-error", None, False),
                    (secure_url.replace("localhost", "127.0.0.1") + "/wrong-name" + suffix,
                     "tls-error", None, True),
                ]
                for url, mode, expected_remote, trusted in cases:
                    with self.subTest(mode=mode, url=url), tempfile.TemporaryDirectory(prefix="psgi-download-") as directory:
                        env = os.environ.copy()
                        env.pop("CURL_CA_BUNDLE", None)
                        if trusted:
                            env["CURL_CA_BUNDLE"] = str(certs / "ca.pem")
                        arguments = [str(RUNNER), url, directory, mode]
                        if expected_remote:
                            arguments.append(expected_remote)
                        result = subprocess.run(arguments, env=env, capture_output=True, text=True, timeout=15)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                config = certs / "catalog-config.json"
                for url, trusted, succeeds in [
                    (secure_url + "/catalog", True, True),
                    (plain_url + "/catalog-upgrade", True, True),
                    (secure_url + "/catalog", False, False),
                    (secure_url.replace("localhost", "127.0.0.1") + "/catalog", True, False),
                ]:
                    with self.subTest(catalog=url, trusted=trusted):
                        config.write_text(json.dumps({"content_urls": {"games": url}}))
                        env = os.environ.copy()
                        env.pop("CURL_CA_BUNDLE", None)
                        if trusted:
                            env["CURL_CA_BUNDLE"] = str(certs / "ca.pem")
                        result = subprocess.run([str(CHECKER), str(config)], env=env,
                                                capture_output=True, text=True, timeout=15)
                        self.assertEqual(result.returncode, 0 if succeeds else 1, result.stdout + result.stderr)
                        expected = "1 unique packages" if succeeds else "certificate"
                        self.assertIn(expected, (result.stdout + result.stderr).lower())
                self.assertEqual(calls[False, "/redirect" + suffix, True], 1)
                self.assertEqual(calls[False, "/range" + suffix, True], 1)
                self.assertEqual(calls[False, "/range" + suffix, False], 0)
                # Two non-Range probes plus one complete download, even though the
                # HTTPS server supports Range. The second installation reuses disk.
                self.assertEqual(calls[True, "/https-range" + suffix, False], 3)
                self.assertEqual(calls[True, "/https-range" + suffix, True], 0)
                self.assertEqual(calls[False, "/upgrade" + suffix, True], 2)
                self.assertEqual(calls[True, "/upgraded" + suffix, True], 2)
                self.assertEqual(calls[True, "/upgraded" + suffix, False], 3)
            finally:
                for server in servers:
                    server.shutdown()
                    server.server_close()
                for thread in threads:
                    thread.join()

    def test_package_routes_and_fallback(self):
        if not RUNNER.exists():
            self.skipTest("build the download runner with make test")
        calls = Counter()

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_args):
                pass

            def do_HEAD(self):
                raise AssertionError("A package probe must not depend on HEAD support")

            def do_GET(self):
                mode = self.path.split("/")[1]
                ranged = bool(self.headers.get("Range"))
                if ranged:
                    assert self.headers["Range"] == "bytes=0-3"
                calls[mode, ranged] += 1
                if mode == "missing":
                    self.send_error(404)
                    return
                data = b"<html>not a package</html>" if mode == "html" else BODY
                partial = mode == "range" and ranged
                if partial:
                    data = data[:4]
                self.send_response(206 if partial else 200)
                if mode != "unknown-length" or ranged:
                    self.send_header("Content-Length", str(len(data)))
                if partial:
                    self.send_header("Content-Range", f"bytes 0-3/{len(BODY)}")
                    self.send_header("Accept-Ranges", "bytes")
                self.end_headers()
                if mode == "truncated" and not ranged:
                    data = data[:10000]
                try:
                    for pos in range(0, len(data), 8192):
                        self.wfile.write(data[pos:pos + 8192])
                except (ConnectionError, OSError):
                    pass  # The four-byte probe intentionally closes a full-body response.

        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            for mode in ("range", "local", "missing", "html", "cancel", "space", "unknown-length", "truncated", "safe-refusal", "uncertain-refusal"):
                with self.subTest(mode=mode), tempfile.TemporaryDirectory(prefix="psgi-download-") as directory:
                    url = f"http://127.0.0.1:{server.server_port}/{mode}/A%20Game%20%5BUS%5D.pkg?token=kept"
                    result = subprocess.run([str(RUNNER), url, directory, mode], capture_output=True, text=True, timeout=15)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(calls["range", True], 1)
            self.assertEqual(calls["range", False], 0)
            self.assertEqual(calls["local", True], 2)
            self.assertEqual(calls["local", False], 1)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()
