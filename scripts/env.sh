#!/bin/sh
# Use the optional rootless cloud toolchain without changing the caller's shell.
set -eu
repo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
default_tools=$(dirname "$repo")/.agenticos-tools
# Reuse the verified Week 1 tools when present; no reinstall for a brand change.
if [ ! -d "$default_tools" ] && [ -d "$(dirname "$repo")/.astra-tools" ]; then
    default_tools=$(dirname "$repo")/.astra-tools
fi
AGENTICOS_TOOLS=${AGENTICOS_TOOLS:-$default_tools}
if [ -d "$AGENTICOS_TOOLS/root/usr/bin" ]; then
    export AGENTICOS_SYSROOT="$AGENTICOS_TOOLS/root"
    export PATH="$AGENTICOS_SYSROOT/usr/bin:$PATH"
    export LD_LIBRARY_PATH="$AGENTICOS_SYSROOT/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
exec "$@"
