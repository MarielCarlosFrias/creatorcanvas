#!/usr/bin/env bash
# Builds a Linux AppImage. Usage: ./packaging/linux/build_appimage.sh [build-dir]
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-$PROJECT_DIR/build/linux-release}"
APPDIR="$PROJECT_DIR/packaging/AppDir"
STAGING="$BUILD_DIR/staging"

echo "==> Staging CreatorCanvas for AppImage from: $BUILD_DIR"

cmake --build "$BUILD_DIR" --target stage --config Release

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib/creatorcanvas"
mkdir -p "$APPDIR/usr/share/creatorcanvas"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/metainfo"
mkdir -p "$APPDIR/usr/share/icons/hicolor/scalable/apps"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

# 1. Executable
if [ -f "$STAGING/bin/creatorcanvas" ]; then
    install -m 0755 "$STAGING/bin/creatorcanvas" "$APPDIR/usr/bin/creatorcanvas"
else
    install -m 0755 "$BUILD_DIR/bin/CreatorCanvas" "$APPDIR/usr/bin/creatorcanvas"
fi
ln -sf creatorcanvas "$APPDIR/usr/bin/CreatorCanvas"

# 2. ONNX Runtime & dependencies
if [ -d "$STAGING/lib/creatorcanvas" ]; then
    cp -d -r "$STAGING/lib/creatorcanvas/." "$APPDIR/usr/lib/creatorcanvas/"
fi

# 3. Share assets, Models & Presets
if [ -d "$STAGING/share/creatorcanvas" ]; then
    cp -r "$STAGING/share/creatorcanvas/." "$APPDIR/usr/share/creatorcanvas/"
fi

if [ -d "$PROJECT_DIR/resources/models" ]; then
    mkdir -p "$APPDIR/usr/share/creatorcanvas/models"
    cp -r "$PROJECT_DIR/resources/models/." "$APPDIR/usr/share/creatorcanvas/models/"
fi

if [ -d "$PROJECT_DIR/resources/presets" ]; then
    mkdir -p "$APPDIR/usr/share/creatorcanvas/presets"
    cp -r "$PROJECT_DIR/resources/presets/." "$APPDIR/usr/share/creatorcanvas/presets/"
fi

# 4. Desktop entry & AppStream
install -m 0644 "$PROJECT_DIR/packaging/linux/creatorcanvas.desktop" \
        "$APPDIR/usr/share/applications/creatorcanvas.desktop"
install -m 0644 "$PROJECT_DIR/packaging/linux/creatorcanvas.appdata.xml" \
        "$APPDIR/usr/share/metainfo/creatorcanvas.appdata.xml"
cp "$PROJECT_DIR/packaging/linux/creatorcanvas.desktop" "$APPDIR/creatorcanvas.desktop"

# 5. Icons
install -m 0644 "$PROJECT_DIR/resources/icons/creatorcanvas.svg" \
        "$APPDIR/usr/share/icons/hicolor/scalable/apps/creatorcanvas.svg"
cp "$PROJECT_DIR/resources/icons/creatorcanvas.svg" "$APPDIR/creatorcanvas.svg"

# Raster 256x256 PNG icon
if [ ! -f "$PROJECT_DIR/packaging/linux/creatorcanvas.png" ]; then
    convert -size 256x256 xc:'#2f6fed' \
        -fill white -gravity center -pointsize 40 -annotate 0 'CC' \
        "$PROJECT_DIR/packaging/linux/creatorcanvas.png" 2>/dev/null \
    || python3 - << 'PY'
import struct, zlib
def chunk(t, d):
    c = t + d
    return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c))
w = h = 256
raw = b''.join(b'\x00' + b'\x2f\x6f\xed' * w for _ in range(h))
png = (b'\x89PNG\r\n\x1a\n'
       + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress(raw))
       + chunk(b'IEND', b''))
open('packaging/linux/creatorcanvas.png', 'wb').write(png)
PY
fi
cp "$PROJECT_DIR/packaging/linux/creatorcanvas.png" \
   "$APPDIR/usr/share/icons/hicolor/256x256/apps/creatorcanvas.png"
cp "$PROJECT_DIR/packaging/linux/creatorcanvas.png" "$APPDIR/creatorcanvas.png"
cp "$PROJECT_DIR/packaging/linux/creatorcanvas.png" "$APPDIR/.DirIcon"

# 6. Deploy Qt6 libraries and plugins
echo "==> Deploying Qt6 libraries and plugins into AppDir..."
mkdir -p "$APPDIR/usr/lib"
mkdir -p "$APPDIR/usr/plugins/platforms"
mkdir -p "$APPDIR/usr/plugins/imageformats"
mkdir -p "$APPDIR/usr/plugins/iconengines"
mkdir -p "$APPDIR/usr/plugins/platformthemes"
mkdir -p "$APPDIR/usr/plugins/xcbglintegrations"

QMAKE_BIN=""
for cmd in qmake6 qmake qtpaths6; do
    if command -v "$cmd" >/dev/null 2>&1; then
        QMAKE_BIN="$cmd"
        break
    fi
