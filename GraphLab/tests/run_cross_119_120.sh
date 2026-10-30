#!/bin/sh
# Exercise both real storage implementations on the same journal files.
set -eu
cd "$(dirname "$0")/.."
old=${PREVIOUS_ROOT:-/home/user/GraphLab_1.19/Source}
[ -f "$old/src/storage.c" ] || { echo 'Set PREVIOUS_ROOT to GraphLab 1.19 Source.' >&2; exit 1; }
w=$(mktemp -d /tmp/gl120-cross-XXXXXX);trap 'rm -rf "$w"' EXIT HUP INT TERM
cc=${CC:-cc}
"$cc" -std=c11 -DGL_HOST -Wall -Wextra -Werror -I"$old/src" tests/cross_119_120.c "$old/src/storage.c" -lm -o "$w/old"
"$cc" -std=c11 -DGL_HOST -DGRAPHLAB_120 -Wall -Wextra -Werror -Isrc tests/cross_119_120.c src/storage.c -lm -o "$w/new"
for mode in 0 2; do
 mkdir "$w/data$mode"
 "$w/old" "$w/data$mode" old-write "$mode"
 cp "$w/data$mode/GLAB8_A.DAT" "$w/before$mode"
 "$w/new" "$w/data$mode" new-read "$mode"
 cmp "$w/before$mode" "$w/data$mode/GLAB8_A.DAT"
 "$w/new" "$w/data$mode" new-update "$mode"
 "$w/old" "$w/data$mode" old-read "$mode"
done
printf '%s\n' 'Previous GLAB8 -> 1.20 -> previous: mode bytes 0/2, Q, steps, formulas, ranges and camera preserved; load alone is read-only.'
