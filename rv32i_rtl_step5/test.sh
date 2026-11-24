#!/usr/bin/env bash
# Complete preserved regressions followed by Step5.
set -euo pipefail
cd "$(dirname "$0")"
./test_step4.sh "${1:-auto}"
./run_io.sh "${1:-auto}"
