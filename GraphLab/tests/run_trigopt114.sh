#!/bin/sh
# Bitwise trig lookup/fast-value verification vs the unchanged 1.13 reference.
set -eu
cd "$(dirname "$0")/..";CC=${CC:-cc}
work=$(mktemp -d "${TMPDIR:-/tmp}/gl114-trig-XXXXXX");trap 'rm -rf "$work"' EXIT HUP INT TERM
flags='-std=c11 -O2 -DGL_HOST -Wall -Wextra -Wpedantic -Werror -Isrc -Itests'
$CC $flags -Dtrigfast_prepare=trigfast_prepare_old -Dtrigfast_value=trigfast_value_old -c tests/ref_trigfast113.c -o "$work/old.o"
$CC $flags -DGL_TRIG_TEST -Dtrigfast_prepare=trigfast_prepare_new -Dtrigfast_value=trigfast_value_new -c src/trigfast.c -o "$work/new.o"
$CC $flags tests/test_trigfast114.c "$work/old.o" "$work/new.o" src/expr.c -lm -o "$work/test"
"$work/test"
$CC $flags tests/test_trigfast114_edges.c "$work/old.o" "$work/new.o" src/expr.c -lm -o "$work/edges"
"$work/edges"
# Unknown/non-binary32 hosts retain the previous implementation.
$CC $flags -DGL_TRIG_TEST -U__SIZEOF_FLOAT__ -Dtrigfast_prepare=trigfast_prepare_new -Dtrigfast_value=trigfast_value_new -c src/trigfast.c -o "$work/fallback.o"
$CC $flags tests/test_trigfast114.c "$work/old.o" "$work/fallback.o" src/expr.c -lm -o "$work/fallback"
"$work/fallback"
echo '1.14 trig binary32 and portable fallback tests passed.'
