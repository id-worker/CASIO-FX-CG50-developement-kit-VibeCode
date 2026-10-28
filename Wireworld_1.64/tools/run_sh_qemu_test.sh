#!/bin/sh
# ISA-level SH4A big-endian engine comparison. Not a Casio OS/G3A emulator.
set -eu
cd "$(dirname "$0")/.."
: "${FXCGSDK:?Set FXCGSDK to the libfxcg SDK directory}"
CC=${SH_CC:-sh3eb-elf-gcc}
QEMU=${QEMU_SH:-qemu-sh4eb}
T=$(mktemp -d "${TMPDIR:-/tmp}/wireworld-sh.XXXXXXXX")
trap 'rm -rf "$T"' EXIT HUP INT TERM
# Linux/QEMU requires 4K congruent PT_LOAD offsets on SH, hence -z max-page-size.
LIBGCC=$($CC -mb -m4a-nofpu -mhitachi -print-file-name=libgcc_be.a)
if [ "$LIBGCC" = libgcc_be.a ]; then GCCLIB=-lgcc; else GCCLIB=-lgcc_be; fi
$CC -mb -m4a-nofpu -mhitachi -Os -ffunction-sections -fdata-sections \
    -I src -I "$FXCGSDK/include" -nostdlib -T tools/uitest/qemu-sh.ld \
    -Wl,-gc-sections,-z,max-page-size=0x1000 \
    tools/uitest/sh_qemu_test.c src/wireworld.c tools/reference/reference_wrapper.c \
    "$GCCLIB" -o "$T/engine"
"$QEMU" -cpu sh7785 "$T/engine"
