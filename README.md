# CreatorCanvas

A fast, layer-based image editor for content creators — YouTube thumbnails, social posts, banners, and similar assets. Combines the simplicity of a template-driven creation flow with a real layer/undo model, without trying to be a full Photoshop/GIMP replacement.

Cross-platform by design: **Windows 10/11** and **Linux** today, with the architecture kept macOS-ready for later.

> **Status:** Phase 1 (core editor) is functionally complete through **M14** (packaging). **M15 — Smart guides & snapping** has its engine implemented and unit-tested; canvas interaction wiring is still pending. See [Milestones](#milestones) below and `DOCUMENTATION.md` for the full technical reference.

---

## Features (Phase 1)

- **Layer-based editing** — image, text, shape, group and background layers, with visibility/lock/opacity/rename/reorder/duplicate
- **Undo/redo** — every mutation goes through a command stack; no operation bypasses history
- **Project files** — `.creatorcanvas` (a ZIP containing `manifest.json` + original media assets + a thumbnail), versioned for forward compatibility
- **Canvas** — pan/zoom/fit, checkerboard transparency, software (QPainter) renderer with per-layer raster caching
- **Image import** — drag-and-drop or picker, with format sniffing and validation; move/scale/rotate via on-canvas handles
- **Text tool** — inline WYSIWYG editing, font/size/style/color/alignment, resizable text box, outline and drop-shadow effects
- **Shape tool** — rectangle, rounded rectangle, ellipse, line, polygon, with fill/stroke controls
- **Export** — PNG / JPEG / WebP, with quality and scale multiplier, rendered off the same pipeline you see on screen (no separate export codepath)
- **Autosave & recovery** — configurable interval, crash-recovery prompt on next launch
- **Start screen** — creator-focused canvas presets (YouTube thumbnail/banner/Shorts, Instagram post/story, TikTok, Facebook, X, and a transparent canvas), plus recent projects
- **Localization** — full English and Brazilian Portuguese (pt-BR), JSON catalogs, no hardcoded UI strings, live language switching
- **Smart guides & snapping engine** — alignment/snap logic implemented and unit-tested; not yet wired into canvas gestures (`m15-engine-only`)

Deliberately **not** in Phase 1 (by design, not by omission — no dead buttons exist for these): brush/eraser tools, blend-mode UI, layer effects/adjustments, background removal, AI features, styled templates (only canvas *sizes* ship now). See the [Roadmap](#roadmap) in `DOCUMENTATION.md`.

## Tech stack

| Component | Choice |
|---|---|
| Language | C++20 |
| Framework | Qt 6 (Widgets), reference/deployment toolchain **Qt 6.8 LTS**, dev builds tolerate 6.2+ |
| Build | CMake ≥ 3.24 with presets (`linux-debug`, `linux-asan`, `linux-release`, `windows-debug`) |
| Rendering | Custom `IRenderer` interface; software renderer (QPainter) in Phase 1, GPU renderer reserved behind the same interface |
| Imaging | `QImage` + Qt image plugins (PNG/JPEG/WebP) |
| Project container | ZIP (vendored `miniz`) holding `manifest.json` + `media/` |
| Testing | QTest, 16 unit-test suites |
| Packaging | AppImage + DEB (Linux), portable ZIP / NSIS stub (Windows) |

Full rationale for the stack choice (and the alternatives that were rejected — Electron, Rust+Tauri, Rust+egui, Python+PySide6, Flutter/Avalonia) is in `DOCUMENTATION.md`.

## Project size

~10,200 lines total: ~5,000 lines of production code across `core/`, `rendering/`, `ui/`, `services/`, `imageio/`, `localization/`; ~2,700 lines across 16 test files.

## Prerequisites

- CMake ≥ 3.24, Ninja (recommended), a C++20 compiler (GCC 11+, Clang 14+, MSVC 2022)
- Qt 6.x development packages (Core, Gui, Widgets, Test)

## Build & test — Linux

```bash
sudo apt install build-essential cmake ninja-build \
    qt6-base-dev qt6-base-dev-tools libgl1-mesa-dev

cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --test-dir build/linux-debug --output-on-failure
./build/linux-debug/bin/CreatorCanvas
```

## Build & test — Windows (VS 2022 developer prompt, Qt installed)

```bat
cmake --preset windows-debug
cmake --build --preset windows-debug --config Debug
ctest --test-dir build/windows-debug -C Debug --output-on-failure
build\windows-debug\bin\Debug\CreatorCanvas.exe
```

Running outside a Qt environment requires the Qt DLLs on `PATH`; the packaging scripts under `packaging/windows/` handle deployment.

## Sanitizer build (Linux)

```bash
cmake --preset linux-asan
cmake --build --preset linux-asan
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/linux-asan --output-on-failure
```

## Packaging

```bash
packaging/linux/build_appimage.sh   # AppImage
packaging/linux/build_deb.sh        # .deb
packaging/windows/build_portable.ps1  # Windows portable ZIP
```

## Milestones

Phase 1 (core editor) is complete through packaging; Phase 1.5 (smart guides) is in progress.

| # | Milestone | Status |
|---|---|---|
| M0 | Application skeleton | ✅ Done |
| M1 | Core document/layer model | ✅ Done |
| M2 | Undo/redo history | ✅ Done |
| M3 | Project save/open (v1 serialization) | ✅ Done |
| M4 | Canvas view (pan/zoom) | ✅ Done |
| M5 | New Document dialog | ✅ Done |
| M6 | Image import + transform (handles, rotate/scale/flip) | ✅ Done |
| M7 | Layers panel | ✅ Done |
| M8 | Text tool (+ inspector) | ✅ Done |
| M9 | Text effects (outline, drop shadow) | ✅ Done |
| M10 | Menus, shortcuts, settings dialog | ✅ Done |
| M11 | Export (PNG/JPEG/WebP) | ✅ Done |
| M12 | Autosave + crash recovery | ✅ Done |
| M13 | Start screen (presets + recents) | ✅ Done |
| M14 | Packaging (AppImage, DEB, Windows portable) | ✅ Done |
| M15 | Smart guides & snapping | 🟡 Engine + tests done, canvas wiring pending |

Full per-milestone detail (what shipped, what was verified, known follow-ups) is in `DOCUMENTATION.md`.

## Project structure

```
creatorcanvas/
├── CMakeLists.txt
├── CMakePresets.json
├── src/
│   ├── main.cpp
│   ├── core/            # Document, layers, history, serialization, geometry, snap
│   ├── rendering/        # CanvasRenderer (software renderer)
│   ├── ui/                # MainWindow, CanvasView, panels, dialogs, StartScreen
│   ├── services/          # Settings, Autosave, RecentFiles, PresetStore, Log
│   ├── localization/       # I18nService
│   └── imageio/            # ImageImporter, DocumentExporter
├── resources/
│   ├── locales/{en,pt-BR}/{common,editor,settings,templates}.json
│   ├── presets/builtin.json
│   └── icons/
├── tests/unit/            # 16 QTest suites
├── packaging/{linux,windows,debroot}/
└── DOCUMENTATION.md
```

## Conventions

- **No hardcoded user-facing strings.** Everything resolves through `I18nService::t(namespace, key)`, backed by JSON catalogs in `resources/locales/<lang>/`. Missing keys fall back to English, then to the dotted key, with a logged warning.
- **All document mutation goes through Commands** (`core/history/`). There is no second mutation path — that's what keeps undo/redo correct by construction.
- **`core/` never depends on QtWidgets** — only QtCore/QtGui, so it's fully unit-testable headless.
- **Only `platform/` contains OS-conditional code.**
- `std::unique_ptr` members of `Q_OBJECT` classes require a full `#include`, not a forward declaration (this bit the project more than once — see `DOCUMENTATION.md` → Known Limitations).
- Free functions living in the `cc::` namespace should be called with explicit `cc::` qualification where a member function of the same name could otherwise shadow them.

See `DOCUMENTATION.md` for the complete conventions, architecture, and API reference.

## License

MIT — see `LICENSE`.

Third-party runtime components: Qt 6 (LGPLv3 — dynamically linked, notices shipped with installers) and `miniz` (public domain, vendored at packaging time). See `NOTICE.md` for full details and distribution obligations.
