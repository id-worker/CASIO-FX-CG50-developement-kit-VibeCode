#!/bin/sh
set -eu
cd "$(dirname "$0")/..";command -v sh-elf-gcc >/dev/null;command -v qemu-sh4eb >/dev/null
work=$(mktemp -d "${TMPDIR:-/tmp}/gl113-target-expr-XXXXXX");trap 'rm -rf "$work"' EXIT HUP INT TERM
flags='-m4-nofpu -mb -DFXCG50 -fno-builtin -Os -ffunction-sections -fdata-sections'
sh-elf-gcc $flags -Isrc -c tests/target_start.S -o "$work/start.o"
sh-elf-gcc $flags -Isrc -c tests/target_expr_fast113.c -o "$work/test.o"
sh-elf-gcc $flags -Isrc -c src/expr.c -o "$work/expr.o"
sh-elf-gcc -m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments \
 "$work"/*.o -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$work/test.elf"
qemu-sh4eb "$work/test.elf"
echo 'SH/QEMU: 6000 fast/ordinary evaluator cases, including NaN, signed zero and 16 KiB stack passed.'
