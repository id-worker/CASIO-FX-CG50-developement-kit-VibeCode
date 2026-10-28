#!/bin/sh
# Host-only integration assertions, not a calculator/hardware test.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/pycedit-host-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cd "$work"
# Requires a C compiler, pkg-config and the libpng development package.
${CC:-cc} -std=gnu99 -DHOST_TEST -O1 -g -Wall -Wextra \
    "$here/test_pycedit.c" -o "$work/test_pycedit" \
    $(pkg-config --cflags --libs libpng) -lm
"$work/test_pycedit"
