#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build

# Use an already installed toolchain only. A present compiler that fails to
# build is a real failure (exit 1), not the missing-toolchain skip (exit 77).
cross=()
link_extra=()
if [[ -n ${RISCV_CC:-} ]]; then
    command -v "$RISCV_CC" >/dev/null || { echo "RISCV_CC not found" >&2; exit 1; }
    cross=("$RISCV_CC")
else
    for candidate in riscv32-unknown-elf-gcc riscv64-unknown-elf-gcc riscv64-linux-gnu-gcc; do
        if command -v "$candidate" >/dev/null; then cross=("$candidate"); break; fi
    done
    if [[ ${#cross[@]} == 0 ]] && command -v clang >/dev/null && command -v ld.lld >/dev/null; then
        targets=$(clang --print-targets)
        if [[ $targets == *riscv32* ]]; then
            cross=(clang --target=riscv32-unknown-elf)
            link_extra=(-fuse-ld=lld)
        fi
    fi
fi
if [[ ${#cross[@]} == 0 ]]; then
    echo "SKIP: no installed RISC-V GCC, or Clang RISC-V backend with ld.lld."
    echo "Local guest cross-build was not performed; test.sh checks for a prebuilt image."
    exit 77
fi

flags=(-march=rv32i -mabi=ilp32 -mno-relax -msmall-data-limit=0
       -ffreestanding -fno-builtin -fno-pic -fno-pie -fno-stack-protector
       -fno-omit-frame-pointer -O0 -g -Wall -Wextra -Wpedantic -Werror
       -Wconversion -Wshadow)
link=(-nostdlib -nostartfiles -static -Wl,-T,guest/linker.ld
      -Wl,--no-relax -Wl,--build-id=none)

set -x
"${cross[@]}" "${flags[@]}" -std=c11 -c guest/guest.c -o build/guest.o
"${cross[@]}" -march=rv32i -mabi=ilp32 -mno-relax -c guest/start.S -o build/start.o
"${cross[@]}" -march=rv32i -mabi=ilp32 "${link_extra[@]}" "${link[@]}" \
    -Wl,-Map,build/guest.map build/start.o build/guest.o -o build/guest.elf
# GNU RISC-V ld cannot change format during a link. Convert its ELF using
# objcopy; NOLOAD .bss/signature/stack remain absent from the flat image.
if [[ ${cross[0]} == *gcc ]]; then
    copier=${RISCV_OBJCOPY:-${cross[0]%-gcc}-objcopy}
    "$copier" -O binary build/guest.elf build/guest.bin
else
    "${cross[@]}" -march=rv32i -mabi=ilp32 "${link_extra[@]}" "${link[@]}" \
        -Wl,--oformat=binary build/start.o build/guest.o -o build/guest.bin
fi
