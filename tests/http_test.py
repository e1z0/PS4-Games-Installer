"""Exercise the real fetch/parser path without contacting a package server."""
import http.server
import json
from collections import Counter
from pathlib import Path
import subprocess
import sys
import tempfile
import threading


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.server.calls[self.path] += 1
        redirects = {
            "/redirect": (302, "/catalog"),
            "/redirect-permanent": (301, "catalog"),
            "/redirect-see-other": (303, f"//127.0.0.1:{self.server.server_port}/catalog"),
            "/redirect-temporary": (307, "/catalog"),
            "/redirect-preserve": (308, "/catalog"),
            "/redirect-loop": (302, "/redirect-loop"),
            "/redirect-invalid": (302, "file:///catalog"),
            "/redirect-missing-location": (302, None),
            "/redirect-missing": (302, "/missing"),
        }
        if self.path in redirects:
            status, location = redirects[self.path]
            self.send_response(status)
            if location is not None:
                self.send_header("Location", location)
            self.end_headers()
            return
        if self.path == "/missing":
            self.send_error(404)
            return
        if self.path == "/large":
            data = b" " * (8 * 1024 * 1024 + 1)
        elif self.path == "/malformed":
            data = b'{"DATA":'
        elif self.path == "/empty":
            data = b'{"DATA":{}}'
        else:
            data = json.dumps({"DATA": {"http://nas/game.pkg": {
                "name": "Test package", "size": 8589934592
            }}}).encode()
        self.send_response(200)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, *args):
        pass


def main():
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.calls = Counter()
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        with tempfile.TemporaryDirectory() as tmp:
            config = Path(tmp) / "config.json"
            for endpoint, expected, text in [
                ("catalog", 0, "1 unique packages"),
                ("empty", 0, "0 unique packages"),
                ("redirect", 0, "1 unique packages"),
                ("redirect-permanent", 0, "1 unique packages"),
                ("redirect-see-other", 0, "1 unique packages"),
                ("redirect-temporary", 0, "1 unique packages"),
                ("redirect-preserve", 0, "1 unique packages"),
                ("redirect-loop", 1, "redirect"),
                ("redirect-invalid", 1, "redirect"),
                ("redirect-missing-location", 1, "Location"),
                ("redirect-missing", 1, "HTTP 404"),
                ("missing", 1, "HTTP 404"),
                ("malformed", 1, "Expected a JSON object"),
                ("large", 1, "Catalog download failed"),
            ]:
                config.write_text(json.dumps({"content_urls": {
                    "games": f"http://127.0.0.1:{server.server_port}/{endpoint}"
                }}))
                result = subprocess.run([sys.argv[1], str(config)], capture_output=True, text=True, timeout=15)
                assert result.returncode == expected, (endpoint, result.stdout, result.stderr)
                assert text.lower() in (result.stdout + result.stderr).lower(), (endpoint, result.stdout, result.stderr)
            assert 1 < server.calls["/redirect-loop"] <= 6, server.calls
        print("HTTP integration tests passed")
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == "__main__":
    main()
