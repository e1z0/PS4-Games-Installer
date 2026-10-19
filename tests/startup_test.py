"""Run the real application through a simulated sandbox-root transition."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
import unittest

PROJECT = Path(__file__).resolve().parents[1]


class StartupRegression(unittest.TestCase):
    def run_startup(self, fail=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "assets").mkdir()
            (root / "build").mkdir()
            for name in ("font.ttf", "icon.png", "cacert.pem", "godaddy-secure-g2.pem"):
                shutil.copyfile(PROJECT / "assets" / name, root / "assets" / name)
            env = dict(os.environ)
            env.pop("PSGI_TEST_STARTUP_FAILURE", None)
            if fail:
                env["PSGI_TEST_STARTUP_FAILURE"] = "1"
            start = time.monotonic()
            result = subprocess.run([str(PROJECT / "build/host/startup-test")], cwd=root,
                                    env=env, capture_output=True, text=True, timeout=30)
            elapsed = time.monotonic() - start
            self.assertTrue((root / "sandbox-assets-hidden/icon.png").is_file())
            self.assertFalse((root / "assets").exists())
            self.assertIn("SANDBOX_ASSETS_HIDDEN", result.stdout)
            self.assertIn("VIDEO_CLOSED\nAPPLICATION_FINISHED=", result.stdout)
            self.assertIn(f"APPLICATION_FINISHED={result.returncode}", result.stdout)
            return result, elapsed

    def test_dashboard_survives_losing_sandbox_asset_paths(self):
        result, elapsed = self.run_startup()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        match = re.search(r"DASHBOARD_AFTER_SECONDS=([0-9.]+)", result.stdout)
        self.assertIsNotNone(match, result.stdout)
        self.assertLess(float(match[1]), 3.0, "Ready dashboard was held behind a splash timer")

    def test_initialization_failure_exits_without_splash_delay(self):
        result, elapsed = self.run_startup(fail=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Simulated privilege setup failure", result.stderr)
        self.assertNotIn("DASHBOARD_AFTER_SECONDS", result.stdout)
        self.assertLess(elapsed, 3.0, "Startup failure was held behind a splash timer")


if __name__ == "__main__":
    unittest.main()
