#!/bin/sh
# Native integration tests of the actual add-in C logic and dictionary.
# Requires a C compiler; frame dumps are temporary and are not the deliverable.
set -eu
cd "$(dirname "$0")"
mkdir -p frames
CC=${CC:-gcc}
FLAGS="-std=gnu99 -O1 -I. -I../../src -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -DMINIZ_NO_ZLIB_APIS -DMINIZ_NO_MALLOC -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ARCHIVE_WRITING_APIS"
$CC $FLAGS harness.c host_stubs.c ../../src/miniz.c ../../src/miniz_tinfl.c -o hosttest
if [ -f ../../dicts/TDIC.DAT ]; then
  TDIC_PATH="$(cd ../../dicts && pwd)/TDIC.DAT"
elif [ -f ../../../TDIC.DAT ]; then
  TDIC_PATH="$(cd ../../.. && pwd)/TDIC.DAT"
else
  echo "TDIC.DAT not found; pass the installation dictionary or build it." >&2
  exit 1
fi
export TDIC_PATH
./hosttest
fixture=$(mktemp "${TMPDIR:-/tmp}/translator-fixture-XXXXXX.DAT")
trap 'rm -f "$fixture"' EXIT HUP INT TERM
python3 make_fixture.py "$fixture"
TDIC_PATH="$fixture" TDIC_SYNTHETIC=1 ./hosttest
