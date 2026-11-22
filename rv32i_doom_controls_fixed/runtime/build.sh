#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
host_flags=(-std=c11 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow)
if [[ ${SANITIZE:-0} == 1 ]]; then
 host_flags+=(-O1 -g -fsanitize=address,undefined -fno-pie -no-pie -fno-omit-frame-pointer)
else host_flags+=(-O2); fi
"${CC:-gcc}" "${host_flags[@]}" rv32i.c rv_run.c runtime/runner.c -o build/runtime-run
cross=${RISCV_CC:-}
if [[ -z $cross ]]; then
 for candidate in riscv32-unknown-elf-gcc riscv64-unknown-elf-gcc riscv64-linux-gnu-gcc; do
  if command -v "$candidate" >/dev/null; then cross=$candidate; break; fi
 done
fi
if [[ -n $cross ]]; then
 flags=(-march=rv32i -mabi=ilp32 -mno-relax -msmall-data-limit=0 -ffreestanding
        -fno-builtin -fno-stack-protector -fno-pic -fno-pie -O1 -g -std=c11
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow)
 objects=()
 for source in runtime/memory.c runtime/heap.c runtime/assets.c runtime/math.c runtime/guest/checks.c; do
  obj="build/rt-$(basename "${source%.c}").o"
  "$cross" "${flags[@]}" -c "$source" -o "$obj"
  objects+=("$obj")
 done
 "$cross" -march=rv32i -mabi=ilp32 -mno-relax -c runtime/guest/start.S -o build/rt-start.o
 "$cross" -march=rv32i -mabi=ilp32 -nostdlib -nostartfiles -static \
  -Wl,-T,runtime/guest/linker.ld -Wl,--no-relax -Wl,--build-id=none -Wl,-Map,build/runtime.map \
  build/rt-start.o "${objects[@]}" -lgcc -o build/runtime.elf
 copier=${RISCV_OBJCOPY:-${cross%-gcc}-objcopy}
 "$copier" -O binary build/runtime.elf build/runtime.bin
 python3 runtime/check_elf.py build/runtime.elf
else
 sha256sum --check runtime/prebuilt/manifest.sha256
 cp runtime/prebuilt/runtime.bin build/runtime.bin
 python3 runtime/check_elf.py runtime/prebuilt/runtime.elf
 echo 'Using hash-checked prebuilt runtime guest.'
fi
