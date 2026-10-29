#!/bin/sh
# Builds and runs the library tests. Usage: tests/library/run.sh [path-to-pine-compiler]
# Run from anywhere; paths are resolved relative to the repository root.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PINE="${1:-$ROOT/build/pine}"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
cd "$ROOT"
status=0
for t in json io net; do
    "$PINE" "tests/library/${t}_test.pine" > "$OUT/$t.c"
    cc -Wall -Wno-unused-function "$OUT/$t.c" library/runtime/pine_rt.c -o "$OUT/$t"
    if (cd "$OUT" && "./$t"); then :; else status=1; fi
done
exit $status
