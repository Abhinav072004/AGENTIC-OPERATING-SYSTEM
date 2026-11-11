#!/bin/sh
# Rootless Debian 13 amd64 setup. APT validates signed indexes and package hashes.
set -eu
repo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ASTRA_TOOLS=${ASTRA_TOOLS:-$(dirname "$repo")/.astra-tools}
export ASTRA_TOOLS
[ "$(uname -m)" = x86_64 ] || { echo 'Requires an x86_64 host' >&2; exit 1; }
. /etc/os-release
[ "$ID" = debian ] && [ "$VERSION_ID" = 13 ] || {
    echo 'Rootless package extraction is supported on Debian 13 only; see docs/SETUP.md.' >&2
    exit 1
}
case "$ASTRA_TOOLS" in
    /*) ;;
    *) echo 'ASTRA_TOOLS must be an absolute path' >&2; exit 1 ;;
esac
for tool in /usr/bin/apt-get dpkg-deb cc make curl tar sha256sum python3; do
    command -v "$tool" >/dev/null || { echo "Missing prerequisite: $tool" >&2; exit 1; }
done
mkdir -p "$ASTRA_TOOLS/apt/lists/partial" "$ASTRA_TOOLS/apt/archives/partial" \
    "$ASTRA_TOOLS/apt/etc/empty" "$ASTRA_TOOLS/root"
cat > "$ASTRA_TOOLS/apt/etc/sources.list" <<'EOF'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian-security trixie-security main
EOF
cat > "$ASTRA_TOOLS/apt/etc/apt.conf" <<EOF
Dir::Etc::parts "$ASTRA_TOOLS/apt/etc/empty";
Dir::Etc::main "/dev/null";
Dir::Etc::sourcelist "$ASTRA_TOOLS/apt/etc/sources.list";
Dir::Etc::sourceparts "$ASTRA_TOOLS/apt/etc/empty";
Dir::State::lists "$ASTRA_TOOLS/apt/lists";
Dir::Cache::archives "$ASTRA_TOOLS/apt/archives";
Debug::NoLocking "true";
APT::Get::List-Cleanup "false";
EOF
export APT_CONFIG="$ASTRA_TOOLS/apt/etc/apt.conf"
/usr/bin/apt-get update
# Exact versions validated for this milestone. Do not silently substitute versions.
/usr/bin/apt-get -y --download-only --no-install-recommends install \
    clang-19=1:19.1.7-3+b1 lld-19=1:19.1.7-3+b1 nasm=2.16.03-1 \
    qemu-system-x86=1:10.0.13+ds-0+deb13u1 \
    qemu-system-common=1:10.0.13+ds-0+deb13u1 \
    qemu-system-data=1:10.0.13+ds-0+deb13u1 \
    seabios=1.16.3-2 ovmf=2025.02-8+deb13u1 gdb=16.3-1 xorriso=1.5.6-1.2+b1
for package in "$ASTRA_TOOLS"/apt/archives/*.deb; do
    dpkg-deb -x "$package" "$ASTRA_TOOLS/root"
done
"$repo/scripts/env.sh" clang-19 --version
"$repo/scripts/env.sh" qemu-system-x86_64 --version
"$repo/scripts/env.sh" make -C "$repo" deps
