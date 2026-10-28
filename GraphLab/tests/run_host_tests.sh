#!/bin/sh
# Functional checks on the host. Does not emulate the calculator OS or hardware.
set -eu
cd "$(dirname "$0")/.."
CC=${CC:-cc}
work="${TMPDIR:-/tmp}/graphlab-host-test-$$"
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work"
flags="-std=c11 -DGL_HOST -Wall -Wextra -Werror -Wpedantic -g -O1"
if [ "${SANITIZE:-1}" = 1 ]; then flags="$flags -fsanitize=address,undefined -fno-omit-frame-pointer"; fi
$CC $flags tests/test_expr.c src/expr.c -lm -o "$work/expr"
$CC $flags tests/fuzz_expr.c src/expr.c -lm -o "$work/fuzz"
$CC $flags tests/test_layout.c tests/host_gint.c src/layout.c src/expr.c -lm -o "$work/layout"
$CC $flags tests/test_editor_navigation.c tests/host_gint.c src/layout.c src/expr.c -lm -o "$work/editor_navigation"
$CC $flags tests/test_editor_meta.c tests/host_gint.c src/editor_meta.c src/layout.c src/expr.c -lm -o "$work/editor_meta"
$CC $flags tests/fuzz_layout.c tests/host_gint.c src/layout.c src/expr.c -lm -o "$work/fuzz_layout"
$CC $flags tests/test_plot.c tests/host_gint.c src/plot.c src/expr.c -lm -o "$work/plot"
$CC $flags tests/test_axes3.c tests/host_gint.c src/plot.c src/expr.c -lm -o "$work/axes3"
$CC $flags tests/test_plot2d.c src/plot2d.c src/expr.c -lm -o "$work/plot2d"
$CC $flags tests/test_voxel.c tests/host_gint.c src/voxel.c src/expr.c -lm -o "$work/voxel"
$CC $flags tests/test_ray.c tests/host_gint.c src/ray.c src/expr.c -lm -o "$work/ray"
$CC $flags tests/test_fractal.c src/fractal.c src/expr.c -lm -o "$work/fractal"
$CC $flags tests/test_navigation.c tests/host_gint.c src/navigation.c src/navigation_input.c src/storage.c -lm -o "$work/navigation"
$CC $flags tests/test_storage.c src/storage.c -lm -o "$work/storage"
$CC $flags tests/test_storage_failures.c src/storage.c -Wl,--wrap=write -lm -o "$work/storage_failures"
$CC $flags -Dmain=graphlab_main -c src/main.c -o "$work/ui_main.o"
$CC $flags -Dmain=graphlab_main -DGL_EDITOR_TEST -DGL_EDITOR_TRACE -c src/main.c -o "$work/editor_main.o"
$CC $flags tests/test_editor_full.c tests/host_gint.c src/input.c src/navigation.c src/navigation_input.c src/expr.c src/layout.c src/editor_meta.c src/plot.c src/plot2d.c src/ray.c src/voxel.c src/fractal.c src/storage.c "$work/editor_main.o" -lm -o "$work/editor_full"
$CC $flags tests/test_normalize.c tests/host_gint.c src/input.c src/navigation.c src/navigation_input.c src/expr.c src/layout.c src/editor_meta.c src/plot.c src/plot2d.c src/ray.c src/voxel.c src/fractal.c src/storage.c "$work/editor_main.o" -lm -o "$work/normalize"
$CC $flags tests/test_editor_fuzz.c tests/host_gint.c src/input.c src/navigation.c src/navigation_input.c src/expr.c src/layout.c src/editor_meta.c src/plot.c src/plot2d.c src/ray.c src/voxel.c src/fractal.c src/storage.c "$work/editor_main.o" -lm -o "$work/editor_fuzz"
$CC $flags tests/test_ui.c tests/host_gint.c src/input.c src/navigation.c src/navigation_input.c src/expr.c src/layout.c src/editor_meta.c src/plot.c src/plot2d.c src/ray.c src/voxel.c src/fractal.c src/storage.c "$work/ui_main.o" -lm -o "$work/ui"
$CC $flags tests/test_ui_pixels.c tests/host_gint.c src/input.c src/navigation.c src/navigation_input.c src/expr.c src/layout.c src/editor_meta.c src/plot.c src/plot2d.c src/ray.c src/voxel.c src/fractal.c src/storage.c "$work/ui_main.o" -lm -o "$work/ui_pixels"
for name in expr fuzz layout editor_navigation editor_meta editor_full editor_fuzz normalize fuzz_layout plot axes3 plot2d voxel ray fractal navigation storage storage_failures ui ui_pixels; do "$work/$name"; done
