#!/bin/sh
# Host CPU timing is not fx-CG50 timing and excludes the calculator display.
set -eu
cd "$(dirname "$0")/.."
old=${PREVIOUS_ROOT:-/home/user/GraphLab_1.18/Source}
[ -f "$old/src/ray.c" ] || { echo 'Set PREVIOUS_ROOT to GraphLab 1.18 Source.' >&2;exit 1; }
w=$(mktemp -d /tmp/gl119-bench-XXXXXX);trap 'rm -rf "$w"' EXIT HUP INT TERM
cc=${CC:-cc}
build(){
    root=$1;out=$2;opt=$3
    "$cc" -O2 -DGL_HOST $opt -I"$root/src" -I"$root/tests" tests/bench_stream119.c \
        "$root/src/expr.c" "$root/src/ray.c" "$root/src/trigfast.c" "$root/src/refine.c" \
        "$root/src/preview3d.c" "$root/tests/host_gint.c" -lm -o "$out"
}
build "$old" "$w/old" ''
build . "$w/new" '-DGL_BENCH_STREAM'
printf '1.18 ';"$w/old"
printf 'current (1.20) ';"$w/new"
