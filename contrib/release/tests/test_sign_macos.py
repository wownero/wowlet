import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("sign_macos", Path(__file__).parents[1] / "sign_macos.py")
signing = importlib.util.module_from_spec(spec)
spec.loader.exec_module(signing)


class MacSigningTests(unittest.TestCase):
    def test_password_uses_private_input_and_diagnostics_never_echo_it(self):
        synthetic = 'synthetic "quoted" value \\ with spaces'
        captured = []

        def execute(arguments, **options):
            captured.append((arguments, options))
            return SimpleNamespace(returncode=1, stdout=synthetic,
                                   stderr="MAC verification failed " + synthetic)

        with patch.object(signing.subprocess, "run", execute):
            with self.assertRaises(RuntimeError) as caught:
                signing.security("import", "candidate.p12", "-P", synthetic)
        self.assertNotIn(synthetic, str(caught.exception))
        arguments, options = captured[0]
        self.assertNotIn(synthetic, " ".join(arguments))
        self.assertIn("quoted", options["input"])
        self.assertTrue(options["capture_output"])
        self.assertNotIn("APPLE_CERTIFICATE_PASSWORD", options["env"])

    def test_control_characters_and_unrelated_keychain_operations_refuse(self):
        with patch.object(signing.subprocess, "run") as execute:
            for value in ("line\nbreak", "nul\x00byte", "carriage\rreturn"):
                with self.assertRaises(ValueError):
                    signing.security("unlock-keychain", "-p", value, "candidate.keychain")
            with self.assertRaises(ValueError):
                signing.security("dump-keychain", "login.keychain")
            execute.assert_not_called()
