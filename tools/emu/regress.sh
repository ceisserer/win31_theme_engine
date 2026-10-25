#!/bin/sh
# Runs every scenario in tools/emu/scenarios against the golden screenshots
# in tests/emu/golden/<profile>/<scenario>/.
#
#   regress.sh <windows-image> [profile] [--update-golden]
#
# The profile names the golden set (default: win31, for the retail Windows
# 3.1 image made by mkwin31.py). Exit code 1 if any screenshot differs.
set -u
IMG=$1
PROFILE=${2:-win31}
UPDATE=${3:-}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${TMPDIR:-/tmp}/retheme-regress
rc=0
for sc in "$ROOT"/tools/emu/scenarios/*.txt; do
    name=$(basename "$sc" .txt)
    echo "== $name"
    if ! python3 "$ROOT/tools/emu/run.py" --base "$IMG" --out "$OUT/$name" --script "$sc" \
            --golden "$ROOT/tests/emu/golden/$PROFILE/$name" $UPDATE > "$OUT.$name.log" 2>&1; then
        rc=1
    fi
    grep -E '^(ok|FAIL|NO GOLDEN|golden)' "$OUT.$name.log"
done
[ $rc = 0 ] && echo "all scenarios match" || echo "differences: see $OUT/<scenario>/*.diff.png"
exit $rc
