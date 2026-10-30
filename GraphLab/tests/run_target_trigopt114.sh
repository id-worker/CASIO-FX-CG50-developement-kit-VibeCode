#!/bin/sh
# Exact Q15/trig comparisons on SH/QEMU, not a hardware speed benchmark.
set -eu
cd "$(dirname "$0")/.."
command -v sh-elf-gcc >/dev/null;command -v qemu-sh4eb >/dev/null
work=$(mktemp -d "${TMPDIR:-/tmp}/gl114-target-trig-XXXXXX");trap 'rm -rf "$work"' EXIT HUP INT TERM
flags='-m4-nofpu -mb -DFXCG50 -fno-builtin -Os -ffunction-sections -fdata-sections -Isrc'
sh-elf-gcc $flags -c tests/target_start.S -o "$work/start.o"
sh-elf-gcc $flags -c tests/target_trigfast114.c -o "$work/test.o"
sh-elf-gcc $flags -Dtrigfast_prepare=trigfast_prepare_old -Dtrigfast_value=trigfast_value_old -c tests/ref_trigfast113.c -o "$work/old.o"
sh-elf-gcc $flags -DGL_TRIG_TEST -Dtrigfast_prepare=trigfast_prepare_new -Dtrigfast_value=trigfast_value_new -c src/trigfast.c -o "$work/new.o"
sh-elf-gcc $flags -c src/expr.c -o "$work/expr.o"
sh-elf-gcc -m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments \
 "$work"/*.o -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$work/test.elf"
qemu-sh4eb "$work/test.elf"
echo 'SH/QEMU: 65535 Q15 conversions, 50000 lookups, 20000 fast-field calls bitwise identical; 16 KiB stack guard passed.'
