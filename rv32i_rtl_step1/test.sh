#!/usr/bin/env bash
# One bounded, headless entry point. Does not start a GUI or execute Doom.
set -euo pipefail
cd "$(dirname "$0")"
root=$PWD
mkdir -p build
python3 scripts/make_vectors.py --check
backend=${1:-auto}
if [[ $backend == auto ]]; then
 if command -v "${IVERILOG:-iverilog}" >/dev/null; then backend=iverilog
 elif command -v xvlog >/dev/null && command -v xelab >/dev/null && command -v xsim >/dev/null; then backend=vivado
 else echo 'No simulator found. Run from a Vivado-enabled terminal: ./test.sh vivado' >&2; exit 77; fi
fi
case "$backend" in
 iverilog)
  ivl_args=()
  if [[ -n ${IVERILOG_BASE:-} ]]; then ivl_args+=(-B "$IVERILOG_BASE"); fi
  for entry in tb_demo:demo tb_rtl_step1:test; do
   top=${entry%:*}; stem=${entry#*:}
   "${IVERILOG:-iverilog}" "${ivl_args[@]}" -g2012 -Wall -s "$top" \
    -o "build/$stem.vvp" rtl/rv32_alu.sv rtl/rv32_regfile.sv "tb/$top.sv" \
    2>&1 | tee "build/compile-$stem.log"
   timeout 30s "${VVP:-vvp}" "build/$stem.vvp" \
    "+VECTORS=$root/vectors/alu_vectors.txt" "+VCD=$root/build/$stem.vcd" \
    2>&1 | tee "build/$stem.log"
  done
  ;;
 vivado)
  for tool in xvlog xelab xsim; do
   command -v "$tool" >/dev/null || { echo "Missing $tool; load Vivado settings64.sh in this terminal." >&2; exit 77; }
  done
  mkdir -p build/vivado
  cd build/vivado
  timeout 60s xvlog -sv "$root/rtl/rv32_alu.sv" "$root/rtl/rv32_regfile.sv" \
   "$root/tb/tb_demo.sv" "$root/tb/tb_rtl_step1.sv" \
   2>&1 | tee compile.log
  for entry in tb_demo:demo tb_rtl_step1:test; do
   top=${entry%:*}; stem=${entry#*:}
   timeout 60s xelab "work.$top" -s "rtl_step1_$stem" -debug typical \
    2>&1 | tee "elaborate-$stem.log"
   timeout 60s xsim "rtl_step1_$stem" -runall \
    -testplusarg "VECTORS=$root/vectors/alu_vectors.txt" \
    -testplusarg "VCD=$root/build/$stem.vcd" \
    2>&1 | tee "$root/build/$stem.log"
  done
  ;;
 *) echo 'Usage: ./test.sh [auto|iverilog|vivado]' >&2; exit 2;;
esac
# Some simulator frontends may return success after a failing testbench.
if ! grep -q '^PASS RTL_STEP1 checks=' "$root/build/test.log"; then
 echo 'FAIL: required testbench completion marker is missing.' >&2; exit 1
fi
if ! grep -q '^PASS RTL_DEMO:' "$root/build/demo.log"; then
 echo 'FAIL: required demo completion marker is missing.' >&2; exit 1
fi
echo 'PASS bounded RTL milestone; no full CPU, guest program or GDS has been run.'
