#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
./platform/build.sh
if [[ ${1:-} == --headless ]]; then
    exec ./build/test-platform build/square.bin
fi
exec python3 platform/window.py "$@"
