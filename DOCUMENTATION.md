# CreatorCanvas — Technical Documentation

Technical reference for CreatorCanvas: architecture, data model, rendering, persistence, i18n, testing, build/packaging, conventions, and known limitations.

**Scope of this document:** current **`main`**. Last tagged release is **v0.2.0** (milestones M0–M20). `main` additionally includes multi-select, alignment/distribution/grouping, grid and safe-zone overlays, layout templates, and extra raster tools (crop, scissors, magic wand, clone stamp). CMake `project()` version is still `0.2.0` until the next tag.

## Table of contents

1. [Overview](#1-overview)
2. [Milestone history](#2-milestone-history)
3. [Architecture](#3-architecture)
4. [Data model & Layers](#4-data-model--layers)
5. [Rendering & raster processing](#5-rendering--raster-processing)
6. [Canvas & interaction](#6-canvas--interaction)
7. [AI Background Removal](#7-ai-background-removal)
8. [Internationalization](#8-internationalization)
9. [Persistence — the `.creatorcanvas` format](#9-persistence--the-creatorcanvas-format)
10. [Export](#10-export)
11. [Commands and undo/redo](#11-commands-and-undoredo)
12. [Testing & Verification](#12-testing--verification)
13. [Build & distribution](#13-build--distribution)
14. [Roadmap](#14-roadmap)
15. [Conventions](#15-conventions)
16. [Known limitations](#16-known-limitations)

---

## 1. Overview

CreatorCanvas is a layer-based image editor aimed at digital content creators. Typical flow: *pick a platform preset or layout template → compose images, shapes, and brush strokes → style text (outline, shadow, gradient) → crop / cut out / adjust color / remove background → export PNG, JPEG, or WebP*.

**Shipped in v0.2.0 (M0–M20):** document model, command stack, ZIP+JSON projects, software canvas renderer, transform gestures, layers panel, text + shape tools, settings, export, autosave, start screen, packaging, snapping, paint/fill, text gradients, shear, image inspector, ONNX background removal.

**On `main` after the tag (M21):** multi-layer selection, align/distribute/group, grid + snap-to-grid, YouTube/Instagram/TikTok safe-zone overlays, `TemplateFactory` start-screen templates, crop/scissors/wand/clone tools.

**Stack:** C++20, Qt 6 Widgets, ONNX Runtime (C API), CMake presets, QTest, ZIP archives via vendored `miniz`.

## 2. Milestone history

| Milestone | Delivered |
|---|---|
| `m0` / `m1` | Application skeleton — CMake, `LogService`, `SettingsService`, document/layer model + serialization |
| `m4-canvas` | Canvas MVP: pan/zoom, checkerboard, software renderer, status bar |
| `m5-new-document` | New Document dialog — social presets, geometry, background fills |
| `m6a-import` | Image import, `AssetStore`, media embedded in the project archive, undo/redo |
| `m6b-transform` | `AffineTransform`, `contentBounds`, selection handles, gestures, Layer menu |
| `m7-layers-panel` | Layers panel: drag reorder, visibility, lock, context menu, focus-on-double-click |
| `m8-text` | Text tool: add text, inline WYSIWYG, inspector, wrap box |
| `m9-text-effects` | Outline and drop shadow, serialized on the layer |
| `m10-save-open` | Save/Open/Save As, unsaved-changes prompt, settings (language + autosave) |
| `m11-export` | PNG/JPEG/WebP with quality and scale, headless offscreen render |
| `m12-autosave` | Autosave + emergency session recovery |
| `m13-start-screen` | Preset cards, recent projects with thumbnails, custom canvas |
| `m14-packaging` | Linux DEB, AppStream, SVG icon, Windows portable ZIP |
| `m15-snap` | `SnapEngine` + on-canvas guides during drag/resize |
| `m16-paint` | `ImageProcessing` brushes (Brush, Pencil, Highlighter, Airbrush, Eraser) + flood fill |
| `m17-blend` | Layer opacity + **seven** `QPainter` blend modes (see §4) |
| `m18-adjustments` | Image Inspector color/blur/sharpen/presets (baked assets) |
| `m19-text-styling` | Two-stop linear/radial text gradients; shear X/Y on `AffineTransform` |
| `m20-ai-removal` | ONNX `u2netp` (and custom models), cooperative cancellation, `--smoke-test` |
| `m21-composer` | Multi-select, align/distribute/group, grid, safe zones, templates, crop/scissors/wand/clone |

## 3. Architecture

### Module diagram

```
┌──────────────────────────────── ui/ ─────────────────────────────────────┐
│  MainWindow · StartScreen · LayersPanel · Text/Shape/Image inspectors    │
│  AIBackgroundDialog · MaskEditCanvas · Dialogs · CanvasView              │
└───────▲───────────────────────────────────────────▲──────────────────────┘
        │ user intents (commands)                   │ repaint requests
┌───────┴──────────── core/ ────────────────────────┴─────── rendering/ ──┐
│  Document · Layer tree · CommandStack                                    │
│  ProjectFile (ZIP+JSON+Thumbnail) · AssetStore                           │
│  AffineTransform · SnapEngine · TemplateFactory                          │
│  ImageProcessing (raster ops) · BackgroundRemover (ONNX)                 │
│  renderDocument() — QPainter compositor (no GPU path)                    │
└──────────────────────────────────────────────────────────────────────────┘
   services/     SettingsService · AutosaveService · RecentFiles · LogService · PresetStore
   imageio/      ImageImporter · DocumentExporter   (compiled into cc_core)
   localization/ I18nService + JSON catalogs
```

CMake targets (`src/CMakeLists.txt`):

| Target | Sources |
|---|---|
| `cc_core` | `core/*` + `imageio/*`, links Qt Core/Gui, `miniz`, ONNX Runtime |
| `cc_rendering` | `rendering/CanvasRenderer.*` |
| `cc_services` | settings, autosave, recents, log, presets |
| `cc_localization` | i18n + locale `.qrc` |
| `creatorcanvas` | `main.cpp` + `ui/*` (output name `CreatorCanvas`) |

There is **no** `src/platform/` tree and **no** `IRenderer` / `core/paint/` module. Raster drawing lives in `cc::ImageProcessing`.

### Principles

- **Separation of concerns:** `core/` must not include QtWidgets, so unit tests and CI can run with `QT_QPA_PLATFORM=offscreen`.
- **Command pattern:** mutating `Document` / layer structure should go through `core/history/Command`. Undo stores property deltas, not full document clones. `Layer` still has public fields — treat those as a read path, not a second write path (see §16).
- **Async cancellation:** ONNX inference inspects `std::atomic<bool>` cancellation flags so the UI can abort.
- **Archive safety:** zip-bomb limits on entry count and uncompressed size; path traversal rejected; embedded media SHA-256 verified on load.

## 4. Data model & Layers

### Document

`src/core/Document.h` owns the root `GroupLayer`, `AssetStore`, canvas width/height/DPI, and a monotonically increasing `revision()`.

### Layer types

All types derive from `cc::Layer` (`src/core/layers/Layer.h`):

- `GroupLayer` — children in paint order (index 0 = bottom).
- `ImageLayer` — `assetId` into `AssetStore` plus `naturalWidth` / `naturalHeight`. No separate adjustment object; color filters replace the asset.
- `TextLayer` — content, font, box, alignment, `TextEffects` (outline, shadow, gradient).
- `ShapeLayer` — rectangle, rounded rect, ellipse, line, polygon; fill/stroke/corner radius/points.
- `BackgroundLayer` — solid fill.

### Shared properties

- `name`, `visible`, `locked` (public fields)
- `opacity` — `[0, 1]`, via getter/setter
- `blendMode` — see below
- `transform` — `AffineTransform`: translation, rotation (deg), scale X/Y, shear X/Y

### Blend modes (implemented)

`enum class BlendMode` maps to `QPainter::CompositionMode`:

| Enum | QPainter mode |
|---|---|
| `Normal` | `SourceOver` |
| `Multiply` | `Multiply` |
| `Screen` | `Screen` |
| `Overlay` | `Overlay` |
| `Darken` | `Darken` |
| `Lighten` | `Lighten` |
| `Add` | `Plus` |

There are **seven** modes, not twelve. Dodge, burn, hard/soft light, difference, and exclusion are not implemented.

### Text effects

`TextEffects` (`src/core/layers/TextEffects.h`):

- Outline: enable, color, width
- Shadow: enable, color, offset X/Y, blur (downscale-blur approximation in the renderer)
- Gradient: enable, `type` 0 = linear / 1 = radial, two colors, angle (degrees). **No conical gradient.**

### Templates

`src/core/templates/TemplateFactory` builds starter documents:

| Kind | Size |
|---|---|
| YouTube tech review | 1280×720 |
| YouTube gaming | 1280×720 |
| Instagram promo | 1080×1080 |
| TikTok / Reels | 1080×1920 |

## 5. Rendering & raster processing

### Software compositor

`cc::renderDocument()` in `src/rendering/CanvasRenderer.cpp`:

1. Walks the layer tree bottom-up.
2. Multiplies group opacity down the tree.
3. Sets `QPainter` transform from `AffineTransform::matrix(contentBounds)`.
4. Sets composition mode from `BlendMode`.
5. Draws background fill, shapes, text (with effects), or the image asset.

The renderer header still documents **full recomposition every call** — there is no per-layer pixmap cache. Export and the viewport share this function; `CanvasView` then draws editor overlays (handles, guides, grid, safe zones, tool cursors) on top.

### Raster engine

`cc::ImageProcessing` (`src/core/image/ImageProcessing.{h,cpp}`), not a `core/paint/` package:

| API | Role |
|---|---|
| `paintStroke` | Brush / Pencil / Highlighter / Airbrush / Eraser between two points |
| `floodFill` | Contiguous fill with tolerance |
| `cropImage` / `calculateAspectCropRect` | Crop |
| `scissorsCut` | Keep or punch a polygon; optional crop to bounds |
| `removeBackground` | Magic-wand style color tolerance (not ONNX) |
| `cloneStamp` | Circular clone with hardness/opacity |
| `adjustColors` | Brightness, contrast, saturation, temperature |
| `applyBlur` / `applySharpen` | Spatial filters |
| `applyPresetFilter` | Grayscale, Sepia, Vintage, High Contrast |

Paint strokes are sampled from pointer movement (line segments between previous and current point). There is no Catmull-Rom stroke spline.

Image Inspector live preview writes a temporary asset via `Document::setImageLayerAsset`; committing (or cancelling) swaps back to the original or a final baked asset through history.

## 6. Canvas & interaction

`src/ui/canvasview.cpp` is the viewport (`CanvasView`). It currently also hosts most tool state (large file; see §16).

- **Pan & zoom:** Space+drag or middle-click; wheel / `+` `-` `1` / `F` fit. Zoom range is implemented in the view (about 10%–3200%).
- **Select:** click, Shift-add, rubber-band; transform handles (scale, rotate, move) for the primary or multi-selection.
- **Snap:** `SnapEngine` against canvas edges/centers and sibling bounds; optional snap-to-grid.
- **Overlays:** selection, multi-selection bounds, snap guides, grid, safe zones (`0` none, `1` YouTube, `2` Instagram, `3` TikTok), crop/scissors/clone previews, brush cursor.
- **Inline text:** double-click a text layer.
- **DnD:** dropped image files import as layers.
- **`CanvasTool`:** `Select`, `Crop`, `Scissors`, `MagicWand`, `CloneStamp`, `Paint`, `FloodFill`.

`MainWindow` wires menus, docks, tool option bars, align/distribute/group, and start-screen templates.

## 7. AI Background Removal

`src/core/image/BackgroundRemover.{h,cpp}` and `src/ui/aibackgrounddialog.{h,cpp}` (+ `maskeditcanvas` for mask touch-up). Quick removal can also run from `CanvasView`.

- **Engine:** ONNX Runtime C API.
- **Default model:** `u2netp.onnx`. User may pick another `.onnx` on disk.
- **Resolution order:** install/bundle `models/` → user data `CreatorCanvas/models/` → file picker.
- **Pipeline:** resize to 320×320, ImageNet normalize, NCHW inference, resize/feather mask, apply alpha.
- **Cancellation:** `cancelFlag` checked around inference and mask scaling.
- **UI:** dialog controls locked while a run is in flight; quick-AI uses a worker `QThread` and a shared atomic flag.

Setup scripts: `scripts/fetch_onnx.sh` (Linux), `scripts/fetch_onnx.bat` (Windows).

## 8. Internationalization

`src/localization/i18nservice.{h,cpp}`:

- Namespaces: `common`, `editor`, `settings`, `templates` under `resources/locales/<lang>/`.
- Languages: `en`, `pt-BR`.
- Runtime switch in Preferences; UI calls `retranslateUi`.
- Missing key → English catalog → humanized fallback + log warning.

## 9. Persistence — the `.creatorcanvas` format

ZIP archive:

```
manifest.json        document state, layers, metadata
thumbnail.png        ~320×180 preview for Recent Projects
media/<uuid>.png     embedded image assets
```

`cc::loadProjectThumbnail()` reads only `thumbnail.png` from the ZIP central directory (no full project inflate).

Saves write a temporary file and rename into place (atomic replace on the same volume).

### Zip-bomb / integrity

- Max entries: 1,024
- Max single entry: 100 MB
- Max total uncompressed: 500 MB
- Reject `..` and absolute entry paths
- Each media asset must match the SHA-256 stored in the manifest

## 10. Export

`src/imageio/DocumentExporter.{h,cpp}`:

- PNG (lossless), JPEG (flattened onto background), WebP
- Scale multiplier and JPEG/WebP quality
- Calls `renderDocument()` off-screen without editor overlays

## 11. Commands and undo/redo

`src/core/history/`:

- `Command` / `CommandStack`
- Typed helpers in `DocumentCommands.h` (layer properties, blend mode, image asset swap, …)
- Continuous edits (inspector sliders, live preview) can merge while dragging
- Raster tools commit a new asset + optional transform when the gesture ends

## 12. Testing & Verification

**19** QTest executables in `tests/CMakeLists.txt` (all run with `QT_QPA_PLATFORM=offscreen`):

1. `test_settings` — preferences persistence
2. `test_i18n` — catalogs, key symmetry, fallback
3. `test_logging` — log formatting
4. `test_layers` — hierarchy, z-order, clone
5. `test_document` — lifecycle, queries, bounds
6. `test_history` — undo/redo
7. `test_serialization` — round-trip and corrupt archives
8. `test_renderer` — off-screen `renderDocument`
9. `test_newdocument` — presets
10. `test_import` — sniff/decode + SHA-256 on assets
11. `test_transform` — affine math
12. `test_textbox` — wrap box
13. `test_effects` — outline/shadow raster
14. `test_export` — PNG/JPEG (and related) export
15. `test_autosave` — recovery files
16. `test_snap` — guide math
17. `test_image_processing` — filters and raster helpers
18. `test_ai_quick` — quick AI / canvas-adjacent paths (`canvasview`, AI dialog, mask canvas compiled in)
19. `test_ui_phase1` — start screen, layers panel, theme icons, collapsible sections

### Smoke test

```bash
QT_QPA_PLATFORM=offscreen CreatorCanvas --smoke-test
```

Checks catalog load, optional ONNX inference, in-memory document, `.creatorcanvas` save/load, thumbnail extract, PNG/JPEG/WebP export, and a headless `MainWindow` load.

## 13. Build & distribution

### CMake presets (`CMakePresets.json`)

- `linux-debug` — Debug, Ninja, warnings as errors
- `linux-release` — Release
- `linux-asan` — ASan + UBSan
- `windows-debug` / `windows-release`

### Packaging

- **Linux DEB:** `packaging/linux/build_deb.sh` (Qt ≥ 6.2). Bundles `libonnxruntime` under `lib/creatorcanvas/`.
- **AppImage:** `packaging/linux/build_appimage.sh`
- **Windows portable:** `packaging/windows/build_portable.ps1`
- **Windows NSIS:** `packaging/windows/installer.nsi`
- **CI:** `.github/workflows/release.yml` on version tags

Install rules also stage locales, presets, desktop/AppStream files, and the SVG icon.

## 14. Roadmap

Already on `main` (do not treat as future work): multi-select, group/align/distribute, grid, safe zones, templates, extra raster tools.

Still ahead:

1. **Split `CanvasView` / `MainWindow`** — isolate tools and overlays so the viewport is not a 2k+ line catch-all.
2. **Layer raster cache + `IRenderer`** — skip full recomposition; give a GPU backend a real seam (none exists today).
3. **GPU viewport** — optional Vulkan/Direct3D (or Qt RHI) behind that interface.
4. **Richer blend modes** — only if compositing moves beyond the current seven `QPainter` modes.
5. **Non-destructive adjustments** — store filter parameters on `ImageLayer` instead of baking pixels.
6. **macOS** — real bundle, notarization, and CI; `MACOSX_BUNDLE` on the target is not a shipping product.

## 15. Conventions

- Conventional Commits (`feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `chore:`) and/or milestone-tagged messages.
- No hardcoded user-visible strings; both `en` and `pt-BR` catalogs in the same change.
- Include-what-you-use; complete types for `std::unique_ptr` members of `Q_OBJECT` classes.
- Cross-platform paths via `QStandardPaths` / `QDir`. Keep new `#ifdef _WIN32` rare until a `platform/` module exists.
- Qualify `cc::` free functions when a class method of the same name would shadow them.

Logs live under the OS application data directory used by `LogService` / `SettingsService`.

## 16. Known limitations

- **sRGB only** — no wide-gamut or CMYK pipeline.
- **Still images only** — no timeline or video export.
- **Software compositor, no cache** — every paint walks the full tree; large documents will hitch.
- **Image adjustments are destructive** — inspector preview is live, but the stored document holds a new raster asset, not an adjustment stack (`ImageAdjustments` does not exist as a type).
- **Public `Layer` fields** — UI or tests can mutate `visible` / `transform` / etc. without a `Command`. That bypasses undo.
- **UI concentration** — most interaction lives in `canvasview.cpp` and `mainwindow.cpp`.
- **Blend/gradient surface smaller than older README copy** — 7 blends, linear+radial two-stop gradients, no conical.
- **No `platform/` isolation** — Windows vs Unix still appears in CMake and isolated `#ifdef`s (e.g. ONNX).
- **`Q_OBJECT` + `unique_ptr`** — incomplete type in the header still breaks the generated destructor; include the full type.
- **macOS** — not a supported distribution target yet.
