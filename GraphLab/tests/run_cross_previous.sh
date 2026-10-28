#!/bin/sh
# Exchange actual journal files between independently compiled 1.06 and
# a previous GraphLab version (1.05 by default; PREVIOUS_ROOT can select another).
# This tests host files; it does not emulate the calculator file system.
set -eu
cd "$(dirname "$0")/.."
newroot=$(pwd)
oldroot=${PREVIOUS_ROOT:-"$(dirname "$newroot")/GraphLab_fxCG50_v1.05"}
test -f "$oldroot/src/storage.c" || { echo "Set PREVIOUS_ROOT to the extracted previous GraphLab source." >&2; exit 1; }
work=$(mktemp -d "${TMPDIR:-/tmp}/gl106-cross-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
CC=${CC:-cc}
"$CC" -std=c11 -DGL_HOST -Wall -Wextra -Werror -I"$oldroot/src" tests/cross_storage.c "$oldroot/src/storage.c" -lm -o "$work/old"
"$CC" -std=c11 -DGL_HOST -Wall -Wextra -Werror -I"$newroot/src" tests/cross_storage.c "$newroot/src/storage.c" -lm -o "$work/new"
mkdir "$work/data"
"$work/old" "$work/data" write-old
"$work/new" "$work/data" read-old
"$work/new" "$work/data" write-new
"$work/old" "$work/data" read-new
