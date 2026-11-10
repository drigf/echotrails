#!/bin/bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
bundle="$project_dir/build/products/Release/EchoTrails.plugin"
destination="${1:-/Applications/Adobe After Effects 2026/Plug-ins}"
if [[ ! -d "$bundle" ]]; then
    echo 'Build first: cmake --preset xcode && cmake --build --preset release' >&2
    exit 1
fi
if [[ ! -d "$destination" ]]; then
    echo "Plug-ins directory does not exist: $destination" >&2
    exit 1
fi
if /usr/bin/pgrep -f '/Adobe After Effects [0-9]+.app/Contents/MacOS/' >/dev/null; then
    echo 'Quit After Effects before installing or updating the plug-in.' >&2
    exit 1
fi
if [[ -e "$destination/EchoTrails.plugin" ]]; then
    echo "An EchoTrails.plugin already exists in $destination. Move it out of the Plug-ins folder before updating." >&2
    exit 1
fi
/usr/bin/ditto "$bundle" "$destination/EchoTrails.plugin"
/usr/bin/codesign --verify --strict "$destination/EchoTrails.plugin"
echo "Installed: $destination/EchoTrails.plugin"
echo 'Restart After Effects. Find Effect > Stylize > Echo Trails.'
