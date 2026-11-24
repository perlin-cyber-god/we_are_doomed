#!/usr/bin/env bash
# Complete Step 2 entry point: preserved Step 1 plus decoder/integration.
set -euo pipefail
cd "$(dirname "$0")"
root=$PWD
mkdir -p build
python3 scripts/make_decode_vectors.py --check
backend=${1:-auto}
if [[ $backend == auto ]]; then
 if command -v "${IVERILOG:-iverilog}" >/dev/null; then backend=iverilog
 elif command -v xvlog >/dev/null && command -v xelab >/dev/null && command -v xsim >/dev/null; then backend=vivado
 else echo 'No HDL simulator found. Load Vivado settings64.sh, then ./test.sh vivado' >&2; exit 77; fi
fi
./test_step1.sh "$backend"
if command -v "${CC:-gcc}" >/dev/null; then
 "${CC:-gcc}" -std=c11 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow -O2 \
  -Ireference reference/rv32i.c scripts/check_decode_reference.c -o build/check-decode-reference
 ./build/check-decode-reference vectors/decode_vectors.txt | tee build/decode-reference.log
else
 echo 'SKIP C legality cross-check: native C compiler unavailable.' | tee build/decode-reference.log
fi
sources=(rtl/rv32_alu.sv rtl/rv32_regfile.sv rtl/rv32_immediate.sv rtl/rv32_decode.sv rtl/rv32_decode_slice.sv)
tops=(tb_decode_demo tb_decode tb_decode_slice)
case "$backend" in
 iverilog)
  flags=()
  if [[ -n ${IVERILOG_BASE:-} ]]; then flags+=(-B "$IVERILOG_BASE"); fi
  for top in "${tops[@]}"; do
   "${IVERILOG:-iverilog}" "${flags[@]}" -g2012 -Wall -s "$top" \
    -o "build/$top.vvp" "${sources[@]}" "tb/$top.sv" 2>&1 | tee "build/$top-compile.log"
   timeout 30s "${VVP:-vvp}" "build/$top.vvp" \
    "+DECODE_VECTORS=$root/vectors/decode_vectors.txt" "+VCD=$root/build/$top.vcd" \
    2>&1 | tee "build/$top.log"
  done
  ;;
 vivado)
  # Step 1 has already checked that the Vivado executables are available.
  absolute_sources=()
  for file in "${sources[@]}"; do absolute_sources+=("$root/$file"); done
  cd build/vivado
  timeout 60s xvlog -sv "${absolute_sources[@]}" \
   "$root/tb/tb_decode_demo.sv" "$root/tb/tb_decode.sv" "$root/tb/tb_decode_slice.sv" \
   2>&1 | tee step2-compile.log
  for top in "${tops[@]}"; do
   timeout 60s xelab "work.$top" -s "$top" -debug typical 2>&1 | tee "$top-elaborate.log"
   timeout 60s xsim "$top" -runall \
    -testplusarg "DECODE_VECTORS=$root/vectors/decode_vectors.txt" \
    -testplusarg "VCD=$root/build/$top.vcd" \
    2>&1 | tee "$root/build/$top.log"
  done
  ;;
 *) echo 'Usage: ./test.sh [auto|iverilog|vivado]' >&2;exit 2;;
esac
grep -q '^PASS RTL_DECODE_DEMO:' "$root/build/tb_decode_demo.log"
grep -q '^PASS RTL_DECODE rows=' "$root/build/tb_decode.log"
grep -q '^PASS RTL_DECODE_SLICE issued=' "$root/build/tb_decode_slice.log"
echo 'PASS RTL Step 2: decoder and externally driven arithmetic slice; no instruction fetch or full CPU yet.'
