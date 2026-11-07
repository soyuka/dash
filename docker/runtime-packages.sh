#!/usr/bin/env bash
# Prints the Debian packages needed to run a staged arm64 tree: the owners of
# every shared library its ELF files link (NEEDED, resolved with dpkg -S in
# the arm64 build image), plus what Qt, GStreamer and dash load at run time.
set -euo pipefail

docker_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
stage=$(realpath "${1:-$docker_dir/../../_stage/arm64}")
image=opendsh-build:arm64-cross

[[ -d $stage/lib ]] || { echo "no staged tree at $stage (run docker/build.sh arm64 all)" >&2; exit 1; }

if [[ ${REBUILD_IMAGE:-0} == 1 ]] || ! docker image inspect "$image" >/dev/null 2>&1; then
    docker build -q -t "$image" -f "$docker_dir/Dockerfile.arm64-cross" "$docker_dir" >/dev/null
fi

# Loaded with dlopen or as plugins, so absent from NEEDED.
runtime_only=(
    qt6-wayland                 # Wayland QPA plugin
    qt6-svg-plugins             # SVG icon engine
    qml6-module-qtquick         # Android Auto video scene
    qml6-module-qtmultimedia    # VideoOutput
    gstreamer1.0-plugins-base   # appsrc, appsink, videoconvert
    gstreamer1.0-plugins-good
    gstreamer1.0-plugins-bad    # h264parse
    gstreamer1.0-libav          # avdec_h264
    libsocketcan2               # optional, lets the socketcan plugin configure bitrate
    pipewire pipewire-pulse pipewire-alsa wireplumber   # audio, wpctl for the volume
    bluez
)

docker run --rm -v "$stage:/stage:ro" "$image" bash -euo pipefail -c '
    mapfile -t elfs < <(find /stage -type f ! -path "*/cmake/*" ! -path "/stage/include/*" \
        -exec sh -c "file -b \"\$1\" | grep -q ^ELF" _ {} \; -print)
    dyn() { readelf -d "$1" | sed -n "s/.*($2).*\[\(.*\)\]/\1/p"; }
    provided=$(for f in "${elfs[@]}"; do dyn "$f" SONAME; done | sort -u)
    needed=$(for f in "${elfs[@]}"; do dyn "$f" NEEDED; done | sort -u)
    for so in $(comm -23 <(echo "$needed") <(echo "$provided")); do
        owner=$(dpkg -S "*/aarch64-linux-gnu/$so" 2>/dev/null | grep ":arm64:" | head -1 | cut -d: -f1)
        if [[ -z $owner ]]; then
            echo "unresolved: $so" >&2
            exit 1
        fi
        echo "$owner"
    done
' | cat - <(printf '%s\n' "${runtime_only[@]}" | sed 's/ *#.*//' | tr ' ' '\n') | sed '/^$/d' | sort -u
