# CreatorCanvas — Technical Documentation

This document is the technical reference for CreatorCanvas: architecture, data model, rendering pipeline, persistence format, internationalization, testing, build/packaging, conventions, and known limitations. It reflects the real state of the codebase after milestone **M15 (engine-only)** — the smart-guides/snapping engine is implemented and unit-tested, but not yet wired into canvas interaction.

## Table of contents

1. [Overview](#1-overview)
2. [Milestone history](#2-milestone-history)
3. [Architecture](#3-architecture)
4. [Data model](#4-data-model)
5. [Rendering pipeline](#5-rendering-pipeline)
6. [Canvas & interaction](#6-canvas--interaction)
7. [Internationalization](#7-internationalization)
8. [Persistence — the `.creatorcanvas` format](#8-persistence--the-creatorcanvas-format)
9. [Export](#9-export)
10. [Commands and undo/redo](#10-commands-and-undoredo)
11. [Testing](#11-testing)
12. [Build & distribution](#12-build--distribution)
13. [Roadmap](#13-roadmap)
14. [Conventions](#14-conventions)
15. [Known limitations](#15-known-limitations)

---

## 1. Overview

CreatorCanvas is a layer-based image editor aimed squarely at content creators — the workflow it optimizes for is: *pick a canvas size (YouTube thumbnail, Instagram post, etc.) → import a couple of images → add and style text → reorder layers → export a PNG*. It intentionally does not try to be a GIMP or Photoshop replacement: no brushes, no adjustment layers, no blend-mode UI, no AI features in Phase 1. Every one of those is either absent from the UI entirely or reserved as an empty interface with nothing registered against it, so there are no dead buttons.

**Current state:** Phase 1 (M0–M14) is complete — skeleton, document/layer model, undo/redo, serialization, canvas, new-document dialog, image import + transform, layers panel, text tool + effects, menus/shortcuts, export, autosave/recovery, start screen, and packaging all shipped and covered by unit tests. **M15** (smart guides & snapping) has its engine and 7 unit tests done; the interactive canvas wiring (drawing guide lines during a drag, snapping the pointer) is the next piece of work.

**Stack:** C++20, Qt 6 (Widgets), CMake with presets, QTest, ZIP+JSON project files via a vendored `miniz`.

## 2. Milestone history

Tag names as they exist in the repository's git history, in order:

| Tag | Delivered |
|---|---|
| (M0, folded into M1) | Application skeleton — CMake project, empty main window, logging, settings persistence |
| `m1-*` | Core document/layer model + serialization — 7/7 tests |
| `m4-canvas` | Canvas MVP: pan/zoom, checkerboard, software renderer, status bar |
| `m5-new-document` | New Document dialog — presets, geometry, background |
| `m6a-import` | Image import, asset store, media embedded in project file, undo/redo wiring |
| `m6b-transform-foundation` | `AffineTransform`, `contentBounds`, renderer + serialization support |
| `m6b-interaction` / `phase1-complete` (early tag, superseded) | Selection, transform handles, gestures, Layer menu |
| `m7-layers-panel` / `m7-layers-panel-final` | Layers panel: drag reorder, visibility, lock, context menu, canvas clipping, focus-on-double-click |
| `m8a-text` → `m8-text` | Text tool: add (Ctrl+T), inline WYSIWYG editing, property inspector (font/size/style/color/align), multiline rendering |
| `m8c-textbox` | Text wrap box — handles resize the box; font size changes only via the inspector |
| `m9-text-effects` | Text effects: outline (with width) and drop shadow (with blur), cached, non-destructive, serialized |
| `m10-save-open-settings` | Save/Open/Save As UI, unsaved-changes prompt, settings dialog (language + autosave interval) |
| `m11-export` | Export UI: PNG/JPEG/WebP with quality and scale, headless render (no overlays) |
| `m12-autosave` | Autosave engine + session recovery |
| `m13-start-screen` | Start screen: preset cards, recent projects, custom creation flow |
| `m14-packaging` / `m14-distribution` | DEB installer (desktop entry, AppData, SVG icon), AppImage recipe, Windows portable ZIP script |
| `m15-engine-only` | Smart guides & snapping engine, 7 unit tests — **canvas integration pending** |

**Total size at `m15-engine-only`:** ~10,220 lines (`src/` + `tests/`), of which ~4,963 lines are production code and ~2,704 lines are tests (16 test files).

## 3. Architecture

### Module diagram & dependency rules

```
┌──────────────────────────── ui/ ─────────────────────────────┐
│  MainWindow · StartScreen · LayersPanel · TextInspector       │
│  Dialogs (New/Export/Settings) · CanvasView                   │
└───────▲───────────────────────────────────▲──────────────────┘
        │ user intents (commands)           │ repaint requests
┌───────┴──────────── core/ ────────────────┴── rendering/ ────┐
│  Document · Layer tree · CommandStack (history)               │
│  ProjectFile/ProjectSerializer/ZipArchive · AssetStore        │
│  AffineTransform (geometry) · SnapEngine                      │
│                                                                │
│  CanvasRenderer — software renderer (QPainter)                │
└───────────────────────────────────────────────────────────────┘
   services/ (SettingsService · AutosaveService · RecentFiles · PresetStore · LogService)
   imageio/  (ImageImporter · DocumentExporter)
   localization/ (I18nService + JSON catalogs)
```

**Dependency rules, enforced by directory discipline:**

- `ui/` never touches layer internals directly — it goes through `Document`'s public API and reacts to its signals (`structureChanged`, `layerPropertyChanged`).
- `core/` depends only on QtCore/QtGui, never QtWidgets — it's fully unit-testable headless (this is why `test_document`, `test_layers`, `test_history`, `test_serialization`, `test_transform`, `test_snap` etc. don't need a `QApplication` display).
- **All document mutation goes through Commands** (`core/history/`). There is no second mutation path — this is what makes undo/redo correct by construction.
- Free functions in the `cc::` namespace (e.g. `cc::saveDocument`, `cc::loadDocument`) must be called with explicit `cc::` qualification from inside classes that define a member function of the same name (e.g. `MainWindow::saveDocument()`), or the member shadows the free function and the call silently resolves to the wrong overload.

### Core patterns

- **Command pattern (undo/redo).** Every mutating operation is an object with `redo()`/`undo()`, capturing the minimal delta (old/new property values or tree positions). The stack lives in `core/history/CommandStack`.
- **Signals for reactivity.** `Document` emits `structureChanged()` and `layerPropertyChanged(LayerId)`. UI panels subscribe rather than polling; the canvas maps these to targeted cache invalidation, panels map them to targeted row updates.
- **Pluggable renderer.** `CanvasRenderer` is the software (QPainter) implementation; the architecture keeps the door open for a GPU backend behind the same interface without a rewrite.

## 4. Data model

### Document

`src/core/Document.h` — the aggregate root. Owns the layer tree, the asset store, and a revision counter used for render-cache invalidation.

Public API (as implemented):

```cpp
int width() const;
int height() const;
int dpi() const;
AssetStore& assets();
GroupLayer* rootGroup() const;

Layer* findLayer(const LayerId& id) const;
GroupLayer* parentOf(const LayerId& id) const;
int indexOf(const LayerId& id) const;

bool addLayer(std::unique_ptr<Layer> layer, GroupLayer* parent = nullptr, int index = -1);
std::unique_ptr<Layer> takeLayer(const LayerId& id);
bool removeLayer(const LayerId& id);
bool reorderLayer(const LayerId& id, GroupLayer* newParent, int newIndex);
LayerId duplicateLayer(const LayerId& id);

bool setLayerName(const LayerId& id, QString name);
bool setLayerVisible(const LayerId& id, bool visible);
bool setLayerLocked(const LayerId& id, bool locked);
bool setLayerOpacity(const LayerId& id, float opacity);
bool setLayerBlendMode(const LayerId& id, BlendMode mode);
bool setLayerTransform(const LayerId& id, const AffineTransform& transform);
bool setLayerTextBox(const LayerId& id, const QSizeF& box);
bool setLayerTextEffects(const LayerId& id, const TextEffects& effects);
bool setLayerTextContent(const LayerId& id, QString content);
void touchLayer(const LayerId& id);

quint64 revision() const;

signals:
    void structureChanged();
    void layerPropertyChanged(const LayerId& id);
```

### Layer hierarchy

`src/core/layers/Layer.h` defines an abstract `Layer` base plus concrete types:

```cpp
class Layer;                          // abstract base
class GroupLayer      final : public Layer;
class ImageLayer      final : public Layer;
class TextLayer       final : public Layer;
class ShapeLayer      final : public Layer;
class BackgroundLayer final : public Layer;
```

Shared `Layer` surface:

```cpp
QString name;
bool visible = true;
bool locked = false;
AffineTransform transform;
float opacity() const;            // clamped [0,1]
void setOpacity(float value);
virtual QRectF contentBounds() const;
virtual std::unique_ptr<Layer> deepCopy() const = 0;
```

`TextLayer` additionally carries:

```cpp
QString content;
QString fontFamily = "Sans Serif";
bool bold = false, italic = false, underline = false;
TextEffects effects;   // outline + drop shadow, see below
```

**`TextEffects`** (`src/core/layers/TextEffects.h`):

```cpp
struct TextOutline {
    bool enabled = false;
    QColor color{0, 0, 0};
    double width = 4.0;
    bool operator==(const TextOutline&) const;
};
struct TextShadow { /* enabled, color, offset, blur */ };
```

Effects are cached (rasterized once per content/style change, not per frame), non-destructive, and serialized with the layer.

**Group transforms compose multiplicatively** with descendants — children store coordinates in parent space, so moving/scaling/rotating a group propagates correctly without rewriting children.

### Geometry

`AffineTransform` (`src/core/geometry/AffineTransform.h`) — position (center), rotation, and X/Y scale (negative scale = flip). Used uniformly for hit-testing, rendering, and serialization.

### Assets

`AssetStore` (`src/core/assets/AssetStore.h`) holds imported image bytes independently of the layers that reference them (`ImageLayer` stores an `assetRef`, not pixels). Original encoded bytes are retained for lossless re-save; the decoded `QImage` is a runtime-only cache.

## 5. Rendering pipeline

`CanvasRenderer` (`src/rendering/CanvasRenderer.cpp`, 258 lines) is the software renderer used both for on-screen painting and for export.

- Walks the layer tree in paint order (bottom to top), skipping invisible layers.
- Applies each layer's `AffineTransform` and `opacity`.
- Composites into the canvas surface with `QPainter`.
- **What you see is what exports:** the same scene path renders on-screen (with a screen-space overlay pass for selection box, transform handles, and — once M15 lands — snap guides) and off-screen for export (overlays excluded). There is no separate export codepath to drift out of sync with the editor.
- Text is laid out via Qt's `QTextLayout`/font stack; outline/shadow effects are pre-rendered into a cache keyed on content + style, not recomputed every frame.

## 6. Canvas & interaction

`CanvasView` (`src/ui/canvasview.cpp`, 698 lines) owns the interactive surface.

**Signals** (what the rest of the UI reacts to):

```cpp
signals:
    void zoomChanged(double zoom);
    void cursorMoved(const QPointF& documentPos);
    void fileDropped(const QString& filePath);
    void selectionChanged(const cc::LayerId& id);
    void transformCommitted(const cc::LayerId& id,
                             const cc::AffineTransform& oldValue,
                             const cc::AffineTransform& newValue);
    void textCommitted(const cc::LayerId& id, const QString& oldValue, const QString& newValue);
    void deleteRequested(const cc::LayerId& id);
    void textBoxCommitted(const cc::LayerId& id, const QSizeF& oldValue, const QSizeF& newValue);
```

**Keyboard shortcuts handled directly on the canvas:**

| Key | Action |
|---|---|
| `F` | Fit to viewport |
| `+` / `=` | Zoom in |
| `-` | Zoom out |
| `1` | Zoom to 100% |
| `Space` (hold) | Pan mode (drag to pan) |
| `Delete` / `Backspace` | Delete selected layer |
| `Escape` | Cancel inline text edit (no commit) |

**Application-level shortcuts** (`MainWindow`):

| Shortcut | Action |
|---|---|
| Ctrl+N | New document |
| Ctrl+O | Open |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save As |
| Ctrl+I | Import image |
| Ctrl+E | Export |
| Ctrl+Z | Undo |
| Ctrl+Shift+Z | Redo |
| Ctrl+T | Add text |
| Delete | Delete selected layer |
| Ctrl+, (Preferences) | Settings dialog |

**Smart guides & snapping (`SnapEngine`, `src/core/snap/`, 151 lines + 245 lines of tests):** the alignment/snap-detection logic — edge and center snapping against other layers and the canvas bounds — is implemented and covered by 7 unit tests. It is **not yet called from `CanvasView`'s drag-gesture handlers**, so no visual guide lines are drawn yet and the pointer does not snap during a move. This is the concrete next step for M15.

## 7. Internationalization

`I18nService` (`src/localization/i18nservice.{h,cpp}`) resolves every user-facing string.

- **Catalog format:** flat-key JSON per namespace — `common`, `editor`, `settings`, `templates` — under `resources/locales/<lang>/`.
- **Supported languages:** `en` (English), `pt-BR` (Brazilian Portuguese) — both fully populated for Phase 1's UI surface.
- **Lookup with fallback:** `t(namespace, key)` → language catalog → English catalog → humanized dotted key + logged warning. A missing key never crashes and never shows a raw identifier to the user.
- **Live switching:** changing language in Settings persists the preference and emits `languageChanged`; every panel re-resolves its strings immediately — no restart required.
- **No hardcoded UI strings anywhere in `src/ui/`.**

Example (`resources/locales/en/common.json`):

```json
{
  "app": { "title": "CreatorCanvas", "tagline": "Create something amazing" },
  "menu": {
    "file": "&File", "file.new": "&New...", "file.open": "&Open...",
    "file.save": "&Save", "file.saveAs": "Save &As...",
    "file.import": "&Import Image...", "file.export": "E&xport...",
    "edit.undo": "&Undo", "edit.redo": "&Redo",
    "layer.flipH": "Flip &Horizontal", "layer.flipV": "Flip &Vertical",
    "settings.open": "&Preferences..."
  },
  "statusbar": { "version": "Version %1", "language": "Language: %1", "zoom": "Zoom: %1%" }
}
```

`resources/presets/builtin.json` holds the canvas-size presets shown on the Start Screen and in the New Document dialog — YouTube Thumbnail/Banner/Shorts, Instagram Square/Portrait Post/Story, TikTok Video, Facebook Post/Story, X Post/Header, and a transparent 1280×720 canvas.

## 8. Persistence — the `.creatorcanvas` format

A project file is a ZIP archive (read/written via a vendored `miniz`, wrapped by `ZipArchive`):

```
manifest.json        ← document + layer tree + metadata
media/<uuid>.png      ← original imported bytes, kept as-is (not recompressed on edit)
preview.png            ← thumbnail used by the Start Screen's recent-projects list
```

`manifest.json` shape:

```json
{
  "formatVersion": 1,
  "app": { "name": "CreatorCanvas", "version": "0.1.0" },
  "document": {
    "width": 1280, "height": 720, "dpi": 96,
    "background": { "type": "solid", "color": "#101014" }
  },
  "assets": [
    { "id": "...", "file": "media/....png", "sha256": "...", "width": 1920, "height": 1080 }
  ],
  "layers": [ { "type": "text", "id": "...", "name": "Main Title" } ]
}
```

**Read/write entry points** (`src/core/serialization/ProjectFile.h`):

```cpp
bool saveDocument(const Document& doc, const QString& filePath, QString* errorMessage = nullptr);
std::unique_ptr<Document> loadDocument(const QString& filePath, QString* errorMessage = nullptr);
```

- **Asset integrity:** each asset's SHA-256 is checked on load.
- **Forward safety:** the loader switches on `formatVersion` and would run registered migrators as the format evolves; the writer always emits the current version. An unrecognized layer type deserializes as an opaque placeholder rather than failing the whole load.
- `ProjectSerializer` (277 lines) handles the JSON shape; `ZipArchive` (96 lines) wraps `miniz` for the container.

## 9. Export

`DocumentExporter` (`src/imageio/DocumentExporter.h`):

```cpp
bool exportDocumentToImage(const Document& doc, const QString& filePath,
                            const QString& format, int quality, double scale,
                            QString* errorMessage = nullptr);
```

- Formats: PNG, JPEG, WebP.
- `scale` renders at a multiple of the document's native size (e.g. 2× for high-res export).
- Uses the same `CanvasRenderer` path as the live canvas, with overlays (selection box, handles) excluded — so exported output always matches what was visible while editing.
- `ExportDialog` (`src/ui/exportdialog.cpp`, 86 lines) exposes format/quality/scale to the user.

## 10. Commands and undo/redo

`core/history/` — `Command` (base), `CommandStack` (95 lines), `DocumentCommands.h` (the concrete command set: add/remove/reorder/duplicate layer, property changes, transform commits, text commits, text-box resize, etc.).

- Every mutating call listed under [Document](#4-data-model) above is wrapped in a `Command` before reaching the model — `CanvasView`, `LayersPanel`, and the dialogs never call `Document` setters directly outside of a command.
- `redo()`/`undo()` capture old/new values only (not full document snapshots), so history stays cheap even for large documents.

## 11. Testing

16 QTest suites under `tests/unit/`, all passing at the `m15-engine-only` tag:

| Test file | Lines | Covers |
|---|---|---|
| `test_settings.cpp` | — | `SettingsService` persistence |
| `test_i18n.cpp` | 169 | Catalog lookup, fallback, key symmetry between `en`/`pt-BR` |
| `test_logging.cpp` | 65 | `LogService` |
| `test_layers.cpp` | 166 | Layer tree operations |
| `test_document.cpp` | 364 | `Document` API surface |
| `test_history.cpp` | 362 | Command stack, undo/redo, interleaving |
| `test_serialization.cpp` | 311 | Save/load round-trip, corrupt-file handling |
| `test_renderer.cpp` | 140 | `CanvasRenderer` output |
| `test_newdocument.cpp` | 123 | New Document dialog / presets |
| `test_import.cpp` | 187 | Image import, format sniffing, malformed input |
| `test_transform.cpp` | 179 | `AffineTransform` math |
| `test_textbox.cpp` | 67 | Text wrap-box resize behavior |
| `test_effects.cpp` | 96 | Outline/shadow text effects |
| `test_export.cpp` | 83 | Export pipeline |
| `test_autosave.cpp` | 77 | Autosave timing + recovery |
| `test_snap.cpp` | 245 | `SnapEngine` alignment/snap detection |

Run everything:

```bash
cmake --preset linux-debug && cmake --build --preset linux-debug
ctest --test-dir build/linux-debug --output-on-failure
```

Expected: `16/16 Passed`.

**`core/` tests need no `QApplication`** — they exercise `Document`, `Layer`, `CommandStack`, serialization, transform math, and `SnapEngine` headlessly, which is only possible because `core/` has no QtWidgets dependency (see [Architecture](#3-architecture)).

## 12. Build & distribution

### CMake presets (`CMakePresets.json`)

| Preset | Purpose |
|---|---|
| `linux-debug` | Debug build, Ninja, Linux |
| `linux-asan` | AddressSanitizer build for CI/manual leak & UB checking |
| `linux-release` | Release build for packaging |
| `windows-debug` | Debug build under MSVC 2022 |

### CI

A GitHub Actions matrix builds and tests on Windows + Ubuntu (22.04/24.04-class runners), installing Qt via `jurplel/install-qt-action` pinned to `6.8.*`, configuring with the matching preset, building with `cmake --build`, and running `ctest`. The ASan job sets `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`.

### Packaging artifacts (present in the repo)

```
packaging/
├── linux/
│   ├── creatorcanvas.desktop        # menu entry
│   ├── creatorcanvas.appdata.xml    # AppStream metadata
│   ├── creatorcanvas.png            # icon
│   ├── AppRun.sh                    # AppImage entry point
│   ├── build_appimage.sh
│   └── build_deb.sh
├── debroot/
│   └── creatorcanvas_0.1.0_amd64/   # staged .deb tree (DEBIAN/control, usr/bin, .desktop, icon)
└── windows/
    └── build_portable.ps1           # windeployqt-based portable ZIP
```

`resources/icons/creatorcanvas.svg` is the source icon; packaging scripts rasterize it as needed.

## 13. Roadmap

**Immediate (M15 completion):** wire `SnapEngine` into `CanvasView`'s move/transform gestures — draw guide lines during drag, snap the pointer/handle to detected alignment points.

**Explicitly deferred past Phase 1** (declared up front so the UI never grows dead controls for them):

- Brush & eraser tools
- Groups/ungroup UI (the data model already supports `GroupLayer`; the *interaction* for grouping/ungrouping is Phase 2)
- Blend-mode UI (the renderer's `BlendMode` enum and `setLayerBlendMode` already exist; exposing it in the inspector is Phase 2)
- Layer effects beyond text outline/shadow (general effects pipeline is reserved as an interface, `effects/` is empty in Phase 1)
- Non-destructive image adjustments
- Styled templates with pre-designed content (canvas *sizes* ship now; designed template content is Phase 2)
- Background removal, any AI-assisted feature (interfaces such as `BackgroundRemovalProvider` are declared for future registration; nothing registers against them yet)
- Gradient fills, perspective/warp transforms, multi-selection + true grouping, layer thumbnails in the panel
- GPU-accelerated renderer (an opt-in second `IRenderer` implementation, once the software path is proven)
- macOS build (the module boundaries — especially keeping OS-specific code confined to `platform/` — are meant to make this a recompile-plus-small-additions job, not a rewrite)

## 14. Conventions

- **Localization:** never hardcode a user-facing string in `src/ui/`. Always `I18nService::t(namespace, key)`. Add new keys to *both* `en` and `pt-BR` catalogs in the same change — a CI-style check should assert key symmetry.
- **Mutation path:** every document change goes through a `Command`. If you find yourself calling a `Document` setter directly from UI code, wrap it in a command instead.
- **`std::unique_ptr` + forward declarations:** a `Q_OBJECT` class holding a `std::unique_ptr<T>` member needs the *full* definition of `T` (a real `#include`), not just a forward declaration — the implicitly-generated destructor needs `sizeof(T)`. This caused real build breaks during M12 (`AutosaveService`) and M13 (`RecentFiles`); the fix each time was replacing the forward declaration with the actual header include in `mainwindow.h`.
- **Namespace shadowing:** free functions in `cc::` (e.g. `cc::saveDocument`) must be called with explicit `cc::` qualification from any class that defines a same-named member (e.g. `MainWindow::saveDocument()`), otherwise the member silently shadows the free function inside that class's scope.
- **No broad `sed` edits** on source files — patches are applied as targeted, reviewed string replacements (`assert old_str in src` before any replace), so a change either applies exactly once where intended or fails loudly instead of matching the wrong occurrence.
- **Logging:** via `qInfo`/`qWarning`/etc., with categories named `cc.<area>` (e.g. `cc.i18n`). Logs rotate under the OS app-data directory.

## 15. Known limitations

- **M15 canvas integration is not done.** `SnapEngine` exists and passes its 7 unit tests, but no guide lines render and no snapping happens during an actual drag yet.
- **No macOS build** — the architecture reserves `platform/` for OS-specific glue precisely so this can be added later, but it hasn't been attempted.
- **No SVG/GIF import** — Phase 1 image import covers PNG/JPEG/WebP; SVG is used only for the app icon, not as an importable layer type.
- **`.creatorcanvas` format version is 1** with no released migrator yet (nothing to migrate *from* — the versioning scaffold is in place and tested against unknown-layer-type graceful degradation, but hasn't been exercised on a real format bump).
- **License:** MIT (see `LICENSE`), with Qt 6's LGPLv3 dynamic-linking obligations and other third-party terms tracked in `NOTICE.md`. Any binary release (installer, AppImage, DEB, portable ZIP) must ship the LGPLv3 text and keep Qt dynamically linked/replaceable per `NOTICE.md`.
