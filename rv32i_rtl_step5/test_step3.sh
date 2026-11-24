#!/usr/bin/env bash
# Complete regression; no interactive windows. Every command has a host timeout.
set -euo pipefail
cd "$(dirname "$0")"
backend=${1:-auto}
if [[ $backend == auto ]]; then
 if command -v "${IVERILOG:-iverilog}" >/dev/null; then backend=iverilog
 elif command -v xvlog >/dev/null && command -v xelab >/dev/null && command -v xsim >/dev/null; then backend=vivado
 else echo 'Load Vivado settings64.sh, then ./test.sh vivado; or install Icarus and run ./test.sh iverilog' >&2;exit 77;fi
fi
case "$backend" in iverilog|vivado) ;; *) echo 'Usage: ./test.sh [auto|iverilog|vivado]' >&2;exit 2;; esac
./test_step2.sh "$backend"
python3 scripts/test_core.py "$backend"
