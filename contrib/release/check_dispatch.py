"""Refuse a build whose resolved source differs from its reviewed request."""
import os
import re
import subprocess


def check(expected, reported, checked_out):
    if not re.fullmatch(r"[0-9a-f]{40}", expected or ""):
        raise ValueError("expected_sha must identify the exact reviewed commit")
    if reported != expected:
        raise ValueError("provider run head differs from expected_sha")
    if checked_out != expected:
        raise ValueError("checked-out source differs from expected_sha")


if __name__ == "__main__":
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    try:
        check(os.environ.get("EXPECTED_SHA"), os.environ.get("GITHUB_SHA"), head)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("Exact requested build source verified")
