#!/bin/sh
# Same formula/camera in independently compiled 1.07 and 1.08; host wall-time
# for SH QEMU only. It does not model the Casio OS, screen or physical timing.
set -eu
cd "$(dirname "$0")/.."
newroot=$(pwd)
oldroot=${PREVIOUS_ROOT:-"$(dirname "$newroot")/GraphLab_fxCG50_v1.07"}
command -v sh-elf-gcc >/dev/null
command -v qemu-sh4eb >/dev/null
test -f "$oldroot/src/ray.c" || { echo 'Set PREVIOUS_ROOT to the previous source directory.' >&2; exit 1; }
work=$(mktemp -d "${TMPDIR:-/tmp}/gl108-bench-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
flags='-m4-nofpu -mb -DFXCG50 -fno-builtin -Os -ffunction-sections -fdata-sections'
for variant in old new; do
    root=$newroot; [ "$variant" = old ] && root=$oldroot
    mkdir "$work/$variant"
    sh-elf-gcc $flags -I"$root/src" -c tests/target_start.S -o "$work/$variant/start.o"
    sh-elf-gcc $flags -I"$root/src" -c tests/target_bench_user.c -o "$work/$variant/bench.o"
    for m in expr ray preview3d; do sh-elf-gcc $flags -c "$root/src/$m.c" -o "$work/$variant/$m.o"; done
    if [ -f "$root/src/trigfast.c" ]; then sh-elf-gcc $flags -c "$root/src/trigfast.c" -o "$work/$variant/trigfast.o"; fi
    sh-elf-gcc -m4-nofpu -mb -nostdlib -Wl,-e,_start,-Ttext,0x10000,--gc-sections,-z,max-page-size=0x1000,--no-warn-rwx-segments \
      "$work/$variant"/*.o -Wl,--start-group -lc -lopenlibm -lgcc -Wl,--end-group -o "$work/$variant.elf"
done
python3 - "$work" <<'PY'
import pathlib,statistics,subprocess,sys,time
root=pathlib.Path(sys.argv[1]); results={'old':[],'new':[]}
for repeat in range(5):
    for name in (('old','new') if repeat%2==0 else ('new','old')):
        start=time.monotonic();subprocess.run(['qemu-sh4eb',str(root/(name+'.elf'))],check=True,timeout=180)
        results[name].append(time.monotonic()-start)
for name in results:print(name, 'runs', ', '.join(f'{x:.3f}' for x in results[name]),'median',f'{statistics.median(results[name]):.3f}')
print('Host wall seconds, Q12, sin(x)+sin(y)+sin(z)=0, SH QEMU, includes point capture; not physical Casio timing.')
PY
