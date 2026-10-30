#!/bin/sh
# Host screenshots only; not Casio OS, a physical display or a photograph.
set -eu
cd "$(dirname "$0")/.."
root=$(pwd)
gint=${GINT_SOURCE:-"/home/dev/.local/share/giteapc/Lephenixnoir/gint"}
font="$gint/src/font8x9.png"
[ -f "$font" ] || { echo 'Set GINT_SOURCE to a gint source checkout.' >&2;exit 1; }
out=${1:-"$root/audit-screens"}
mkdir -p "$out";out=$(cd "$out" && pwd)
w=$(mktemp -d /tmp/gl120-screen-XXXXXX);trap 'rm -rf "$w"' EXIT HUP INT TERM
python3 tools/export_host_font.py "$font" > "$w/font.h"
cc=${CC:-cc};flags='-std=c11 -O1 -DGL_HOST -Wall -Wextra -Werror -Wpedantic'
for mod in input navigation navigation_input expr layout editor_meta plot plot2d ray trigfast preview3d refine intersection fractal scroll storage; do
    "$cc" $flags -c "src/$mod.c" -o "$w/$mod.o"
done
"$cc" $flags -Dmain=graphlab_main -c src/main.c -o "$w/main.o"
"$cc" $flags "-DGL_HOST_FONT=\"$w/font.h\"" -c tests/host_gint.c -o "$w/host.o"
"$cc" $flags tests/audit_screens120.c "$w"/*.o -lm -o "$w/screens"
for scene in sum sum_erased product unary_erased fraction_edit series_edit calc calc_sum 2d 3d_edit 3d_preview 3d_progress 3d_final 3d_volume settings3d 3d_map 3d_map_found fractal; do
    "$w/screens" "$scene" "$out/$scene.ppm"
done
python3 - "$out" <<'PY'
from pathlib import Path
from PIL import Image
import sys
for p in Path(sys.argv[1]).glob('*.ppm'):
    Image.open(p).save(p.with_suffix('.png'),optimize=True)
    p.unlink()
PY
printf 'Host screenshots saved to %s. They are not calculator photos.\n' "$out"
