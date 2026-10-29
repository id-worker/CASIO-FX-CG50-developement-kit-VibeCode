#!/bin/sh
# Host integration/VRAM/key tests for the light version; not a calculator test.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/pycedit-host-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
# Requires a C compiler, pkg-config and the libpng development package.
san=""
if [ "${SANITIZE:-0}" = 1 ]; then
    san="-fsanitize=address,undefined -fno-omit-frame-pointer"
fi
cd "$work"
${CC:-cc} -std=gnu99 -DHOST_TEST -O1 -g -Wall -Wextra $san \
    "$here/test_pycedit.c" -o test_pycedit \
    $(pkg-config --cflags --libs libpng) -lm
echo '--- PyCEdit light ---'
./test_pycedit
if [ -n "${PYCEDIT_TEST_IMAGES:-}" ]; then
    mkdir -p "$PYCEDIT_TEST_IMAGES/light"
    cp ./*.png "$PYCEDIT_TEST_IMAGES/light/"
fi
