import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("dispatch", Path(__file__).parents[1] / "check_dispatch.py")
dispatch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dispatch)


class DispatchTests(unittest.TestCase):
    def test_exact_source_is_required_before_building(self):
        sha = "a" * 40
        dispatch.check(sha, sha, sha)
        for expected, reported, checked_out in (
            (None, sha, sha), ("master", sha, sha),
            (sha, "b" * 40, sha), (sha, sha, "b" * 40),
        ):
            with self.subTest(expected=expected, reported=reported, checked_out=checked_out):
                with self.assertRaises(ValueError):
                    dispatch.check(expected, reported, checked_out)
