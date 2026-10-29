#!/bin/sh
# SuperH CPU/VRAM regression tests only. No Casio OS or physical screen.
set -eu
cd "$(dirname "$0")/.."
: "${FXCGSDK:?Set FXCGSDK to the libfxcg SDK used to build PyCEdit}"
CC=${CC_SH:-sh3eb-elf-gcc}
QEMU=${QEMU_SH:-qemu-sh4eb}
work=$(mktemp -d "${TMPDIR:-/tmp}/pycedit-sh.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$CC" -mb -m4a-nofpu -mhitachi -nostdlib -std=gnu99 -Os -fno-builtin \
    -ffunction-sections -fdata-sections \
    -I"$FXCGSDK/include" tools/sh_start.S tools/test_pycedit_sh.c \
    -Wl,-T,tools/qemu-sh.ld,--gc-sections,-z,max-page-size=0x1000 \
    -L"$FXCGSDK/lib" -Wl,--start-group -lc -lfxcg -lgcc -Wl,--end-group \
    -o "$work/light.elf"
"$QEMU" "$work/light.elf"
echo 'SuperH/QEMU: light syntax RGB565, language states, C and Python SYM catalog pagination passed.'
