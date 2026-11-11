#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
. ./boot/limine.lock
archive=.cache/limine.tar.gz
mkdir -p .cache
if [ ! -f "$archive" ]; then
    trap 'rm -f "$archive.tmp"' EXIT HUP INT TERM
    curl --fail --location --show-error --proto '=https' --tlsv1.2 \
        "https://codeload.github.com/limine-bootloader/limine/tar.gz/$LIMINE_COMMIT" \
        -o "$archive.tmp"
    printf '%s  %s\n' "$LIMINE_SHA256" "$archive.tmp" | sha256sum -c -
    mv "$archive.tmp" "$archive"
fi
printf '%s  %s\n' "$LIMINE_SHA256" "$archive" | sha256sum -c -
# Always re-extract verified assets; do not trust an old extracted tree.
rm -rf .cache/limine
mkdir .cache/limine
tar -xzf "$archive" --strip-components=1 -C .cache/limine
make -C .cache/limine CC=cc
