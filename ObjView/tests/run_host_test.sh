#!/bin/sh
# Real application source with host gint stubs; no calculator OS/hardware.
set -eu
cd "$(dirname "$0")/.."
T=$(mktemp -d "${TMPDIR:-/tmp}/objview-ui.XXXXXX")
trap 'rm -rf "$T"' EXIT HUP INT TERM
CC=${CC:-cc}
flags='-std=c11 -O1 -g -ffunction-sections -fdata-sections -Itests -Itests/stub'
$CC $flags tests/test_display.c tests/host_gint.c -Wl,--gc-sections -lm -o "$T/new"
"$T/new"
# Optional: compare viewport raster sample with real older source.
if [ -n "${REFERENCE_SOURCE:-}" ]; then
  $CC $flags -DSOURCE_FILE="\"$REFERENCE_SOURCE\"" tests/test_display.c tests/host_gint.c -Wl,--gc-sections -lm -o "$T/old"
  "$T/new" raster > "$T/new.txt"
  "$T/old" raster > "$T/old.txt"
  cmp "$T/new.txt" "$T/old.txt"
  echo 'Reference/current viewport raster samples identical.'
fi
