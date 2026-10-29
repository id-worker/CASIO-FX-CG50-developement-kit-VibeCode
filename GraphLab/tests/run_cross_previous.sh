#!/bin/sh
# Exchange actual journal files between independently compiled 1.09 and
# a previous GraphLab version (1.08 by default; PREVIOUS_ROOT can select another).
# This tests host files; it does not emulate the calculator file system.
set -eu
cd "$(dirname "$0")/.."
newroot=$(pwd)
# A minimal, licensed 1.08 storage fixture lives with these tests; override
# PREVIOUS_ROOT to test another complete compatible version instead.
oldroot=${PREVIOUS_ROOT:-"$newroot/tests/fixtures/GraphLab_1.08"}
test -f "$oldroot/src/storage.c" || { echo "Set PREVIOUS_ROOT to the extracted previous GraphLab source." >&2; exit 1; }
work=$(mktemp -d "${TMPDIR:-/tmp}/gl109-cross-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
CC=${CC:-cc}
"$CC" -std=c11 -DGL_HOST -Wall -Wextra -Werror -I"$oldroot/src" tests/cross_storage.c "$oldroot/src/storage.c" -lm -o "$work/old"
"$CC" -std=c11 -DGL_HOST -Wall -Wextra -Werror -I"$newroot/src" tests/cross_storage.c "$newroot/src/storage.c" -lm -o "$work/new"
mkdir "$work/data"
"$work/old" "$work/data" write-old
"$work/new" "$work/data" read-old
sha256sum "$work/data"/GLAB3_*.DAT > "$work/old-before"
"$work/new" "$work/data" write-new
sha256sum "$work/data"/GLAB3_*.DAT > "$work/old-after"
cmp "$work/old-before" "$work/old-after"
"$work/new" "$work/data" read-new
"$work/old" "$work/data" read-old
echo 'Old journal byte-for-byte intact; new and old versions retain their own saves.' 
