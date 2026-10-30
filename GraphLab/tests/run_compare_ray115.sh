#!/bin/sh
# Same frames and numerical counters against 1.14; host is not a calculator.
set -eu
cd "$(dirname "$0")/..";newroot=$(pwd);oldroot=${PREVIOUS_ROOT:-$(dirname "$(dirname "$newroot")")/GraphLab_1.14/Source}
test -f "$oldroot/src/ray.c" || { echo 'Set PREVIOUS_ROOT to GraphLab 1.14 Source.' >&2; exit 1; }
work=$(mktemp -d "${TMPDIR:-/tmp}/gl115-host-frame-XXXXXX");trap 'rm -rf "$work"' EXIT HUP INT TERM
CC=${CC:-cc}
for kind in old new no_columns; do
 root=$newroot;[ "$kind" = old ] && root=$oldroot
 extra='';[ -f "$root/src/refine.c" ] && extra="$root/src/refine.c"
 opt='';wrap='';[ "$kind" = no_columns ] && { opt='-DFAIL_COLUMNS';wrap='-Wl,--wrap=malloc'; }
 "$CC" -std=c11 -DGL_HOST -O2 -Wall -Wextra -Werror $opt -I"$root/tests" -I"$root/src" \
   tests/test_compare_ray113.c "$root/tests/host_gint.c" "$root/src/expr.c" \
   "$root/src/ray.c" "$root/src/trigfast.c" "$root/src/preview3d.c" "$root/src/plot.c" $extra \
   $wrap -lm -o "$work/$kind"
 "$work/$kind" "$work/$kind.bin" 2>"$work/$kind.stats"
done
cmp "$work/old.bin" "$work/new.bin";cmp "$work/new.bin" "$work/no_columns.bin"
cmp "$work/old.stats" "$work/new.stats";cmp "$work/new.stats" "$work/no_columns.stats"
echo 'Selected versions: 14 host frames and all numerical counters identical; column-allocation failure also identical.'
