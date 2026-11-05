#!/usr/bin/env bash
# Play-only frame timing with the bot. Usage: tools/perf.sh [zero-based rounds...]
set -euo pipefail
if [ "${1:-}" = --help ]; then
  echo 'Usage: tools/perf.sh [rounds...] (default: 0 3 6 9); set ZEAL_NATIVE and ZOS_ROM'
  exit 0
fi
cd "$(dirname "$0")/.."
ZDE="${ZDE:-zde}"
export ZDE_SOFT_EXIT=0
mkdir -p perf
if [ "$#" -eq 0 ]; then set -- 0 3 6 9; fi
for round in "$@"; do
  case "$round" in ''|*[!0-9]*) echo 'round must be a nonnegative integer' >&2; exit 2 ;; esac
  "$ZDE" --no-tty exec cmake -B "build-p$round" -DAUTOPILOT=ON -DSTART_ROUND="$round" -DBIN_DIR="/src/build-p$round/out"
  "$ZDE" --no-tty cmake "build-p$round"
  python3 tools/bpfps.py "build-p$round/out/bubblekeep.bin" "${SECS:-90}" --watch _game:13 --dump "perf/round$round"
  printf 'start round %d -> ' "$((round + 1))"
  python3 tools/play.py "perf/round$round.frames" 'b[7] > 0'
done
