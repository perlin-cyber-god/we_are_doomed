#!/usr/bin/env bash
# Focused controls check only: no GUI, engine startup, or long regression suite.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
"${CC:-gcc}" -std=c11 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow -O2 \
 rv32i.c doom/host.c doom/test_host.c -o build/test-controls
./build/test-controls
python3 doom/test_keys.py
