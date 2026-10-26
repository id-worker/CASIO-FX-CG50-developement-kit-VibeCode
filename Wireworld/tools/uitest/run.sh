#!/bin/sh
# Runs src/main.c on the PC against the syscall stubs (see host_stub.c).
set -e
cd "$(dirname "$0")/../.."
gcc -std=gnu99 -g -O1 -Wall -I tools/uitest/stub -DHOST_TEST \
    -o /tmp/uitest tools/uitest/ui_test.c tools/uitest/host_stub.c \
    src/wireworld.c src/patterns.c
/tmp/uitest
