#!/usr/bin/env bash
set -euo pipefail

appdir="${1:?usage: check-libcom-err.sh APPDIR}"
library="$(find "$appdir/usr/lib" -name libcom_err.so.2 -print -quit 2>/dev/null || true)"
if [[ -z "$library" || ! -e "$library" ]]; then
  echo "ERROR: AppDir is missing the bundled libcom_err.so.2 runtime library." >&2
  exit 1
fi
