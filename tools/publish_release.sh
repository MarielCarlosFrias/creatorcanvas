#!/usr/bin/env bash
# Publica uma release do CreatorCanvas: constroi os artefactos (AppImage + deb)
# e cria a release no GitHub com as notas de docs/release-notes-<versao>.md.
#
# Uso:
#   ./tools/publish_release.sh 0.3.0             # constroi e publica
#   ./tools/publish_release.sh 0.3.0 --dry-run   # mostra os comandos sem executar
#
# Pre-requisitos:
#   - gh autenticado (gh auth login) ou GITHUB_TOKEN no ambiente
#   - dpkg-deb (pacote debian-utils); appimagetool e descarregado se faltar
set -euo pipefail

if [ $# -lt 1 ]; then
    echo "Uso: $0 <versao> [--dry-run]" >&2
    exit 1
fi

VERSION=$1
DRY_RUN=0
if [ $# -ge 2 ] && [ "$2" = "--dry-run" ]; then DRY_RUN=1; fi

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TAG="v$VERSION"
NOTES="$PROJECT_DIR/docs/release-notes-$VERSION.md"

run() {
    if [ "$DRY_RUN" = "1" ]; then
        echo "[dry-run] $*"
    else
        "$@"
    fi
}

echo "==> A publicar $TAG a partir de $PROJECT_DIR"

# 1. gh disponivel e autenticado?
if ! command -v gh >/dev/null 2>&1; then
    echo ">> 'gh' nao encontrado (sudo apt install gh, ou binario em cli/cli/releases)." >&2
    exit 1
fi
if [ "$DRY_RUN" = "0" ] && ! gh auth status >/dev/null 2>&1; then
    echo ">> Sem sessao do GitHub. Corre 'gh auth login' (ou exporta GITHUB_TOKEN)." >&2
    exit 1
fi

# 2. appimagetool (sem FUSE: APPIMAGE_EXTRACT_AND_RUN=1 faz a extraccao)
APPDIR_TOOL="$PROJECT_DIR/.tmp_tools/appimagetool"
if ! command -v appimagetool >/dev/null 2>&1 && [ ! -x "$APPDIR_TOOL" ]; then
    echo "==> A descarregar appimagetool..."
    mkdir -p "$PROJECT_DIR/.tmp_tools"
    run curl -sSL -o "$APPDIR_TOOL" \
        https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
    run chmod +x "$APPDIR_TOOL"
fi
export PATH="$PROJECT_DIR/.tmp_tools:$PATH"
export APPIMAGE_EXTRACT_AND_RUN=1

# 3. Artefactos
echo "==> A construir a AppImage..."
run "$PROJECT_DIR/packaging/linux/build_appimage.sh" "$PROJECT_DIR/build"
APPIMAGE="$PROJECT_DIR/build/CreatorCanvas-$VERSION-x86_64.AppImage"

echo "==> A construir o .deb..."
run "$PROJECT_DIR/packaging/linux/build_deb.sh" "$PROJECT_DIR/build"
DEB="$PROJECT_DIR/build/packages/creatorcanvas_$VERSION"_amd64.deb

# 4. Notas de lancamento
if [ "$DRY_RUN" = "0" ] && [ ! -f "$NOTES" ]; then
    echo ">> Notas em falta: $NOTES" >&2
    exit 1
fi

# 5. Tag anotada, se ainda nao existir
if ! git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
    echo "==> A criar a tag $TAG..."
    run git tag -a "$TAG" -m "CreatorCanvas $VERSION"
fi

# 6. Release
echo "==> A criar a release $TAG..."
run gh release create "$TAG" \
    --title "CreatorCanvas $VERSION" \
    --notes-file "$NOTES" \
    "$APPIMAGE" "$DEB"

echo "==> Feito."
