from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
CHECKER = ROOT / "contrib/AppImage/check-libcom-err.sh"


class AppImageDependencyBundleTest(unittest.TestCase):
    def test_accepts_appdir_with_libcom_err(self):
        with tempfile.TemporaryDirectory() as temporary:
            appdir = Path(temporary) / "AppDir"
            library = appdir / "usr/lib/libcom_err.so.2"
            library.parent.mkdir(parents=True)
            library.write_bytes(b"fixture")

            result = subprocess.run(["bash", str(CHECKER), str(appdir)], check=False)

            self.assertEqual(result.returncode, 0)

    def test_refuses_appdir_without_libcom_err(self):
        with tempfile.TemporaryDirectory() as temporary:
            appdir = Path(temporary) / "AppDir"
            (appdir / "usr/lib").mkdir(parents=True)

            result = subprocess.run(
                ["bash", str(CHECKER), str(appdir)],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertTrue(result.stderr.strip())


if __name__ == "__main__":
    unittest.main()
