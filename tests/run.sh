#!/usr/bin/env sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake -S "$root" -B "$root/build"
cmake --build "$root/build"
ctest --test-dir "$root/build" --output-on-failure
