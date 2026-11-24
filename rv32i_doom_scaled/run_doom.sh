#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
./doom/build.sh
if [[ ${1:-} == --headless ]]; then
 shift
 exec python3 doom/headless.py "$@"
fi
exec python3 doom/window.py "$@"
