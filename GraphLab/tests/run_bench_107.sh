#!/bin/sh
# Host wall-time illustration under qemu-sh4eb, NOT physical calculator speed.
set -eu
cd "$(dirname "$0")/.."
for tool in sh-elf-gcc qemu-sh4eb python3; do command -v "$tool" >/dev/null || { echo "Missing $tool" >&2; exit 1; }; done
work=$(mktemp -d "${TMPDIR:-/tmp}/gl107-bench-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
flags='-m4-nofpu -mb -DFXCG50 -fno-builtin -Os -ffunction-sections -fdata-sections'
for module in expr ray trigfast preview3d; do sh-elf-gcc $flags -c "src/$module.c" -o "$work/$module.o"; done
sh-elf-gcc $flags -c tests/target_start.S -o "$work/start.o"
for mode in 0 1 2; do
    sh-elf-gcc $flags -DBENCH_MODE="$mode" -c tests/target_bench_107.c -o "$work/bench.o"
    sh-elf-gcc -m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments \
      "$work/start.o" "$work/bench.o" "$work/expr.o" "$work/ray.o" "$work/trigfast.o" "$work/preview3d.o" \
      -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$work/$mode.elf"
done
python3 - "$work" <<'PY'
import pathlib,statistics,subprocess,sys,time
work=pathlib.Path(sys.argv[1]); medians=[]
for mode in range(3):
    times=[]
    for _ in range(3):
        start=time.perf_counter();subprocess.run(['qemu-sh4eb',str(work/f'{mode}.elf')],check=True,timeout=120)
        times.append(time.perf_counter()-start)
    medians.append(statistics.median(times))
print('Q48 sphere, SH QEMU host wall times, median of 3; no Casio OS/hardware:')
print(f'first final PLOT with point capture: {medians[0]:.3f} s')
print(f'first + 3 cached final PLOTs:       {medians[1]:.3f} s')
print(f'first + 10 point previews:          {medians[2]:.3f} s')
print(f'approx per cached PLOT: {(medians[1]-medians[0])/3:.3f} s; per preview: {(medians[2]-medians[0])/10:.3f} s')
PY
