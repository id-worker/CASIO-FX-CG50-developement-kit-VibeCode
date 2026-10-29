#!/bin/sh
# Runs src/main.c on the PC against the syscall stubs (see host_stub.c).
set -e
cd "$(dirname "$0")/../.."
gcc -std=gnu99 -g -O1 -Wall -I tools/uitest/stub -DHOST_TEST -DWW_TEST_SEMANTIC_COLORS \
    -o /tmp/uitest tools/uitest/ui_test.c tools/uitest/host_stub.c \
    src/wireworld.c src/patterns.c
/tmp/uitest
# The unique-color suite above checks layers; this separate build uses the
# actual release palette and forces startup allocation failure.
gcc -std=gnu99 -g -O1 -Wall -Wextra -Werror -I tools/uitest/stub -DHOST_TEST \
    -o /tmp/ww-low-memory tools/uitest/test_low_memory.c tools/uitest/host_stub.c \
    src/wireworld.c src/patterns.c
/tmp/ww-low-memory
