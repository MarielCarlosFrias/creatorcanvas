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

# Instala via regras do CMake com DESTDIR — e NÃO copiando o binário staged.
# O binário staged carrega o RUNPATH da ÁRVORE DE BUILD (caminho absoluto que
# só existe na máquina que compilou); instalado assim, o .deb falhava com
# "cannot open shared object file libonnxruntime.so.1" na máquina do usuário
# (era a falha do smoke test do .deb no workflow Release). `cmake --install`
# aplica o INSTALL_RPATH ($ORIGIN/../lib/creatorcanvas), tornando o pacote
# relocalizável.
cmake --build "$BUILD_DIR" --config Release
# DESTDIR é variável de ambiente (não opção de linha de comando).
rm -rf "$DEBROOT"
mkdir -p "$DEBROOT"
DESTDIR="$DEBROOT" cmake --install "$BUILD_DIR" --config Release --prefix /usr
mkdir -p "$DEBROOT/DEBIAN"

# O install do CMake usa OUTPUT_NAME "CreatorCanvas" (maiúsculo), mas o
# .desktop (Exec=creatorcanvas), a convenção Linux e o smoke test do
# workflow usam minúsculo. Num sistema de ficheiros sensível a maiúsculas
# o comando não era encontrado. Symlink resolve os dois nomes.
BIN_DIR="$DEBROOT/usr/bin"
if [ -x "$BIN_DIR/CreatorCanvas" ] && [ ! -e "$BIN_DIR/creatorcanvas" ]; then
    ln -s CreatorCanvas "$BIN_DIR/creatorcanvas"
fi

# O modelo de IA não é versionado (é descarregado no CI) e não tem regra de
# install: entra manualmente.
if [ -d "$PROJECT_DIR/resources/models" ]; then
    mkdir -p "$DEBROOT/usr/share/creatorcanvas/models"
    cp -r "$PROJECT_DIR/resources/models/." \
          "$DEBROOT/usr/share/creatorcanvas/models/"
fi

cat > "$DEBROOT/DEBIAN/control" <<EOF
Package: $PKG_NAME
Version: $VERSION
Section: graphics
Priority: optional
Architecture: $PKG_ARCH
Depends: libc6 (>= 2.31), libqt6core6 (>= 6.2), libqt6gui6 (>= 6.2), libqt6widgets6 (>= 6.2), libstdc++6 (>= 11)
Recommends: qt6-image-formats-plugins
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
