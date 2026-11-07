#!/bin/sh
# Non-root setup for the managed Debian 13 cloud image used for this project.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tools=/workspace/.tns-tools
prefix=/workspace/.tns-sysroot
cmake_bin="$tools/cmake/data/bin/cmake"
if [ ! -x "$cmake_bin" ]; then
    python3 -m pip install --no-cache-dir --target "$tools" cmake==4.4.4 ninja==1.13.2
fi
if [ ! -f "$prefix/.tns-deps-ready" ]; then
    . /etc/os-release
    [ "$ID" = debian ] && [ "$VERSION_CODENAME" = trixie ] || {
        echo 'This non-root bootstrap targets Debian 13; see README for standard Linux/MSYS2 setup.' >&2
        exit 1
    }
    tmp=$(mktemp -d /tmp/tns-setup.XXXXXX)
    trap 'rm -rf "$tmp"' EXIT HUP INT TERM
    mkdir -p "$tmp/lists/partial" "$tmp/cache/archives/partial" "$tmp/packages" "$prefix"
    cat > "$tmp/debian.sources" <<'SOURCES'
Types: deb
URIs: https://deb.debian.org/debian
Suites: trixie
Components: main
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg
SOURCES
    apt_user() {
        /usr/bin/apt-get -o Dir::Etc::main=/dev/null -o Dir::Etc::parts=- \
          -o Dir::Etc::sourcelist="$tmp/debian.sources" -o Dir::Etc::sourceparts=- \
          -o Dir::State::lists="$tmp/lists" -o Dir::Cache="$tmp/cache" "$@"
    }
    apt_user update
    (cd "$tmp/packages" && apt_user download libsdl2-dev libx11-dev libxext-dev \
      libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxrender-dev libxss-dev \
      libxxf86vm-dev libxinerama-dev x11proto-dev xtrans-dev libxau-dev libxdmcp-dev libxcb1-dev)
    for package in "$tmp"/packages/*.deb; do dpkg-deb -x "$package" "$prefix"; done
    touch "$prefix/.tns-deps-ready"
fi
# Debian dev-package linker names refer to runtime libraries already in the cloud image.
python3 - "$prefix" <<'PY'
from pathlib import Path
import sys
lib = Path(sys.argv[1]) / 'usr/lib/x86_64-linux-gnu'
for link in lib.glob('*.so'):
    if link.is_symlink() and not link.exists():
        actual = Path('/lib/x86_64-linux-gnu') / link.readlink().name
        if actual.exists():
            link.unlink()
            link.symlink_to(actual)
PY
"$cmake_bin" -S "$repo" -B "$repo/build" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$prefix/usr" \
  -DCMAKE_CXX_FLAGS="-I$prefix/usr/include/x86_64-linux-gnu"
"$cmake_bin" --build "$repo/build" -j2
"$tools/cmake/data/bin/ctest" --test-dir "$repo/build" --output-on-failure
