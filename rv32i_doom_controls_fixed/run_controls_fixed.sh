#!/usr/bin/env bash
# Rebuild only the native adapter; use the unchanged verified RV32I guest.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build/doom
sha256sum --check doom/prebuilt/manifest.sha256 > build/controls-prebuilt-check.log 2>&1 || {
 cat build/controls-prebuilt-check.log; exit 1;
}
"${CC:-gcc}" -std=c11 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow -O2 \
 -fPIC -shared rv32i.c doom/host.c -o build/libdoomhost.so
cp doom/prebuilt/doom.bin build/doom/doom.bin
echo 'Using unchanged verified RV32I guest with corrected host fire/use mapping.'
exec python3 doom/window.py
