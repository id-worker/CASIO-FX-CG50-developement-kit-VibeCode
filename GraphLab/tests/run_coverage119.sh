#!/bin/sh
# Host line/branch coverage with distinct main.c builds; not target coverage.
set -eu
cd "$(dirname "$0")/.."
SRC=$(pwd);export SRC
out=${1:-"$SRC/coverage-report"}
mkdir -p "$out";out=$(cd "$out" && pwd)
COV_WORK=$(mktemp -d /tmp/gl119-gcov-XXXXXX);export COV_WORK
trap 'rm -rf "$COV_WORK"' EXIT HUP INT TERM
python3 - "$COV_WORK/runner.sh" <<'PY'
from pathlib import Path
import sys
s=Path('tests/run_host_112.sh').read_text()
s=s.replace('cd "$(dirname "$0")/.."','cd "$SRC"',1)
s=s.replace('work="${TMPDIR:-/tmp}/graphlab-host112-$$";trap \'rm -rf "$work"\' EXIT HUP INT TERM','work="$COV_WORK"',1)
s=s.replace("-g -O1'","-g -O0 --coverage'",1)
s=s.replace('${SANITIZE:-1}','${SANITIZE:-0}',1)
assert 'work="$COV_WORK"' in s and '--coverage' in s
Path(sys.argv[1]).write_text(s)
PY
sh "$COV_WORK/runner.sh" > "$out/suite.log" 2>&1
for mod in input navigation navigation_input expr layout editor_meta plot plot2d ray trigfast preview3d refine intersection fractal scroll storage ui_main editor_main memory_main; do
    src=$mod;case $mod in *_main) src=main;;esac
    mkdir -p "$out/$mod"
    gcov -b -c -o "$COV_WORK/$mod.gcno" "src/$src.c" > "$out/$mod/summary.txt"
    # gcov can also emit reports for inline headers, such as camera_near.h.
    # Store those with this object rather than leaving artifacts in Source.
    for report in ./*.gcov; do
        [ -f "$report" ] || continue
        mv "$report" "$out/$mod/"
    done
done
printf 'Coverage: %s\nmain.c is built with different flags for ui_main, editor_main and memory_main. Do not add their percentages.\n' "$out"
