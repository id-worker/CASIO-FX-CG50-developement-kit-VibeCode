#!/bin/sh
# SH CPU/instruction and linked libm/fxlibc checks; this is not a calculator emulator.
set -eu
cd "$(dirname "$0")/.."
command -v sh-elf-gcc >/dev/null || { echo 'Add the fxSDK bin directory to PATH.' >&2; exit 1; }
command -v qemu-sh4eb >/dev/null || { echo 'Install qemu-user (qemu-sh4eb).' >&2; exit 1; }
work="${TMPDIR:-/tmp}/graphlab-sh-test-$$"
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work"
CFLAGS='-m4-nofpu -mb -DFXCG50 -fno-builtin -Os -ffunction-sections -fdata-sections'
sh-elf-gcc $CFLAGS -c tests/target_start.S -o "$work/start.o"
sh-elf-gcc $CFLAGS -c tests/target_qemu.c -o "$work/test.o"
for module in expr layout plot plot2d ray trigfast voxel preview3d fractal scroll navigation; do sh-elf-gcc $CFLAGS -c "src/$module.c" -o "$work/$module.o"; done
sh-elf-gcc -m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments \
    "$work"/*.o -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$work/test.elf"
qemu-sh4eb "$work/test.elf"
echo 'SuperH QEMU checks passed (no calculator hardware or OS emulation).'
sh-elf-gcc $CFLAGS -c tests/target_107.c -o "$work/test107.o"
sh-elf-gcc -m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments \
    "$work/start.o" "$work/test107.o" "$work/expr.o" "$work/ray.o" "$work/trigfast.o" "$work/voxel.o" \
    "$work/preview3d.o" "$work/fractal.o" "$work/scroll.o" \
    -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$work/test107.elf"
qemu-sh4eb "$work/test107.elf"
echo 'SuperH 1.08: guarded trig, period proof, low-Q RAY, point preview, voxel capture, strips and fractal region passed.'
# Real big-endian storage code with an in-memory file stub; not Casio OS.
mkdir -p "$work/quality"
sh-elf-gcc $CFLAGS -DGL_HOST -c tests/target_start.S -o "$work/quality/start.o"
sh-elf-gcc $CFLAGS -DGL_HOST -c tests/target_quality.c -o "$work/quality/test.o"
sh-elf-gcc $CFLAGS -DGL_HOST -c src/storage.c -o "$work/quality/storage.o"
sh-elf-gcc -m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments \
    "$work"/quality/*.o -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$work/quality.elf"
qemu-sh4eb "$work/quality.elf"
echo 'SuperH custom Q v5 journal passed using a simulated in-memory filesystem (not Casio OS).'
# If the SDK also includes gint sources, exercise its real keydev/getkey
# transforms with synthetic fx-CG50 matrix changes and GraphLab input_apply().
compiler=$(command -v sh-elf-gcc)
sdkroot=$(dirname "$(dirname "$compiler")")
gint_source=${GINT_SOURCE:-"$sdkroot/sources/gint"}
if [ -f "$gint_source/src/keysc/keydev.c" ] && [ -f "$gint_source/src/keysc/getkey.c" ]; then
    mkdir -p "$work/keyboard"
    for f in tests/target_start.S tests/target_keyboard.c src/input.c \
             "$gint_source/src/keysc/keydev.c" "$gint_source/src/keysc/getkey.c"; do
        name=$(basename "${f%.*}")
        sh-elf-gcc $CFLAGS -c "$f" -o "$work/keyboard/$name.o"
    done
    sh-elf-gcc -m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments \
        "$work"/keyboard/*.o -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$work/keyboard.elf"
    qemu-sh4eb "$work/keyboard.elf"
    echo 'gint keydev/getkey + GraphLab modifier checks passed (synthetic matrix, not physical hardware).'
    mkdir -p "$work/editor"
    for f in tests/target_start.S tests/target_editor.c src/input.c src/expr.c src/layout.c src/editor_meta.c \
             "$gint_source/src/keysc/keydev.c" "$gint_source/src/keysc/getkey.c"; do
        name=$(basename "${f%.*}")
        sh-elf-gcc $CFLAGS -c "$f" -o "$work/editor/$name.o"
    done
    sh-elf-gcc $CFLAGS -DGL_EDITOR_TEST -Dmain=graphlab_application_main -c src/main.c -o "$work/editor/main.o"
    sh-elf-gcc -m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments \
        "$work"/editor/*.o -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$work/editor.elf"
    qemu-sh4eb "$work/editor.elf"
    echo 'Actual target editor + gint keydev passed fractions, SUM/PROD, long/deep input and nested OPTN editor on a 16 KiB stack (synthetic input, not physical hardware).'
    mkdir -p "$work/navigation"
    for f in tests/target_start.S tests/target_navigation_input.c src/navigation_input.c \
             "$gint_source/src/keysc/keydev.c"; do
        name=$(basename "${f%.*}")
        sh-elf-gcc $CFLAGS -c "$f" -o "$work/navigation/$name.o"
    done
    sh-elf-gcc -m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments \
        "$work"/navigation/*.o -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$work/navigation.elf"
    qemu-sh4eb "$work/navigation.elf"
    echo 'Actual target camera input + gint keydev passed (synthetic matrix and timer stubs; not physical hardware).'
else
    echo 'Optional gint modifier test skipped: set GINT_SOURCE to the gint source directory.'
fi
