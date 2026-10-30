#!/bin/sh
# Legacy sources retained for history, NOT linked into current 1.20 G3A.
set -eu
cd "$(dirname "$0")/.."
w=$(mktemp -d /tmp/gl119-legacy-XXXXXX);trap 'rm -rf "$w"' EXIT HUP INT TERM
cc=${CC:-cc};flags='-g -O1 -std=c11 -DGL_HOST -Wall -Wextra -Werror -Wpedantic'
if [ "${SANITIZE:-1}" = 1 ]; then flags="$flags -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie"; fi
$cc $flags tests/test_section110.c src/section.c src/expr.c -Wl,--wrap=malloc -lm -o "$w/section"
"$w/section"
$cc $flags tests/test_mesh110.c src/mesh_preview.c src/plot.c src/expr.c tests/host_gint.c -Wl,--wrap=malloc,--wrap=expr_eval_real -lm -o "$w/mesh"
"$w/mesh"
echo 'Both legacy modules were tested separately; neither is linked in the 1.20 add-in.'
