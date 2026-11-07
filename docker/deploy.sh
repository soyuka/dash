#!/usr/bin/env bash
# Deploys aasdk + openauto + dash, cross-built for arm64, to a Raspberry Pi
# running Debian trixie, under /opt/opendsh.
set -euo pipefail

prefix=/opt/opendsh

usage() {
    cat >&2 <<USAGE
usage: $0 [--no-build] [--install-packages] [--stage DIR] <[user@]host>

  Builds aasdk, openauto and dash for arm64 with docker/build.sh (unless
  --no-build), copies the staged bin/ and lib/ to $prefix on the host,
  registers $prefix/lib with the dynamic linker and prints the runtime
  packages derived from the binaries (installed with --install-packages).

  --no-build          deploy the existing staged tree as is
  --install-packages  apt-get install the runtime packages on the host
  --stage DIR         staged tree to deploy (default: _stage/arm64)

  Run dash on the host:   $prefix/bin/dash   (QT_QPA_PLATFORM=wayland under labwc)
  Remove it:              sudo rm -rf $prefix /etc/ld.so.conf.d/opendsh.conf && sudo ldconfig
USAGE
    exit 1
}

docker_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
stage=$docker_dir/../../_stage/arm64
build=1
install_packages=0
host=

while [[ $# -gt 0 ]]; do
    case $1 in
        --no-build) build=0 ;;
        --install-packages) install_packages=1 ;;
        --stage) stage=$2; shift ;;
        -h|--help) usage ;;
        -*) usage ;;
        *) [[ -z $host ]] || usage; host=$1 ;;
    esac
    shift
done
[[ -n $host ]] || usage

if [[ $build == 1 ]]; then
    "$docker_dir/build.sh" arm64 all
fi
stage=$(realpath "$stage")
[[ -x $stage/bin/dash ]] || { echo "no dash in $stage/bin, build first" >&2; exit 1; }
file -b "$stage/bin/dash" | grep -q aarch64 || { echo "$stage/bin/dash is not an arm64 binary" >&2; exit 1; }

packages=$("$docker_dir/runtime-packages.sh" "$stage")

# Copy as the ssh user first, then install with sudo in one interactive session
# so that a sudo password prompt works.
upload=.cache/opendsh-deploy
echo "==> copying $stage to $host:~/$upload"
ssh "$host" "mkdir -p ~/$upload"
rsync -a --delete --info=stats1 --exclude=lib/cmake/ "$stage/bin" "$stage/lib" "$host:$upload/"

# openauto and aasdk do not carry an $ORIGIN runpath, hence the ld.so.conf entry
echo "==> installing to $prefix"
ssh -t "$host" "sudo mkdir -p $prefix \
    && sudo rsync -a --delete --chown=root:root ~/$upload/ $prefix/ \
    && echo $prefix/lib | sudo tee /etc/ld.so.conf.d/opendsh.conf >/dev/null \
    && sudo ldconfig"

echo "==> runtime packages"
echo "$packages" | tr '\n' ' '; echo
if [[ $install_packages == 1 ]]; then
    ssh -t "$host" "sudo apt-get update && sudo apt-get install -y --no-install-recommends $(echo "$packages" | tr '\n' ' ')"
else
    echo "install them with: ssh $host sudo apt-get install --no-install-recommends $(echo "$packages" | tr '\n' ' ')"
fi
