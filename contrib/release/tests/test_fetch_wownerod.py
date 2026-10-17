import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("fetch_wownerod", Path(__file__).parents[1] / "fetch_wownerod.py")
fetcher = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(fetcher)


class DaemonTests(unittest.TestCase):
    def test_checksum_failure_never_extracts_a_daemon(self):
        response = io.BytesIO(b"untrusted archive bytes")
        response.geturl = lambda: "https://release-assets.githubusercontent.com/archive"
        with tempfile.TemporaryDirectory() as tmp, patch.object(fetcher.urllib.request, "urlopen", return_value=response), patch.object(fetcher, "copy_daemon") as extract:
            with self.assertRaisesRegex(ValueError, "SHA-256"):
                fetcher.fetch("x86_64-linux-gnu", Path(tmp) / "daemon")
            extract.assert_not_called()
            self.assertFalse((Path(tmp) / "daemon/wownerod").exists())

    def test_archive_paths_cannot_write_outside_destination(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            archive = root / "daemon.tar.bz2"
            with tarfile.open(archive, "w:bz2") as output:
                info = tarfile.TarInfo("../../wownerod")
                info.size = 3
                output.addfile(info, io.BytesIO(b"exe"))
            destination = root / "stage"
            destination.mkdir()
            fetcher.copy_daemon(archive, destination, False)
            self.assertEqual((destination / "wownerod").read_bytes(), b"exe")
            self.assertFalse((root / "wownerod").exists())

    def test_symlink_daemon_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            archive = root / "daemon.tar.bz2"
            with tarfile.open(archive, "w:bz2") as output:
                info = tarfile.TarInfo("wownerod")
                info.type = tarfile.SYMTYPE
                info.linkname = "/etc/passwd"
                output.addfile(info)
            with self.assertRaisesRegex(ValueError, "regular daemon"):
                fetcher.copy_daemon(archive, root, False)
            self.assertFalse((root / "wownerod").exists())
