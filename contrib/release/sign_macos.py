"""Sign one private candidate without placing passwords in process arguments.

Apple SecurityTool reads one quoted operation from stdin in interactive mode:
https://github.com/apple-oss-distributions/SecurityTool/blob/main/security.c
Output stays captured because native diagnostics may repeat supplied input.
"""
import base64
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile


FIELDS = (
    "APPLE_CERTIFICATE", "APPLE_CERTIFICATE_PASSWORD", "APPLE_SIGNING_IDENTITY",
    "APPLE_API_ISSUER", "APPLE_API_KEY", "APPLE_API_KEY_P8_BASE64",
)


def argument(value):
    if any(ord(char) < 32 or ord(char) == 127 for char in value):
        raise ValueError("Keychain input contains an unsupported control character")
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'


def child_environment():
    return {key: os.environ[key] for key in ("HOME", "PATH", "TMPDIR", "LANG", "DEVELOPER_DIR") if key in os.environ}


def security(operation, *values):
    if operation not in {
        "create-keychain", "set-keychain-settings", "unlock-keychain", "import",
        "set-key-partition-list", "delete-keychain",
    }:
        raise ValueError("Keychain operation is outside candidate signing")
    command = " ".join(argument(value) for value in (operation, *values)) + "\n"
    if len(command.encode()) > 1024:
        raise ValueError("Keychain input exceeds the bounded operation size")
    result = subprocess.run(
        ["/usr/bin/security", "-i", "-q"], input=command,
        text=True, capture_output=True, env=child_environment(), timeout=90,
    )
    if result.returncode:
        # Never forward diagnostics or operation data. Report recognized causes.
        causes = {
            "MAC verification failed": "PKCS12 encoding or password verification failed",
            "User interaction is not allowed": "keychain requires interactive access",
            "already exists": "ephemeral keychain or imported identity already exists",
            "could not be found": "requested keychain item was not found",
        }
        cause = next((cause for marker, cause in causes.items() if marker in result.stderr),
                     "native keychain operation returned an error")
        raise RuntimeError(f"{operation}: {cause} (exit {result.returncode}); raw diagnostics withheld")


def run(arguments, *, timeout=120):
    result = subprocess.run(arguments, text=True, capture_output=True,
                            env=child_environment(), timeout=timeout)
    if result.returncode:
        causes = {
            "no identity found": "expected signing identity was not found",
            "User interaction is not allowed": "signing key requires interactive access",
            "errSecInternalComponent": "keychain denied access to the signing identity",
            "not signed at all": "candidate contains an unsigned component",
        }
        cause = next((cause for marker, cause in causes.items() if marker in result.stderr),
                     "native signing operation returned an error")
        raise RuntimeError(f"{Path(arguments[0]).name}: {cause} (exit {result.returncode}); raw diagnostics withheld")
    return result.stdout


def private_file(path, content):
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, "wb") as stream:
        stream.write(content)


def signing_inputs():
    credentials = {name: os.environ.get(name, "") for name in FIELDS}
    missing = [name for name, value in credentials.items() if not value]
    if missing:
        raise ValueError("Signing input is missing: " + ", ".join(missing))
    if "Such Software" not in credentials["APPLE_SIGNING_IDENTITY"]:
        raise ValueError("Developer ID identity does not name the expected publisher")
    return credentials


def main():
    credentials = signing_inputs()
    app = Path("build/dist/Wowlet.app").resolve()
    if not app.is_dir():
        raise ValueError("Packaged Wowlet.app is missing")
    old_umask = os.umask(0o077)
    try:
        with tempfile.TemporaryDirectory(prefix=".wowlet-signing-", dir=app.parent) as temporary:
            workspace = Path(temporary)
            keychain = workspace / "candidate.keychain-db"
            certificate = workspace / "identity.p12"
            notary_key = workspace / "notary.p8"
            private_file(certificate, base64.b64decode(credentials["APPLE_CERTIFICATE"]))
            private_file(notary_key, base64.b64decode(credentials["APPLE_API_KEY_P8_BASE64"]))
            password = secrets.token_hex(32)
            created = False
            try:
                security("create-keychain", "-p", password, str(keychain))
                created = True
                security("set-keychain-settings", "-lut", "3600", str(keychain))
                security("unlock-keychain", "-p", password, str(keychain))
                security("import", str(certificate), "-k", str(keychain), "-P",
                         credentials["APPLE_CERTIFICATE_PASSWORD"], "-T", "/usr/bin/codesign")
                security("set-key-partition-list", "-S", "apple-tool:,apple:", "-s",
                         "-k", password, str(keychain))

                def sign(path):
                    run(["/usr/bin/codesign", "--force", "--options", "runtime", "--timestamp",
                         "--keychain", str(keychain), "--sign",
                         credentials["APPLE_SIGNING_IDENTITY"], str(path)])

                for path in sorted(app.rglob("*")):
                    if path.is_file() and not any(part.endswith(".framework") for part in path.parts):
                        if "Mach-O" in run(["/usr/bin/file", "-b", str(path)]):
                            sign(path)
                frameworks = app / "Contents/Frameworks"
                for path in sorted(frameworks.glob("*.framework")):
                    sign(path)
                sign(app)
                run(["/usr/bin/codesign", "--verify", "--deep", "--strict", str(app)])
                archive = workspace / "candidate.zip"
                run(["/usr/bin/ditto", "-c", "-k", "--keepParent", str(app), str(archive)])
                raw = run(["/usr/bin/xcrun", "notarytool", "submit", str(archive),
                           "--key", str(notary_key), "--key-id", credentials["APPLE_API_KEY"],
                           "--issuer", credentials["APPLE_API_ISSUER"], "--wait", "--timeout", "30m",
                           "--output-format", "json"], timeout=1900)
                response = json.loads(raw)
                if response.get("status") != "Accepted":
                    raise RuntimeError("Apple notarization did not return Accepted")
                run(["/usr/bin/xcrun", "stapler", "staple", str(app)])
                run(["/usr/bin/xcrun", "stapler", "validate", str(app)])
                print("Developer ID signature verified; notarization accepted; staple validated")
            finally:
                if created:
                    security("delete-keychain", str(keychain))
    finally:
        os.umask(old_umask)


if __name__ == "__main__":
    try:
        if sys.argv[1:] == ["--check-inputs"]:
            signing_inputs()
            print("Required signing inputs are present")
        elif sys.argv[1:]:
            raise ValueError("Use no arguments to sign or --check-inputs for the cheap input guard")
        else:
            main()
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        # These exceptions contain logical fields, native result classes or
        # public file paths. Captured child output is never included.
        raise SystemExit(str(error)) from None
