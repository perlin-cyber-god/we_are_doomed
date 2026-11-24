#!/usr/bin/env bash
# Full project: preserved Step3 tests, followed by the compiled guest.
set -euo pipefail
cd "$(dirname "$0")"
./test_step3.sh "${1:-auto}"
./run_guest.sh "${1:-auto}"
