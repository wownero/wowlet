import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
import tempfile
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

    def test_missing_inputs_refuse_without_native_operations(self):
        with patch.dict(signing.os.environ, {}, clear=True), patch.object(signing.subprocess, "run") as execute:
            with self.assertRaises(ValueError):
                signing.signing_inputs()
            execute.assert_not_called()

    def test_control_characters_and_unrelated_keychain_operations_refuse(self):
        with patch.object(signing.subprocess, "run") as execute:
            for value in ("line\nbreak", "nul\x00byte", "carriage\rreturn"):
                with self.assertRaises(ValueError):
                    signing.security("unlock-keychain", "-p", value, "candidate.keychain")
            with self.assertRaises(ValueError):
                signing.security("dump-keychain", "login.keychain")
            execute.assert_not_called()

    def test_user_keychain_search_is_restored_after_signing_failure(self):
        previous = ['/Users/runner/Library/Keychains/login.keychain-db', '/Library/Keychains/System.keychain']
        calls = []
        def security(operation, *values):
            calls.append((operation, values))
            return ' '.join('"' + value + '"' for value in previous)
        with patch.object(signing, "security", security):
            with self.assertRaisesRegex(RuntimeError, "synthetic signing failure"):
                with signing.keychain_search(Path('/private/candidate.keychain-db')):
                    self.assertEqual(calls[-1][1][-len(previous):], tuple(previous))
                    self.assertIn('/private/candidate.keychain-db', calls[-1][1])
                    raise RuntimeError('synthetic signing failure')
        self.assertEqual(calls[-1], ('list-keychains', ('-d', 'user', '-s', *previous)))

    def test_failed_search_list_write_still_restores_original_list(self):
        calls = []
        previous = '/Users/runner/Library/Keychains/login.keychain-db'
        def security(operation, *values):
            calls.append((operation, values))
            if '/private/candidate.keychain-db' in values:
                raise RuntimeError('synthetic search write failure')
            return '"' + previous + '"'
        with patch.object(signing, "security", security):
            with self.assertRaises(RuntimeError):
                with signing.keychain_search(Path('/private/candidate.keychain-db')):
                    self.fail('failed search-list setup entered signing')
        self.assertEqual(calls[-1][1], ('-d', 'user', '-s', previous))

    def test_signing_walk_skips_symlink_aliases_and_framework_internals(self):
        with tempfile.TemporaryDirectory() as temporary:
            app = Path(temporary)
            executable = app / 'actual.dylib'
            executable.write_bytes(b'mach-o fixture')
            (app / 'alias.dylib').symlink_to(executable.name)
            framework = app / 'QtCore.framework'
            framework.mkdir()
            (framework / 'QtCore').write_bytes(b'framework fixture')
            with patch.object(signing, 'run', return_value='Mach-O arm64'):
                self.assertEqual(list(signing.signable_files(app)), [executable])

    def test_native_failure_cause_does_not_echo_untrusted_diagnostics(self):
        sentinel = 'must-not-echo-native-input'
        result = SimpleNamespace(returncode=1, stdout='', stderr='unable to build chain ' + sentinel)
        with patch.object(signing.subprocess, 'run', return_value=result):
            with self.assertRaises(RuntimeError) as caught:
                signing.run(['/usr/bin/codesign', 'public-component'])
        self.assertIn('certificate chain', str(caught.exception))
        self.assertNotIn(sentinel, str(caught.exception))
