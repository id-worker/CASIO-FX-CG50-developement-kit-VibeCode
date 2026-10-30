#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
command -v sh-elf-gcc >/dev/null
command -v qemu-sh4eb >/dev/null
work=$(mktemp -d "${TMPDIR:-/tmp}/gl110-sh-XXXXXX");trap 'rm -rf "$work"' EXIT HUP INT TERM
flags='-m4-nofpu -mb -DFXCG50 -fno-builtin -Os -ffunction-sections -fdata-sections'
for f in tests/target_start.S tests/target_mesh110.c src/expr.c src/plot.c src/mesh_preview.c src/section.c; do
    b=$(basename "${f%.*}");sh-elf-gcc $flags -c "$f" -o "$work/$b.o"
done
sh-elf-gcc -m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments \
  "$work"/*.o -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$work/test.elf"
qemu-sh4eb "$work/test.elf"
echo '1.10 mesh and section passed on SH QEMU (synthetic heap/VRAM, not Casio OS or hardware).'
# Additional big-endian SH verification of the 1.11 raster span division.
mkdir "$work/math"
sh-elf-gcc $flags -c tests/target_start.S -o "$work/math/start.o"
sh-elf-gcc $flags -c tests/target_preview_math111.c -o "$work/math/math.o"
sh-elf-gcc $flags -DGL_TEST_PREVIEW_MATH -O2 -c src/mesh_preview.c -o "$work/math/mesh.o"
sh-elf-gcc -m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments \
  "$work/math"/*.o -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$work/math/test.elf"
qemu-sh4eb "$work/math/test.elf"
echo '1.11 fast span division matches signed division on big-endian SH QEMU.'
