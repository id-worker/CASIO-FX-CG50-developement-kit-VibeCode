#!/bin/sh
# Compatibility entry point: use the current full SuperH suite.
set -eu
exec sh "$(dirname "$0")/run_target_112.sh" "$@"
