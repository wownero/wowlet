import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parents[1]))
SPEC = importlib.util.spec_from_file_location("write_receipt", Path(__file__).parents[1] / "write_receipt.py")
receipt = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(receipt)


class ReceiptTests(unittest.TestCase):
    def test_hash_changes_with_artifact_bytes_and_symlinks_refuse(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "candidate.zip"
            path.write_bytes(b"candidate")
            first = receipt.file_record(path)
            path.write_bytes(b"different candidate")
            second = receipt.file_record(path)
            self.assertNotEqual(first["sha256"], second["sha256"])
            self.assertEqual(second["size"], path.stat().st_size)
            link = Path(tmp) / "link.zip"
            link.symlink_to(path)
            with self.assertRaisesRegex(ValueError, "regular file"):
                receipt.file_record(link)
