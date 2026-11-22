#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/doom
flags=(-std=c11 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow)
if [[ ${SANITIZE:-0} == 1 ]]; then
 flags+=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
else flags+=(-O2); fi
"${CC:-gcc}" "${flags[@]}" -fPIC -shared rv32i.c doom/host.c -o build/libdoomhost.so
"${CC:-gcc}" "${flags[@]}" -fno-pie -no-pie rv32i.c doom/host.c doom/test_host.c -o build/test-doom-host
cross=${RISCV_CC:-}
if [[ -z $cross ]]; then
 for candidate in riscv32-unknown-elf-gcc riscv64-unknown-elf-gcc riscv64-linux-gnu-gcc; do
  if command -v "$candidate" >/dev/null; then cross=$candidate; break; fi
 done
fi
if [[ -n $cross ]]; then
 RISCV_CC="$cross" PICOLIBC_ROOT="${PICOLIBC_ROOT:-$PWD/doom/picolibc}" python3 doom/build_guest.py
 python3 doom/check_image.py build/doom/doom.elf
else
 sha256sum --check doom/prebuilt/manifest.sha256
 cp doom/prebuilt/doom.bin build/doom/doom.bin
 python3 doom/check_image.py doom/prebuilt/doom.elf
 echo 'Using verified prebuilt Doom engine guest.'
fi
