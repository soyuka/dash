#!/usr/bin/env bash
# Starts dash with the labwc session (Raspberry Pi OS / Debian trixie desktop).
set -euo pipefail

dash=${1:-/opt/opendsh/bin/dash}
autostart=${XDG_CONFIG_HOME:-$HOME/.config}/labwc/autostart

[[ -x $dash ]] || { echo "usage: $0 [path to dash] (no executable at $dash)" >&2; exit 1; }

mkdir -p "$(dirname "$autostart")"
if grep -qs -- "$dash" "$autostart"; then
    echo "already in $autostart"
else
    echo "QT_QPA_PLATFORM=wayland $dash &" >> "$autostart"
    echo "added to $autostart"
fi
