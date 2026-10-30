#!/bin/sh
# Reproduce the old DEL regressions from 1.18 using the current host VRAM.
# This compiles a temporary copy of the old code; it never changes the old release.
set -eu
cd "$(dirname "$0")/..";root=$(pwd)
old=${PREVIOUS_ROOT:-/home/user/GraphLab_1.18/Source}
gint=${GINT_SOURCE:-/home/dev/.local/share/giteapc/Lephenixnoir/gint}
[ -f "$old/src/main.c" ] && [ -f "$gint/src/font8x9.png" ] || {
    echo 'Set PREVIOUS_ROOT to 1.18 Source and GINT_SOURCE to a gint source checkout.' >&2;exit 1;
}
out=${1:-"$root/baseline-screens"};mkdir -p "$out";out=$(cd "$out" && pwd)
w=$(mktemp -d /tmp/gl119-baseline-XXXXXX);trap 'rm -rf "$w"' EXIT HUP INT TERM
cp -R "$old/src" "$w/src"
# The 1.19 synthetic event has an additional timestamp field. Use its header
# consistently in ALL temporary 1.18 objects, not a mixture of event ABIs.
python3 - "$w/src/platform.h" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text();a='#include "../tests/host_gint.h"'
assert a in s;p.write_text(s.replace(a,'#include "host_gint.h"'))
PY
python3 tools/export_host_font.py "$gint/src/font8x9.png" > "$w/font.h"
cc=${CC:-cc};flags='-std=c11 -O1 -DGL_HOST -Wall -Wextra -Werror -Wpedantic'
for mod in input navigation navigation_input expr layout editor_meta plot plot2d ray trigfast voxel preview3d refine intersection fractal scroll storage; do
    "$cc" $flags -I"$w/src" -I"$root/tests" -c "$w/src/$mod.c" -o "$w/$mod.o"
done
"$cc" $flags -I"$w/src" -I"$root/tests" -Dmain=graphlab_main -DGL_EDITOR_TEST -DGL_EDITOR_TRACE -c "$w/src/main.c" -o "$w/main.o"
"$cc" $flags "-DGL_HOST_FONT=\"$w/font.h\"" -c tests/host_gint.c -o "$w/host.o"
"$cc" $flags -I"$w/src" -I"$root/tests" tests/audit_editor_cli119.c "$w"/*.o -lm -o "$w/editor"
SNAP="$out/sum_before.ppm" SNAP_KEY=10 "$w/editor" 'summ(1,3,n,((x)/(n)))' \
    DEL DEL DEL DEL DEL DEL DEL DEL DEL DEL > "$out/sum_trace.txt"
SNAP="$out/func_before.ppm" SNAP_KEY=7 "$w/editor" '2*sin(x+1)' \
    LEFT LEFT LEFT LEFT DEL DEL DEL > "$out/func_trace.txt"
python3 - "$out" <<'PY'
from pathlib import Path
from PIL import Image
import sys
for p in Path(sys.argv[1]).glob('*.ppm'):
    Image.open(p).save(p.with_suffix('.png'),optimize=True);p.unlink()
PY
echo 'Baseline traces and synthetic host shots saved; they are not calculator photos.'
