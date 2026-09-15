#!/bin/bash
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${HERE}/usr/lib/creatorcanvas:${LD_LIBRARY_PATH:-}"
export XDG_DATA_DIRS="${HERE}/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
if [ -d "${HERE}/usr/plugins" ]; then
    export QT_PLUGIN_PATH="${HERE}/usr/plugins:${QT_PLUGIN_PATH:-}"
    export QT_QPA_PLATFORM_PLUGIN_PATH="${HERE}/usr/plugins/platforms:${QT_QPA_PLATFORM_PLUGIN_PATH:-}"
fi

BIN="${HERE}/usr/bin/creatorcanvas"
if [ ! -x "$BIN" ]; then
    BIN="${HERE}/usr/bin/CreatorCanvas"
fi

exec "$BIN" "$@"
