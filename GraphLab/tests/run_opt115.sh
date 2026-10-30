#!/bin/sh
# 1.15 binary32 RAY and trig exactness. Add --exhaustive for every positive
# input phase with exponents 107..132. Host tests do not measure CG50 time.
set -eu
cd "$(dirname "$0")/..";CC=${CC:-cc}
w=$(mktemp -d "${TMPDIR:-/tmp}/gl115-opt-XXXXXX");trap 'rm -rf "$w"' EXIT HUP INT TERM
F='-std=c11 -O2 -DGL_HOST -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Isrc -Itests'
$CC $F -DGL_TRIG_TEST -c src/trigfast.c -o "$w/phase.o"
$CC $F tests/test_phase115.c "$w/phase.o" -Wl,--gc-sections -lm -o "$w/phase"
"$w/phase" "$@"
$CC $F -DGL_RAY_TEST -fno-builtin -c src/ray.c -o "$w/ray.o"
$CC $F -fno-builtin tests/test_ray_float115.c "$w/ray.o" -Wl,--gc-sections -lm -o "$w/ray"
"$w/ray"
$CC $F -Dtrigfast_prepare=trigfast_prepare_old -Dtrigfast_value=trigfast_value_old -c tests/ref_trigfast113.c -o "$w/old.o"
$CC $F -DGL_TRIG_TEST -Dtrigfast_prepare=trigfast_prepare_new -Dtrigfast_value=trigfast_value_new -c src/trigfast.c -o "$w/new.o"
$CC $F tests/test_trig_patterns115.c "$w/old.o" "$w/new.o" src/expr.c -lm -o "$w/patterns"
"$w/patterns"
# On unknown float layouts, the old trig implementation is retained.
$CC $F -U__SIZEOF_FLOAT__ -DGL_TRIG_TEST -c src/trigfast.c -o "$w/portable.o"
$CC $F tests/test_phase115.c "$w/portable.o" -Wl,--gc-sections -lm -o "$w/portable"
"$w/portable"
$CC $F -U__SIZEOF_FLOAT__ -DGL_RAY_TEST -fno-builtin -c src/ray.c -o "$w/ray-portable.o"
$CC $F -fno-builtin tests/test_ray_float115.c "$w/ray-portable.o" -Wl,--gc-sections -lm -o "$w/ray-portable"
"$w/ray-portable"
echo '1.15 host optimization checks passed.'
