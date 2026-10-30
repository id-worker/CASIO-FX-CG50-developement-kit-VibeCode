#!/bin/sh
# Compatibility entry point: run the current standard 1.20 host suite.
set -eu
exec sh "$(dirname "$0")/run_host_112.sh" "$@"
