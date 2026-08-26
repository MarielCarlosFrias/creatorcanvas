#!/usr/bin/env bash
# Builds a Linux AppImage. Usage: ./packaging/linux/build_appimage.sh [build-dir]
set -euo pipefail

BUILD_DIR="${1:-build/linux-release}"
APPDIR="$(pwd)/packaging/AppDir"

cmake --build "$BUILD_DIR" --target creatorcanvas

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp "$BUILD_DIR/bin/CreatorCanvas" "$APPDIR/usr/bin/"
cp packaging/linux/creatorcanvas.desktop \
   "$APPDIR/usr/share/applications/creatorcanvas.desktop"

# Placeholder icon (replace with a real PNG when design lands)
if [ ! -f packaging/linux/creatorcanvas.png ]; then
    convert -size 256x256 xc:'#2f6fed' \
        -fill white -gravity center -pointsize 40 -annotate 0 'CC' \
        packaging/linux/creatorcanvas.png 2>/dev/null \
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
cp packaging/linux/creatorcanvas.png \
   "$APPDIR/usr/share/icons/hicolor/256x256/apps/creatorcanvas.png"
cp packaging/linux/creatorcanvas.desktop "$APPDIR/creatorcanvas.desktop"
cp packaging/linux/creatorcanvas.png "$APPDIR/creatorcanvas.png"
cp packaging/linux/AppRun.sh "$APPDIR/AppRun"

# Bundle Qt libraries
if command -v linuxdeployqt >/dev/null 2>&1; then
    linuxdeployqt "$APPDIR/creatorcanvas.desktop" -appimage
    echo ">> AppImage: $(ls -1 CreatorCanvas*.AppImage)"
else
    echo ">> linuxdeployqt não encontrado."
    echo "   Instale: https://github.com/probonopd/linuxdeployqt/releases"
    echo "   Ou use o pacote DEB (packaging/linux/build_deb.sh)."
fi
