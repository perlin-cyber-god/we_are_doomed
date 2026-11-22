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

rm -f build/guest-status.txt build/guest-run.log
if ./build_guest.sh >build/guest-build.log 2>&1; then
    cat build/guest-build.log
    python3 verify_compiled_guest.py build/guest.bin build/guest.elf | tee build/guest-run.log
    echo 'COMPILED AND EXECUTED' > build/guest-status.txt
else
    result=$?
    cat build/guest-build.log
    if [[ $result != 77 ]]; then echo 'BUILD FAILED' >build/guest-status.txt; exit "$result"; fi
    if [[ -f guest/prebuilt/guest.bin ]]; then
        sha256sum --check guest/prebuilt/manifest.sha256
        python3 verify_compiled_guest.py guest/prebuilt/guest.bin guest/prebuilt/guest.elf | tee build/guest-run.log
        echo 'EXECUTED VERIFIED PREBUILT C GUEST; local cross-build skipped' >build/guest-status.txt
    else
        echo 'NOT COMPILED OR EXECUTED: toolchain unavailable' > build/guest-status.txt
    fi
fi
sha256sum --check originals.sha256
echo 'PASS base CPU and original compiled guest checks.'
./platform/build.sh
./build/test-platform build/square.bin
python3 -m py_compile platform/window.py
if [[ ${GUI_SMOKE:-0} == 1 ]]; then
    python3 platform/window.py --smoke
else
    echo 'GUI smoke not requested (GUI_SMOKE=1 requires an active display).'
fi
./runtime/build.sh
python3 runtime/verify.py
echo 'PASS all available checks, including the interpreted runtime guest.'
