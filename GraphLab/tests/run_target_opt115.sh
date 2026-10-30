#!/bin/sh
# Exactness checks compiled for SuperH and run in QEMU; not Casio hardware time.
set -eu
cd "$(dirname "$0")/..";command -v sh-elf-gcc >/dev/null;command -v qemu-sh4eb >/dev/null
w=$(mktemp -d "${TMPDIR:-/tmp}/gl115-target-opt-XXXXXX");trap 'rm -rf "$w"' EXIT HUP INT TERM
F='-m4-nofpu -mb -DFXCG50 -fno-builtin -Os -ffunction-sections -fdata-sections -Isrc'
L='-m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments'
sh-elf-gcc $F -c tests/target_start.S -o "$w/start.o"
sh-elf-gcc $F -DGL_TRIG_TEST -c src/trigfast.c -o "$w/phase.o"
sh-elf-gcc $F -c tests/target_phase115.c -o "$w/phase-test.o"
sh-elf-gcc $L "$w/start.o" "$w/phase-test.o" "$w/phase.o" -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$w/phase.elf"
qemu-sh4eb "$w/phase.elf"
echo 'SH: 100000 phase cases + signed edges passed; stack guard intact.'
sh-elf-gcc $F -DGL_RAY_TEST -c src/ray.c -o "$w/ray.o"
sh-elf-gcc $F -c tests/target_ray_float115.c -o "$w/ray-test.o"
sh-elf-gcc $L "$w/start.o" "$w/ray-test.o" "$w/ray.o" -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$w/ray.elf"
qemu-sh4eb "$w/ray.elf"
echo 'SH: 50000 float classifications, comparisons, +/-0 addition and Q scales passed; stack guard intact.'
sh-elf-gcc $F -Dtrigfast_prepare=trigfast_prepare_old -Dtrigfast_value=trigfast_value_old -c tests/ref_trigfast113.c -o "$w/old.o"
sh-elf-gcc $F -DGL_TRIG_TEST -Dtrigfast_prepare=trigfast_prepare_new -Dtrigfast_value=trigfast_value_new -c src/trigfast.c -o "$w/new.o"
sh-elf-gcc $F -c src/expr.c -o "$w/expr.o"
sh-elf-gcc $F -c tests/target_trig_patterns115.c -o "$w/pattern-test.o"
sh-elf-gcc $L "$w/start.o" "$w/pattern-test.o" "$w/old.o" "$w/new.o" "$w/expr.o" -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$w/patterns.elf"
qemu-sh4eb "$w/patterns.elf"
echo 'SH: 64 sin/cos patterns with +/-zero constants, 16384 samples passed; stack guard intact.'
