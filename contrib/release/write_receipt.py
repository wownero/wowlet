"""Bind candidate archive hashes to the checked source and provider run."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

from check_dispatch import check


def file_record(path):
    if not path.is_file() or path.is_symlink() or path.stat().st_size == 0:
        raise ValueError("Candidate artifact must be a nonempty regular file")
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return {"name": path.name, "size": path.stat().st_size, "sha256": digest.hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True, choices=("linux", "macos", "windows-build"))
    parser.add_argument("--file", type=Path, required=True, action="append")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    check(os.environ.get("EXPECTED_SHA"), os.environ.get("GITHUB_SHA"), head)
    run_id = os.environ.get("GITHUB_RUN_ID", "")
    if not re.fullmatch(r"[1-9][0-9]*", run_id):
        raise ValueError("Provider run identity is absent")
    version = re.findall(r'^\s*VERSION "([0-9]+\.[0-9]+\.[0-9]+)"\s*$', Path("CMakeLists.txt").read_text(), re.M)
    if len(version) != 1:
        raise ValueError("Committed Wowlet version is ambiguous")
    module = subprocess.check_output(["git", "ls-tree", "HEAD", "monero"], text=True).split()
    if len(module) != 4 or module[:2] != ["160000", "commit"] or module[3] != "monero":
        raise ValueError("Committed wallet-library pin is absent")
    files = [file_record(path) for path in args.file]
    if len({row["name"] for row in files}) != len(files):
        raise ValueError("Candidate artifact filenames repeat")
    receipt = {"schema_version": 1, "app": "wowlet", "version": version[0], "platform": args.platform,
               "dispatch_commit": head, "wallet_library_commit": module[2],
               "environment_run_id": run_id, "environment_run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT"),
               "artifacts": files}
    if args.platform == "macos":
        signing = json.loads(Path("build/dist/macos-signing.json").read_text())
        if signing.get("developer_id_verified") is not True or signing.get("notarization_status") != "Accepted" or signing.get("staple_validated") is not True:
            raise ValueError("macOS signing evidence is incomplete")
        receipt["signing"] = signing
    with args.output.open("x") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print("Candidate source and artifact hashes recorded")


if __name__ == "__main__":
    main()
