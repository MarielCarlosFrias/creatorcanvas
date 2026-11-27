# Manter o projecto leve (disco)

O que ocupa espaço e o que pode ser apagado sem perder nada de fonte.

## O que foi apagado (2026-11-27)

| Onde | Tamanho | Porquê era lixo |
|---|---|---|
| `build/linux-asan`, `build/linux-debug`, `build/linux-release` | ~5,8 GB | árvores de preset inteiras; reconstroem-se com `cmake --preset linux-debug` quando a CI ou a depuração precisar |
| `build/staging`, `build/debroot`, `build/packages` | ~134 MB | saída dos scripts de empacotamento; regenerada por `packaging/linux/build_deb.sh` |
| `6.8.3/` (download do Qt) | 2,5 GB | tentativa de instalar o Qt 6.8 para depurar o CI; falhou e encheu o disco |
| `.tmp_tools/` (venv + gh + appimagetool) | ~100 MB | ferramentas descarregadas; `tools/publish_release.sh` volta a buscá-las |
| `CreatorCanvas-windows-build-kit.zip` | 119 MB | kit pré-construído; continua disponível no CI |

Resultado: **9,5 GB → 956 MB** (projecto), e o build activo (`build/`, 511 MB) ficou intacto e verificado.

## Regra prática

- **`build/` nunca é versionado** (`.gitignore:2`) — é a árvore de compilação.
- O que **precita** de ficar em disco fora do git:
  - `build/` — compilação incremental (511 MB; sem ela, cada arranque reconstrói 500+ alvos);
  - `CreatorCanvas-0.3.0-x86_64.AppImage` e `creatorcanvas_0.3.0_amd64.deb` — os artefactos da release, na raiz.
- O que **pode apagar-se** sempre: qualquer `build/*/` de preset (debug/release/asan), `packaging/AppDir`, `build/staging`, `build/debroot`, `build/packages`.
- Para reconstruir um preset: `cmake --preset linux-debug && cmake --build --preset linux-debug`.

## Se o disco encher outra vez

1. `rm -rf build/linux-debug build/linux-release build/linux-asan` (presets completos).
2. `rm -rf build/staging build/debroot build/packages` (empacotamento).
3. Se o `build/` raiz também tiver de ir: `rm -rf build && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` (perde o incremental, mas ganha ~500 MB).