done

QT_LIBS=""
QT_PLUGINS=""
if [ -n "$QMAKE_BIN" ]; then
    if [ "$QMAKE_BIN" = "qtpaths6" ]; then
        QT_LIBS="$($QMAKE_BIN --variable QT_INSTALL_LIBS 2>/dev/null || true)"
        QT_PLUGINS="$($QMAKE_BIN --variable QT_INSTALL_PLUGINS 2>/dev/null || true)"
    else
        QT_LIBS="$($QMAKE_BIN -query QT_INSTALL_LIBS 2>/dev/null || true)"
        QT_PLUGINS="$($QMAKE_BIN -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
    fi
fi

if [ -z "$QT_LIBS" ] || [ ! -d "$QT_LIBS" ]; then
    for candidate in /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib; do
        if [ -f "$candidate/libQt6Core.so.6" ]; then
            QT_LIBS="$candidate"
            break
        fi
    done
fi

if [ -z "$QT_PLUGINS" ] || [ ! -d "$QT_PLUGINS" ]; then
    for candidate in /usr/lib/x86_64-linux-gnu/qt6/plugins /usr/lib64/qt6/plugins /usr/lib/qt6/plugins; do
        if [ -d "$candidate/platforms" ]; then
            QT_PLUGINS="$candidate"
            break
        fi
    done
fi

if [ -n "$QT_LIBS" ] && [ -d "$QT_LIBS" ]; then
    echo "    Copying Qt6 shared libraries from $QT_LIBS..."
    for lib in libQt6Core libQt6Gui libQt6Widgets libQt6DBus libQt6XcbQpa libQt6Svg libQt6OpenGL libQt6OpenGLWidgets; do
        cp -d "$QT_LIBS"/$lib.so* "$APPDIR/usr/lib/" 2>/dev/null || true
    done
    for lib in libicui18n libicuuc libicudata; do
        cp -d "$QT_LIBS"/$lib.so* "$APPDIR/usr/lib/" 2>/dev/null || true
    done
fi

if [ -n "$QT_PLUGINS" ] && [ -d "$QT_PLUGINS" ]; then
    echo "    Copying Qt6 plugins from $QT_PLUGINS..."
    for plugindir in platforms imageformats iconengines platformthemes xcbglintegrations; do
        if [ -d "$QT_PLUGINS/$plugindir" ]; then
            mkdir -p "$APPDIR/usr/plugins/$plugindir"
            cp -d -r "$QT_PLUGINS/$plugindir/." "$APPDIR/usr/plugins/$plugindir/" 2>/dev/null || true
        fi
    done
fi

# 7. AppRun entrypoint
install -m 0755 "$PROJECT_DIR/packaging/linux/AppRun.sh" "$APPDIR/AppRun"

echo "==> AppDir successfully staged at: $APPDIR"

# 8. Package AppImage if tools are available
VERSION=$(grep -m1 'VERSION' "$PROJECT_DIR/CMakeLists.txt" | tr -cd '0-9.')
OUTPUT_APPIMAGE="$BUILD_DIR/CreatorCanvas-${VERSION}-x86_64.AppImage"

if command -v linuxdeploy >/dev/null 2>&1; then
    echo "==> Running linuxdeploy..."
    LINUXDEPLOY_ARGS=(--appdir "$APPDIR" -d "$APPDIR/creatorcanvas.desktop" -i "$APPDIR/creatorcanvas.svg")
    if command -v linuxdeploy-plugin-qt >/dev/null 2>&1; then
        LINUXDEPLOY_ARGS+=(--plugin qt)
    fi
    if command -v appimagetool >/dev/null 2>&1; then
        LINUXDEPLOY_ARGS+=(--output appimage)
    fi
    linuxdeploy "${LINUXDEPLOY_ARGS[@]}" || true
    if [ -f "$PROJECT_DIR/CreatorCanvas-${VERSION}-x86_64.AppImage" ]; then
        mv "$PROJECT_DIR/CreatorCanvas-${VERSION}-x86_64.AppImage" "$OUTPUT_APPIMAGE"
        echo ">> AppImage created: $OUTPUT_APPIMAGE"
    fi
fi

if [ ! -f "$OUTPUT_APPIMAGE" ] && command -v appimagetool >/dev/null 2>&1; then
    appimagetool "$APPDIR" "$OUTPUT_APPIMAGE"
    echo ">> AppImage created: $OUTPUT_APPIMAGE"
elif [ ! -f "$OUTPUT_APPIMAGE" ] && command -v linuxdeployqt >/dev/null 2>&1; then
    linuxdeployqt "$APPDIR/creatorcanvas.desktop" -appimage
    echo ">> AppImage created with linuxdeployqt."
elif [ ! -f "$OUTPUT_APPIMAGE" ]; then
    echo ">> Informação: AppDir pronto em '$APPDIR' com todas as bibliotecas e plugins Qt."
    echo "   Para gerar o arquivo final único .AppImage, utilize 'appimagetool $APPDIR'."
fi
