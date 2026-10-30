#!/bin/sh
# Independent 1.08/1.10 host binaries; no calculator OS emulation.
set -eu
cd "$(dirname "$0")/.."
newroot=$(pwd)
# This test only needs the bundled 1.08 storage implementation and headers.
oldroot=${PREVIOUS_ROOT:-"$newroot/tests/fixtures/GraphLab_1.08"}
test -f "$oldroot/src/storage.c" || { echo 'Set PREVIOUS_ROOT to GraphLab 1.08 sources.' >&2; exit 1; }
work=$(mktemp -d "${TMPDIR:-/tmp}/gl110-q-cross-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cc=${CC:-cc}
"$cc" -std=c11 -DGL_HOST -Wall -Wextra -Werror -I"$oldroot/src" tests/cross_quality.c "$oldroot/src/storage.c" -lm -o "$work/old"
"$cc" -std=c11 -DGL_HOST -DGL_109 -Wall -Wextra -Werror -I"$newroot/src" tests/cross_quality.c "$newroot/src/storage.c" -lm -o "$work/new"
mkdir "$work/data"
"$work/old" "$work/data" old-write
cp -a "$work/data" "$work/no-sidecar"
rm "$work/no-sidecar"/GLQ8_*.DAT
"$work/new" "$work/no-sidecar" new-untrusted
sha256sum "$work/data"/*.DAT > "$work/before"
"$work/new" "$work/data" new-import
sha256sum "$work/data"/GLAB3_*.DAT "$work/data"/GLQ8_*.DAT > "$work/after"
cmp "$work/before" "$work/after"
"$work/old" "$work/data" old-read-own
"$work/new" "$work/data" new-still-own
echo 'Legacy Q sidecar import and journal isolation verified.'
