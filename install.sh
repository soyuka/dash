#!/usr/bin/env bash
# Native setup on Debian 13 (trixie), e.g. directly on a Raspberry Pi.
# Everything comes from apt; aasdk, openauto and dash are built from the
# sibling checkouts (../aasdk, ../openauto, this repository).
#
# Cross-building on a PC and deploying to the Pi is usually faster:
# see docker/deploy.sh.
set -euo pipefail

src=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root=$(dirname "$src")
prefix=/opt/opendsh
build_type=Release

usage() {
    cat >&2 <<USAGE
usage: $0 [--runtime] [--build-deps] [--build] [--prefix DIR] [--debug] [--backlight-udev]

  --runtime         apt-get install the packages needed to run dash
  --build-deps      apt-get install the packages needed to build aasdk, openauto, dash
  --build           build and install $root/{aasdk,openauto,dash} into the prefix
  --prefix DIR      install prefix for --build (default $prefix)
  --debug           Debug build
  --backlight-udev  udev rule letting users write /sys/class/backlight/*/brightness

Without options: --runtime --build-deps --build.
USAGE
    exit 1
}

build_deps=(
    build-essential cmake ninja-build pkgconf git ca-certificates
    libboost-dev libboost-system-dev libboost-log-dev
    libprotobuf-dev protobuf-compiler libusb-1.0-0-dev libssl-dev
    qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-multimedia-dev
    qt6-connectivity-dev qt6-serialbus-dev qt6-serialport-dev qt6-websockets-dev
    qt6-svg-dev qt6-wayland-dev libkf6bluezqt-dev
    libglib2.0-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
    libgstreamer-plugins-bad1.0-dev librtaudio-dev
)

runtime=0 deps=0 build=0 udev=0
if [[ $# -eq 0 ]]; then
    runtime=1 deps=1 build=1
fi
while [[ $# -gt 0 ]]; do
    case $1 in
        --runtime) runtime=1 ;;
        --build-deps) deps=1 ;;
        --build) build=1 ;;
        --prefix) prefix=$2; shift ;;
        --debug) build_type=Debug ;;
        --backlight-udev) udev=1 ;;
        *) usage ;;
    esac
    shift
done

. /etc/os-release
if [[ ${ID:-} != debian || ${VERSION_CODENAME:-} != trixie ]]; then
    echo "warning: written for Debian trixie, this is ${PRETTY_NAME:-unknown}" >&2
fi

packages=()
[[ $runtime == 1 ]] && mapfile -t -O "${#packages[@]}" packages < <(grep -v '^#' "$src/docker/runtime-packages.txt")
[[ $deps == 1 ]] && packages+=("${build_deps[@]}")
if [[ ${#packages[@]} -gt 0 ]]; then
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends "${packages[@]}"
fi

if [[ $udev == 1 ]]; then
    echo 'SUBSYSTEM=="backlight", RUN+="/bin/chmod 666 /sys/class/backlight/%k/brightness"' \
        | sudo tee /etc/udev/rules.d/52-dashbrightness.rules >/dev/null
    sudo udevadm control --reload-rules
fi

if [[ $build == 1 ]]; then
    for repo in aasdk openauto dash; do
        [[ -f $root/$repo/CMakeLists.txt ]] || { echo "missing $root/$repo" >&2; exit 1; }
        cmake -S "$root/$repo" -B "$root/_build/native/$repo" -G Ninja \
            -DCMAKE_BUILD_TYPE="$build_type" \
            -DCMAKE_INSTALL_PREFIX="$prefix" \
            -DCMAKE_PREFIX_PATH="$prefix"
        cmake --build "$root/_build/native/$repo" --parallel "$(( $(nproc) > 2 ? $(nproc) - 1 : 1 ))"
        sudo cmake --install "$root/_build/native/$repo"
    done
    echo "$prefix/lib" | sudo tee /etc/ld.so.conf.d/opendsh.conf >/dev/null
    sudo ldconfig
    echo "installed: $prefix/bin/dash"
fi
