#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
flags=(-std=c11 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow)
if [[ ${SANITIZE:-0} == 1 ]]; then
    flags+=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
else
    flags+=(-O2)
fi
"${CC:-gcc}" "${flags[@]}" -fPIC -shared rv32i.c platform/platform.c -o build/libplatform.so
"${CC:-gcc}" "${flags[@]}" -fno-pie -no-pie rv32i.c platform/platform.c platform/test_platform.c -o build/test-platform
cross=${RISCV_CC:-}
if [[ -z $cross ]]; then
    for candidate in riscv32-unknown-elf-gcc riscv64-unknown-elf-gcc riscv64-linux-gnu-gcc; do
        if command -v "$candidate" >/dev/null; then cross=$candidate; break; fi
    done
fi
if [[ -n $cross ]]; then
    "$cross" -march=rv32i -mabi=ilp32 -mno-relax -msmall-data-limit=0 \
        -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie \
        -O1 -g -std=c11 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow \
        -c platform/guest/square.c -o build/square.o
    "$cross" -march=rv32i -mabi=ilp32 -mno-relax -c platform/guest/start.S -o build/square-start.o
    "$cross" -march=rv32i -mabi=ilp32 -nostdlib -nostartfiles -static \
        -Wl,-T,platform/guest/linker.ld -Wl,--no-relax -Wl,--build-id=none \
        -Wl,-Map,build/square.map build/square-start.o build/square.o -o build/square.elf
    copier=${RISCV_OBJCOPY:-${cross%-gcc}-objcopy}
    "$copier" -O binary build/square.elf build/square.bin
elif [[ -f platform/prebuilt/square.bin ]]; then
    sha256sum --check platform/prebuilt/manifest.sha256
    cp platform/prebuilt/square.bin build/square.bin
    echo 'Using verified prebuilt RV32I square guest.'
else
    echo 'Missing RISC-V GNU compiler and prebuilt square image.' >&2; exit 1
fi
