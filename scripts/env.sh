#!/bin/sh
# Use the optional rootless cloud toolchain without changing the caller's shell.
set -eu
repo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ASTRA_TOOLS=${ASTRA_TOOLS:-$(dirname "$repo")/.astra-tools}
if [ -d "$ASTRA_TOOLS/root/usr/bin" ]; then
    export ASTRA_SYSROOT="$ASTRA_TOOLS/root"
    export PATH="$ASTRA_SYSROOT/usr/bin:$PATH"
    export LD_LIBRARY_PATH="$ASTRA_SYSROOT/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
exec "$@"
