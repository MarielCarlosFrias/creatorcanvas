#!/usr/bin/env bash
# Builds a .deb package. Usage: ./packaging/linux/build_deb.sh [build-dir]
set -euo pipefail

BUILD_DIR="${1:-build/linux-release}"
VERSION="$(cmake -LA -N "$BUILD_DIR" 2>/dev/null | grep -oP 'CreatorCanvas_VERSION:\w+=\K[0-9.]+' || echo 0.1.0)"
PKGDIR="$(pwd)/packaging/debroot/creatorcanvas_${VERSION}_amd64"

cmake --build "$BUILD_DIR" --target creatorcanvas

rm -rf packaging/debroot
mkdir -p "$PKGDIR/DEBIAN" \
         "$PKGDIR/usr/bin" \
         "$PKGDIR/usr/share/applications" \
         "$PKGDIR/usr/share/icons/hicolor/256x256/apps"

cp "$BUILD_DIR/bin/CreatorCanvas" "$PKGDIR/usr/bin/"

cat > "$PKGDIR/DEBIAN/control" << CONTROL
Package: creatorcanvas
Version: $VERSION
Section: graphics
Priority: optional
Architecture: amd64
Depends: libqt6core6t64, libqt6gui6, libqt6widgets6
Maintainer: CreatorCanvas Project
Description: Fast, layer-based image editor for content creators
 CreatorCanvas is a simplified, layer-based image editor targeting
 content creators (YouTube thumbnails, social media posts, banners).
CONTROL

[ -f packaging/linux/creatorcanvas.png ] || \
    python3 -c "
import struct, zlib
def chunk(t, d):
    c = t + d
    return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c))
w = h = 256
raw = b''.join(b'\x00' + b'\x2f\x6f\xed' * w for _ in range(h))
png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
open('packaging/linux/creatorcanvas.png', 'wb').write(png)
"
cp packaging/linux/creatorcanvas.png \
   "$PKGDIR/usr/share/icons/hicolor/256x256/apps/creatorcanvas.png"
cp packaging/linux/creatorcanvas.desktop \
   "$PKGDIR/usr/share/applications/creatorcanvas.desktop"

dpkg-deb --build --root-owner-group "$PKGDIR"
echo ">> DEB: $(ls -1 packaging/debroot/*.deb)"
