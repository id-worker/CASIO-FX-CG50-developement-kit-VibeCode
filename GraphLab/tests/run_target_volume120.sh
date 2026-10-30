#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
command -v sh-elf-gcc >/dev/null;command -v qemu-sh4eb >/dev/null
w=$(mktemp -d /tmp/gl120-target-volume-XXXXXX);trap 'rm -rf "$w"' EXIT HUP INT TERM
flags='-m4-nofpu -mb -DFXCG50 -fno-builtin -Os -ffunction-sections -fdata-sections'
for f in tests/target_start.S tests/target_volume120.c src/expr.c src/trigfast.c src/preview3d.c src/refine.c;do
 b=$(basename "${f%.*}");sh-elf-gcc $flags -c "$f" -o "$w/$b.o"
done
sh-elf-gcc -m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments \
 "$w"/*.o -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$w/test.elf"
qemu-sh4eb "$w/test.elf"
echo 'SuperH Ray volumes: analytic sphere, clip faces, poles, domain edge and mixed nearest depth passed; 16 KiB stack (no Casio OS/hardware).'
