#!/usr/bin/env bash
# Focused Step5 console/timer milestone. No GUI or Doom.
set -euo pipefail
cd "$(dirname "$0")"
backend=${1:-auto}
if [[ $backend == auto ]]; then
 if command -v "${IVERILOG:-iverilog}" >/dev/null; then backend=iverilog
 elif command -v xvlog >/dev/null && command -v xelab >/dev/null && command -v xsim >/dev/null; then backend=vivado
 else echo 'Load Vivado settings64.sh, then ./run_io.sh vivado' >&2;exit 77;fi
fi
python3 -u scripts/test_io.py "$backend"
