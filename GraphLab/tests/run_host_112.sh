#!/bin/sh
# Functional host tests (not a calculator OS or hardware emulator).
set -eu
cd "$(dirname "$0")/.."
CC=${CC:-cc};work="${TMPDIR:-/tmp}/graphlab-host112-$$";trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work"
flags='-std=c11 -DGL_HOST -Wall -Wextra -Werror -Wpedantic -g -O1'
if [ "${SANITIZE:-1}" = 1 ]; then flags="$flags -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie"; fi
for mod in input navigation navigation_input expr layout editor_meta plot plot2d ray trigfast preview3d refine intersection fractal scroll storage;do
    $CC $flags -c "src/$mod.c" -o "$work/$mod.o"
done
$CC $flags -c tests/host_gint.c -o "$work/host_gint.o"
ar rcs "$work/libbase.a" "$work"/*.o
$CC $flags -Dmain=graphlab_main -c src/main.c -o "$work/ui_main.o"
$CC $flags -Dmain=graphlab_main -DGL_EDITOR_TEST -DGL_EDITOR_TRACE -c src/main.c -o "$work/editor_main.o"
$CC $flags -Dmain=graphlab_main -DGL_EDITOR_TEST -c src/main.c -o "$work/memory_main.o"
# Include all modules as an archive; the linker extracts only those each test uses.
compile(){ name=$1;file=$2;main=$3;wrap=$4; $CC $flags "$file" $main -Wl,--start-group "$work/libbase.a" -Wl,--end-group $wrap -lm -o "$work/$name"; }
compile expr tests/test_expr.c '' ''
compile math_audit119 tests/test_math_audit119.c '' ''
compile period_audit119 tests/test_period_audit119.c '' ''
compile expr_fast113 tests/test_expr_fast113.c '' ''
compile fuzz tests/fuzz_expr.c '' ''
compile layout tests/test_layout.c '' ''
compile editor_navigation tests/test_editor_navigation.c '' ''
compile editor_meta tests/test_editor_meta.c '' ''
compile editor_full tests/test_editor_full.c "$work/editor_main.o" ''
compile editor_delete119 tests/test_editor_delete119.c "$work/editor_main.o" ''
compile editor_fuzz tests/test_editor_fuzz.c "$work/editor_main.o" ''
compile long_editor tests/test_long_editor.c "$work/editor_main.o" ''
compile editor_memory tests/test_editor_memory.c "$work/memory_main.o" '-Wl,--wrap=malloc,--wrap=free'
compile normalize tests/test_normalize.c "$work/editor_main.o" ''
compile fuzz_layout tests/fuzz_layout.c '' ''
compile plot tests/test_plot.c '' ''
compile axes3 tests/test_axes3.c '' ''
compile plot2d tests/test_plot2d.c '' ''
compile ray tests/test_ray.c '' ''
compile ray_volume120 tests/test_ray_volume120.c '' ''
compile ray_optimization tests/test_ray_optimization.c '' ''
compile ray_small119 tests/test_ray_small119.c '' ''
compile fractal tests/test_fractal.c '' ''
compile scroll tests/test_scroll.c '' ''
compile fractal_pan tests/test_fractal_pan.c '' ''
compile preview3d tests/test_preview3d.c '' ''
compile preview_quality tests/test_preview_quality112.c '' ''
compile math108 tests/test_108_math.c '' ''
compile quality tests/test_quality112.c '' '-Wl,--wrap=write'
compile navigation tests/test_navigation.c '' ''
compile storage tests/test_storage112.c '' ''
compile storage_failures tests/test_storage_failures112.c '' '-Wl,--wrap=write'
compile ui tests/test_ui.c "$work/ui_main.o" ''
compile ui_audit119 tests/test_ui_audit119.c "$work/ui_main.o" ''
compile ui_pixels tests/test_ui_pixels.c "$work/ui_main.o" ''
compile ui_pan tests/test_ui_pan.c "$work/ui_main.o" ''
compile intersection tests/test_intersection112.c '' ''
compile shapes tests/test_map_shapes112.c '' ''
compile planes tests/test_map_planes112.c '' ''
compile map_memory tests/test_map_memory112.c '' '-Wl,--wrap=malloc'
compile map_ui tests/test_map_ui112.c "$work/ui_main.o" ''
compile low_memory tests/test_low_memory112.c "$work/ui_main.o" '-Wl,--wrap=malloc'
compile entry tests/test_equation_entry112.c "$work/ui_main.o" ''
compile parameter_entry tests/test_parameter_entry1121.c "$work/ui_main.o" ''
compile pixel116 tests/test_pixel_intersection116.c '' ''
compile height116 tests/test_height_map116.c '' ''
compile fast_ray116 tests/test_fast_ray116.c '' ''
compile refine_ray118 tests/test_refine_ray118.c '' ''
compile final_stream119 tests/test_final_stream119.c '' ''
compile refine_ui118 tests/test_refine_ui118.c "$work/ui_main.o" ''
compile refine_memory118 tests/test_refine_memory118.c '' '-Wl,--wrap=calloc'
compile pixel_ui116 tests/test_pixel_ui116.c "$work/ui_main.o" ''
compile settings_fast116 tests/test_settings_fast116.c "$work/ui_main.o" ''
for name in expr math_audit119 period_audit119 expr_fast113 fuzz layout editor_navigation editor_meta editor_full editor_delete119 editor_fuzz long_editor editor_memory normalize fuzz_layout plot axes3 plot2d ray ray_volume120 ray_optimization ray_small119 fractal scroll fractal_pan preview3d preview_quality math108 quality navigation storage storage_failures ui ui_audit119 ui_pixels ui_pan intersection shapes planes map_memory map_ui low_memory entry parameter_entry pixel116 height116 fast_ray116 refine_ray118 final_stream119 refine_ui118 refine_memory118 pixel_ui116 settings_fast116; do echo "--- $name"; "$work/$name"; done
