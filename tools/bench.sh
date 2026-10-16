#!/usr/bin/env bash
# Times every bench/*.relic on the tree-walker (--tree) and on the VM (default) and checks
# both print the same thing. Not run in CI; usage: tools/bench.sh [path/to/relic]
set -euo pipefail
RELIC=${1:-build/relic}
cd "$(dirname "$0")/.."
printf "%-12s %10s %10s %8s\n" bench tree-walk vm speedup
for f in bench/*.relic; do
  # redraw needs a window; skip it on builds without graphics.
  if [ "$(basename "$f")" = redraw.relic ] && ! echo 'print(host_gfx_available())' | "$RELIC" | grep -q true; then
    continue
  fi
  t0=$(date +%s.%N); a=$("$RELIC" --tree "$f"); t1=$(date +%s.%N)
  b=$("$RELIC" "$f"); t2=$(date +%s.%N)
  [ "$a" = "$b" ] || { echo "output differs on $f"; exit 1; }
  awk -v n="$(basename "$f" .relic)" -v t0="$t0" -v t1="$t1" -v t2="$t2" \
    'BEGIN { x = t1 - t0; y = t2 - t1; printf "%-12s %9.3fs %9.3fs %7.2fx\n", n, x, y, x / y }'
done
