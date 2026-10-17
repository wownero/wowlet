"""Fetch the pinned daemon archive and copy only its verified daemon binary."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import tarfile
import tempfile
import urllib.request
import zipfile

MAX_ARCHIVE = 300 * 1024 * 1024
MAX_BINARY = 128 * 1024 * 1024
TARGETS = {"x86_64-linux-gnu", "x86_64-w64-mingw32", "aarch64-apple-darwin11", "x86_64-apple-darwin11"}


def copy_daemon(archive, destination, windows):
    name = "wownerod.exe" if windows else "wownerod"
    if windows:
        container = zipfile.ZipFile(archive)
        members = [m for m in container.infolist() if PurePosixPath(m.filename).name == name]
        valid = lambda m: not m.is_dir() and (m.external_attr >> 16 & 0o170000) in (0, 0o100000) and 0 < m.file_size <= MAX_BINARY
        reader = container.open
    else:
        container = tarfile.open(archive, "r:bz2")
        members = [m for m in container.getmembers() if PurePosixPath(m.name).name == name]
        valid = lambda m: m.isfile() and 0 < m.size <= MAX_BINARY
        reader = container.extractfile
    with container:
        if len(members) != 1 or not valid(members[0]):
            raise ValueError("Pinned archive must contain exactly one regular daemon binary")
        # Do not extract archive paths, symlinks or unrelated files.
        with reader(members[0]) as source, (destination / name).open("xb") as output:
            shutil.copyfileobj(source, output)
    (destination / name).chmod(0o755)


def fetch(target, destination):
    manifest = json.loads(Path(__file__).with_name("wownerod.json").read_text())
    suffix = ".zip" if target.endswith("mingw32") else ".tar.bz2"
    filename = f"wownero-{target}-{manifest['version']}{suffix}"
    expected = manifest["archives"][filename]
    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="wowlet-daemon-") as staging:
        archive = Path(staging) / filename
        digest = hashlib.sha256()
        size = 0
        url = f"https://github.com/wownero/wownero/releases/download/{manifest['version']}/{filename}"
        with urllib.request.urlopen(url, timeout=120) as response, archive.open("xb") as output:
            if not response.geturl().startswith("https://"):
                raise ValueError("Daemon download redirected outside HTTPS")
            while chunk := response.read(1024 * 1024):
                size += len(chunk)
                if size > MAX_ARCHIVE:
                    raise ValueError("Daemon archive exceeds size bound")
                digest.update(chunk)
                output.write(chunk)
        if digest.hexdigest() != expected:
            raise ValueError("Daemon archive SHA-256 differs from the reviewed release pin")
        copy_daemon(archive, destination, target.endswith("mingw32"))
    print(f"Verified {filename}: {expected}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", required=True, choices=sorted(TARGETS))
    parser.add_argument("--destination", required=True, type=Path)
    args = parser.parse_args()
    fetch(args.target, args.destination)


if __name__ == "__main__":
    main()
