#!/bin/sh
# 12 further host RAY frames + 8 point previews vs 1.14.
set -eu
cd "$(dirname "$0")/..";newroot=$(pwd);oldroot=${PREVIOUS_ROOT:-$(dirname "$(dirname "$newroot")")/GraphLab_1.14/Source};CC=${CC:-cc}
test -f "$oldroot/src/ray.c" || { echo 'Set PREVIOUS_ROOT to GraphLab 1.14 Source.' >&2;exit 1; }
work=$(mktemp -d "${TMPDIR:-/tmp}/gl115-extra-XXXXXX");trap 'rm -rf "$work"' EXIT HUP INT TERM
for kind in old new;do
 root=$newroot;[ "$kind" = old ] && root=$oldroot
 extra='';[ -f "$root/src/refine.c" ] && extra="$root/src/refine.c"
 "$CC" -std=c11 -DGL_HOST -O2 -Wall -Wextra -Werror -I"$root/tests" -I"$root/src" \
 tests/test_compare_ray115_extra.c "$root/tests/host_gint.c" "$root/src/expr.c" \
 "$root/src/ray.c" "$root/src/trigfast.c" "$root/src/preview3d.c" "$root/src/plot.c" $extra -lm -o "$work/$kind"
 "$work/$kind" "$work/$kind.bin" 2>"$work/$kind.stats"
done
cmp "$work/old.bin" "$work/new.bin";cmp "$work/old.stats" "$work/new.stats"
echo '12 RAY images, 8 point-Preview images/clouds and all stats identical to selected previous version on host.'
