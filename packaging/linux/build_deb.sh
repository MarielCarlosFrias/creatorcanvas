#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <build-directory>" >&2
    exit 1
fi

BUILD_DIR="$1"
PROJECT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

# Extrai VERSION de dentro do bloco project(...), mesmo multi-linha.
VERSION="$(awk '/project\(/{flag=1} flag{print} /\)/{if(flag) exit}' "$PROJECT_DIR/CMakeLists.txt" \
    | grep -m1 -oP 'VERSION\s+\K[0-9.]+' || true)"
VERSION="${VERSION:-0.1.0}"

STAGING="$BUILD_DIR/staging"
DEBROOT="$BUILD_DIR/debroot"
PKG_NAME="creatorcanvas"
PKG_ARCH="$(dpkg --print-architecture 2>/dev/null || echo amd64)"

echo "==> Building DEB for CreatorCanvas $VERSION [$PKG_ARCH]"

cmake --build "$BUILD_DIR" --target stage --config Release

rm -rf "$DEBROOT"
mkdir -p "$DEBROOT/DEBIAN"
mkdir -p "$DEBROOT/usr/bin"
mkdir -p "$DEBROOT/usr/share/creatorcanvas"
mkdir -p "$DEBROOT/usr/share/applications"
mkdir -p "$DEBROOT/usr/share/metainfo"
mkdir -p "$DEBROOT/usr/share/icons/hicolor/scalable/apps"
mkdir -p "$DEBROOT/usr/lib/creatorcanvas"

install -m 0755 "$STAGING/bin/creatorcanvas" "$DEBROOT/usr/bin/creatorcanvas"

if [ -d "$STAGING/lib/creatorcanvas" ]; then
    cp -d -r "$STAGING/lib/creatorcanvas/." \
          "$DEBROOT/usr/lib/creatorcanvas/"
fi

if [ -d "$STAGING/share/creatorcanvas" ]; then
    cp -r "$STAGING/share/creatorcanvas/." \
          "$DEBROOT/usr/share/creatorcanvas/"
fi

if [ -d "$PROJECT_DIR/resources/models" ]; then
    mkdir -p "$DEBROOT/usr/share/creatorcanvas/models"
    cp -r "$PROJECT_DIR/resources/models/." \
          "$DEBROOT/usr/share/creatorcanvas/models/"
fi

install -m 0644 "$PROJECT_DIR/packaging/linux/creatorcanvas.desktop" \
        "$DEBROOT/usr/share/applications/creatorcanvas.desktop"
install -m 0644 "$PROJECT_DIR/packaging/linux/creatorcanvas.appdata.xml" \
        "$DEBROOT/usr/share/metainfo/creatorcanvas.appdata.xml"
install -m 0644 "$PROJECT_DIR/resources/icons/creatorcanvas.svg" \
        "$DEBROOT/usr/share/icons/hicolor/scalable/apps/creatorcanvas.svg"

cat > "$DEBROOT/DEBIAN/control" <<EOF
Package: $PKG_NAME
Version: $VERSION
Section: graphics
Priority: optional
Architecture: $PKG_ARCH
Depends: libc6 (>= 2.31), libqt6core6 (>= 6.5), libqt6gui6 (>= 6.5), libqt6widgets6 (>= 6.5), libstdc++6 (>= 11)
Maintainer: CreatorCanvas Team <noreply@creatorcanvas.example.com>
Description: Simple image editor for content creators
 CreatorCanvas is a lightweight, layer-based image editor focused on
 what content creators actually need: fast canvas sizing for YouTube,
 Instagram, TikTok and Facebook, real layer control, and reliable
 export in PNG/JPEG/WebP.
EOF

cat > "$DEBROOT/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if [ -x /usr/bin/update-desktop-database ]; then
    update-desktop-database -q /usr/share/applications || true
fi
if [ -x /usr/bin/gtk-update-icon-cache ]; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
fi
exit 0
EOF
chmod 0755 "$DEBROOT/DEBIAN/postinst"

mkdir -p "$BUILD_DIR/packages"
DEB_FILE="$BUILD_DIR/packages/${PKG_NAME}_${VERSION}_${PKG_ARCH}.deb"
dpkg-deb --build --root-owner-group "$DEBROOT" "$DEB_FILE"

echo
echo "Built: $DEB_FILE"
echo "Install with: sudo dpkg -i $DEB_FILE"
