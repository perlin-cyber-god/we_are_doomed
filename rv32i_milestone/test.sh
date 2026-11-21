#!/usr/bin/env bash
# Single entry point: preserved milestones, new ISA, runner, optional C guest.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
host_cc=${CC:-gcc}
flags=(-std=c11 -Wall -Wextra -Wpedantic -Werror)
new_flags=(-Wconversion -Wshadow)
if [[ ${SANITIZE:-0} == 1 ]]; then
    flags+=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
else
    flags+=(-O2)
fi
"$host_cc" --version | head -1
printf 'Host flags:'; printf ' %q' "${flags[@]}"; printf '\n'
sha256sum --check originals.sha256
cmp -n "$(wc -c < README.original.md)" README.original.md README.md

for step in 1 2 3 4; do
    "$host_cc" "${flags[@]}" "cpu_step${step}.c" -o "build/cpu_step${step}"
    "./build/cpu_step${step}" > "build/step${step}.log" 2>&1 || { cat "build/step${step}.log"; exit 1; }
    tail -1 "build/step${step}.log"
done
"$host_cc" "${flags[@]}" "${new_flags[@]}" rv32i.c test_rv32i.c -o build/test-rv32i
./build/test-rv32i
"$host_cc" "${flags[@]}" "${new_flags[@]}" rv32i.c rv_run.c test_run.c -o build/test-run
./build/test-run
"$host_cc" "${flags[@]}" "${new_flags[@]}" rv32i.c rv_run.c rv32_run.c -o build/rv32-run

if ./build_guest.sh >build/guest-build.log 2>&1; then
    cat build/guest-build.log
    ./build/rv32-run build/guest.bin | tee build/guest-run.log
    echo 'COMPILED AND EXECUTED' > build/guest-status.txt
else
    result=$?
    cat build/guest-build.log
    if [[ $result != 77 ]]; then exit "$result"; fi
    echo 'NOT COMPILED OR EXECUTED: toolchain unavailable' > build/guest-status.txt
fi
sha256sum --check originals.sha256
echo 'PASS all available checks (see guest status above).'
