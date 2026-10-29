#!/bin/sh
# Big-endian Linux/QEMU tests. Not an emulator for the Casio OS or G3A.
set -eu
cd "$(dirname "$0")/.."
CC=${MIPS_CC:-mips-linux-gnu-gcc}
QEMU=${QEMU_MIPS:-qemu-mips}
T=$(mktemp -d "${TMPDIR:-/tmp}/wireworld-mips.XXXXXXXX")
trap 'rm -rf "$T"' EXIT HUP INT TERM
$CC -static -std=gnu99 -O2 -I src tools/test_native.c src/wireworld.c src/patterns.c -o "$T/native"
"$QEMU" "$T/native"
$CC -static -std=gnu99 -O2 -I src tools/test_equivalence.c tools/reference/reference_wrapper.c src/wireworld.c -o "$T/equivalence"
"$QEMU" "$T/equivalence"
for test in ui_test font_test timing_test; do
    $CC -static -std=gnu99 -O2 -Wno-unused-function -DHOST_TEST -I tools/uitest/stub \
        "tools/uitest/$test.c" tools/uitest/host_stub.c src/wireworld.c src/patterns.c -o "$T/$test"
    "$QEMU" "$T/$test"
done
