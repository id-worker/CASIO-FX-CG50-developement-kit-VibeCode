#!/bin/sh
# Run the portable engine, real v1.63 equivalence, VRAM, font and RTC tests.
set -eu
cd "$(dirname "$0")/.."
CC=${CC:-gcc}
CFLAGS=${CFLAGS:--O2 -g -Wall -Wextra}
T=$(mktemp -d "${TMPDIR:-/tmp}/wireworld-test.XXXXXXXX")
trap 'rm -rf "$T"' EXIT HUP INT TERM
printf '\n== Native Wireworld rules and circuits ==\n'
$CC -std=gnu99 $CFLAGS -I src tools/test_native.c src/wireworld.c src/patterns.c -o "$T/native"
"$T/native"
printf '\n== Engine vs original 1.63 ==\n'
$CC -std=gnu99 $CFLAGS -I src tools/test_equivalence.c \
    tools/reference/reference_wrapper.c src/wireworld.c -o "$T/equivalence"
"$T/equivalence"
for test in ui_test font_test timing_test; do
    printf '\n== %s ==\n' "$test"
    # ui_test identifies semantic layers by distinct test-only colors.
    # Production remains monochrome; validate its real palette separately below.
    semantic=''
    if [ "$test" = ui_test ]; then semantic='-DWW_TEST_SEMANTIC_COLORS'; fi
    $CC -std=gnu99 $CFLAGS -Wno-unused-function -I tools/uitest/stub -DHOST_TEST $semantic \
        "tools/uitest/$test.c" tools/uitest/host_stub.c \
        src/wireworld.c src/patterns.c -o "$T/$test"
    "$T/$test"
done
printf '\n== real-palette low-memory screen ==\n'
$CC -std=gnu99 $CFLAGS -Wno-unused-function -I tools/uitest/stub -DHOST_TEST \
    tools/uitest/test_low_memory.c tools/uitest/host_stub.c \
    src/wireworld.c src/patterns.c -o "$T/low_memory"
"$T/low_memory"
