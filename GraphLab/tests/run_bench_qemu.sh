#!/bin/sh
# SH QEMU wall-time comparisons only: not a physical calculator benchmark.
set -eu
cd "$(dirname "$0")/.."
for tool in sh-elf-gcc qemu-sh4eb python3; do command -v "$tool" >/dev/null || { echo "Missing $tool in PATH" >&2; exit 1; }; done
work=$(mktemp -d "${TMPDIR:-/tmp}/gl102-bench-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cflags='-m4-nofpu -mb -DFXCG50 -fno-builtin -Os -ffunction-sections -fdata-sections'
for module in expr ray trigfast voxel preview3d; do sh-elf-gcc $cflags -c "src/$module.c" -o "$work/$module.o"; done
sh-elf-gcc $cflags -c tests/target_start.S -o "$work/start.o"
for mode in voxel12 voxel20 voxel32 ray12 ray20 ray32; do
    case "$mode" in voxel*) type=0;n=${mode#voxel};; ray*) type=1;n=${mode#ray};; esac
    sh-elf-gcc $cflags -DBENCH_N="$n" -DBENCH_TYPE="$type" -c tests/target_bench.c -o "$work/bench.o"
    sh-elf-gcc -m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments \
        "$work/start.o" "$work/bench.o" "$work/expr.o" "$work/ray.o" "$work/trigfast.o" "$work/voxel.o" "$work/preview3d.o" \
        -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$work/$mode.elf"
done
python3 - "$work" <<'PY'
import pathlib, statistics, subprocess, sys, time
work=pathlib.Path(sys.argv[1])
print('SH QEMU wall time: same sphere and camera; voxel and ray have different image quality, not physical calculator times.')
for name in ('voxel12','voxel20','voxel32','ray12','ray20','ray32'):
    times=[]
    for i in range(5):
        before=time.perf_counter()
        subprocess.run(['qemu-sh4eb',str(work/(name+'.elf'))],check=True,timeout=120)
        times.append(time.perf_counter()-before)
    print(f'{name:8} median={statistics.median(times):.3f} s (5 runs)')
PY
