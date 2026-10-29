#!/bin/sh
# Optional SuperH/QEMU test of gint's real key event transforms.
# GINT_SOURCE must point to the matching gint source tree; its license applies.
set -eu
cd "$(dirname "$0")/.."
: "${GINT_SOURCE:?Set GINT_SOURCE to the gint source tree}"
CC=${CC_SH:-sh-elf-gcc}
QEMU=${QEMU_SH:-qemu-sh4eb}
T=$(mktemp -d "${TMPDIR:-/tmp}/objview-gint-sh.XXXXXX")
trap 'rm -rf "$T"' EXIT HUP INT TERM
"$CC" -m4-nofpu -mb -DFXCG50 -std=c11 -O2 -fno-builtin \
  -ffunction-sections -fdata-sections -nostdlib \
  -I"$GINT_SOURCE/include" \
  tests/test_gint_keys_sh.c "$GINT_SOURCE/src/keysc/keydev.c" \
  "$GINT_SOURCE/src/keysc/getkey.c" \
  -Wl,-T,tests/qemu-sh.ld -Wl,--gc-sections,-z,max-page-size=0x1000 \
  -o "$T/keys.elf"
"$QEMU" "$T/keys.elf"
