# CreatorCanvas — Technical Documentation

Technical reference for CreatorCanvas: architecture, data model, rendering, persistence, i18n, testing, build/packaging, conventions, and known limitations.

**Scope of this document:** current **`main`**. Last tagged release is **v0.2.1** (2026-11-04), which bundles milestones M0–M23 plus the M21–M23 feature work (multi-select, alignment/distribution/grouping, grid and safe-zone overlays, layout templates, extra raster tools — crop, scissors, magic wand, clone stamp, flood fill, gradient, retouch, healing, marquee —, the modular `cc_tools` architecture (`ICanvasTool`), and Photoshop-style layer drag-and-drop reordering with full undo/redo). The CMake `project()` version (`0.2.1`) matches the tag. `main` after the tag adds: Photoshop-style clipping masks (also inside groups, honoured by export), perspective mockups (4-point warp, free-form mode), the pen (Bézier) tool, batch processing, workspace layout save/restore, customizable keyboard shortcuts, performance work (scanline flood fill / magic wand, lazy asset encoding), and the Swedish (`sv`) translation contributed by the community. The post-`v0.2.1` feature set on this branch adds: **Select Subject (AI)** (non-destructive subject selection from the ONNX model), the **adjustments panel** (curves/levels via a shared `AdjustmentEngine`, with a non-destructive `AdjustmentLayer` path), **Layer Comps** and **Web Slices** panels with export, **numeric transform** (X/Y/W/H/rotation), and the **`IRenderer` seam with per-layer render cache**.

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

**Shipped in v0.2.1 (M0–M23):** document model, command stack, ZIP+JSON projects, software canvas renderer, transform gestures, layers panel, text + shape tools, settings, export, autosave, start screen, packaging, snapping, paint/fill, text gradients, shear, image inspector, ONNX background removal, multi-select, group/align/distribute, grid, safe zones, templates, crop/scissors/wand/clone/flood-fill/gradient/retouch/healing/marquee tools, the modular `cc_tools` architecture, Photoshop-style layer drag-and-drop, clipping masks, perspective mockups, pen (Bézier) tool, batch processing, workspace layouts, and customizable shortcuts.

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
| `m22-tools-dnd` | Modular tool system (`ICanvasTool`, `ToolContext`, `cc_tools` library) & Photoshop-style layer drag-and-drop with undo/redo |
| `m23-photocraft-tools` | Marquee selection (rect/ellipse, Shift add/Alt subtract, feather), Gradient (linear/radial, selection mask), Retouch brush (blur/sharpen/smudge/dodge/burn/sponge), Spot Healing + Red Eye, tool grouping and responsive toolbars |
| `v0.2.1` (tag, 2026-11-04) | Ships M0–M23 plus 147 fix/perf/CI commits: mockup perspective (real 4-corner warp, free-form, click order, live preview, Esc cancel), clipping masks (also inside groups, export-aware), pen (Bézier) tool, batch processing, workspace layouts, shortcut customization, scanline flood fill / magic wand, lazy asset encoding, and packaging/CI repairs (DEB smoke test, NSIS) |
| post-`v0.2.1` (`main`) | Responsive layout refactor (docks auto-hide/restore, compact toolbars), ActionManager shortcut registry with `QSettings` persistence and per-action reset, Swedish (`sv`) translation, clipping-mask fixes (base-layer alpha, refusal feedback) |

## 3. Architecture

### Module diagram

```
┌──────────────────────────────── ui/ ─────────────────────────────────────┐
│  MainWindow · StartScreen · LayersPanel · Text/Shape/Image inspectors    │
│  AIBackgroundDialog · MaskEditCanvas · Dialogs · CanvasView              │
└───────▲───────────────────────────────▲───────────────────▲──────────────┘
        │ user intents (commands)       │ active tool       │ repaint requests
        │                               ▼                   │
        │                       ┌─── tools/ ───┐            │
        │                       │ ICanvasTool  │            │
        │                       │ SelectTool   │            │
        │                       │ MarqueeTool  │            │
        │                       │ CropTool     │            │
        │                       │ ScissorsTool │            │
        │                       │ PenTool      │            │
        │                       │ PaintTool    │            │
        │                       │ RasterTools  │            │
        │                       │ PaintExtras  │            │
        │                       └───────┬──────┘            │
        │                               │ delegate ops      │
┌───────┴──────────── core/ ────────────▼───────────────────┴─────── rendering/ ──┐
│  Document · Layer tree · CommandStack · LayerComps · WebSlice            │
│  ProjectFile (ZIP+JSON+Thumbnail) · AssetStore · ColorSpace              │
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
| `cc_tools` | `tools/*` (`ICanvasTool`, tools suite), links `cc_core` |
| `cc_rendering` | `rendering/CanvasRenderer.*` + `rendering/IRenderer.*` |
| `cc_services` | settings, autosave, recents, log, presets |
| `cc_localization` | i18n + locale `.qrc` |
| `creatorcanvas` | `main.cpp` + `ui/*` (output name `CreatorCanvas`) |

There is **no** `src/platform/` tree and **no** `core/paint/` module — raster drawing lives in `cc::ImageProcessing`. The compositor interface exists since o cache de camadas: `rendering/IRenderer.h` (ver §5).

### Principles

- **Separation of concerns:** `core/` must not include QtWidgets, so unit tests and CI can run with `QT_QPA_PLATFORM=offscreen`.
- **Command pattern:** mutating `Document` / layer structure should go through `core/history/Command`. Undo stores property deltas, not full document clones. `Layer` still has public fields — treat those as a read path, not a second write path (see §16).
- **Async cancellation:** ONNX inference inspects `std::atomic<bool>` cancellation flags so the UI can abort.
- **Archive safety:** zip-bomb limits on entry count and uncompressed size; path traversal rejected; embedded media SHA-256 verified on load.

## 4. Data model & Layers

### Document

`src/core/Document.h` owns the root `GroupLayer`, `AssetStore`, canvas width/height/DPI, the document color space (`ColorSpace`), the pixel selection, Layer Comps (`LayerComp`), Web Slices (`WebSlice`), and a monotonically increasing `revision()`.

### Layer types

All types derive from `cc::Layer` (`src/core/layers/Layer.h`) — **eight** types in total:

- `GroupLayer` — children in paint order (index 0 = bottom).
- `ImageLayer` — `assetId` into `AssetStore` plus `naturalWidth` / `naturalHeight`, an optional non-destructive `alphaMask`, `ImageEffects` (outline, drop shadow, inner shadow, color overlay), an optional `adjustmentLayerId` link (non-destructive adjustments), a per-layer `colorSpace`, and transient render caches (never serialized).
- `TextLayer` — content, font, box, alignment, `TextEffects` (outline, shadow, gradient).
- `ShapeLayer` — 18 kinds (rectangle, rounded rect, ellipse, line, polygon/arrow/star/badge/speech bubble/heart/triangle/diamond/pentagon/hexagon/cross/checkmark/cloud/explosion); fill or gradient, stroke, corner radius, dashed stroke, star tuning, `ShapeShadow`.
- `BackgroundLayer` — solid fill or gradient.
- `FillLayer` — procedural solid/gradient/pattern fill with dither and layer-alignment options.
- `AdjustmentLayer` — procedural adjustment applied to everything below it (or to the linked image layer when used as a clipping mask): Levels, Curves, Brightness/Contrast, Hue/Saturation, Shadow/Highlight, Vignette, Color Balance, Photo Filter, Channel Mixer, Invert, Threshold, Posterize, Gradient Map (13 types; new ones are always appended to the enum so integers written by older project files keep their meaning — `ProjectFile` bounds-checks against the enum, not a hardcoded count). Parameters are serialized as key/value pairs.
- `VectorLayer` — cubic Bézier path with editable nodes (`VectorNode`), optional fill and/or stroke.

### Shared properties

- `name`, `visible`, `locked` (public fields; `LayerLocks` adds granular transparency/pixels/position/all locks, with group locks inherited by descendants)
- `opacity` — `[0, 1]`, via getter/setter
- `fillOpacity` — `[0, 1]`, affects content only (not effects), like Photoshop
- `blendMode` — see below
- `blendIf` — `BlendIf`: per-channel blend ranges (under/over) applied by the renderer
- `colorTag` — 8 organizing colors
- `clipped` — clipping mask: the layer is masked by the alpha of the accumulated content below it (see §6)
- `linkGroup` — layers sharing a link group move together
- `vectorMask` — optional mask image applied together with `alphaMask`
- `transient` — in-memory only (dialog previews); never serialized
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

All **twelve** blend modes defined in the enum are currently mapped to `QPainter` composition modes (the seven above plus ColorDodge, ColorBurn, HardLight, SoftLight, Difference and Exclusion), and all twelve are offered by the Layers panel combo. (Older revisions of this document listed only seven as mapped.)

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

### Layer Comps & Web Slices

Both are modelled in `core/document/` (`LayerComps.h`, `WebSlice.h`), stored on the `Document`, and round-tripped by `ProjectFile` (manifest keys `layerComps` / `webSlices`). They follow the PhotoCraft schema:

- `LayerComp` — named snapshot of per-layer state flags (`applyVisibility`, `applyPosition`, `applyAppearance`) plus `CompLayerState` deltas (visibility, position, opacity, blend mode). `compFromDocument()` captures the current state of the whole tree; `Layer ▸ Layer Comps` opens the panel (save / apply / delete).
- `WebSlice` — named rectangle (`SliceKind::Image` / `NoImage`) with a target `exportFormat` (`png`, `jpg`, `webp`). `File ▸ Web Slices` opens the panel; `exportSliceToImage()` renders each slice.

All mutations go through `LayerCompCommands.h` (`executeAddLayerComp`, `executeApplyLayerComp`, `executeRemoveLayerComp`, `executeAddWebSlice`, `executeRemoveWebSlice`). Applying a comp is **one** undo step: `ApplyLayerCompCommand` snapshots the pre-state of every touched layer. Slices are created from the active selection (its bbox is mapped through the layer transform into document space) or by numeric X/Y/W/H.

## 5. Rendering & raster processing

### Software compositor

`cc::renderDocument()` in `src/rendering/CanvasRenderer.cpp`:

1. Walks the layer tree bottom-up (`composeLayers()` is recursive: a group that needs compositing is rendered into its own buffer, so masks apply between siblings of that group).
2. Multiplies group opacity down the tree; applies `fillOpacity` to content only.
3. Sets `QPainter` transform from `AffineTransform::matrix(contentBounds)`.
4. Sets composition mode from `BlendMode` and applies `BlendIf` ranges.
5. Applies clipping masks: a clipped layer is composed in isolation and masked with the alpha of the accumulated content below (`CompositionMode_DestinationIn`).
6. Draws background fill, procedural fills, shapes, text (with effects), Bézier paths, or the image asset — with alpha/vector masks, and converting the layer's `colorSpace` to the document space when `isColorConversionSupported()` allows.
7. `AdjustmentLayer` parameters are applied to the accumulated result (or to the linked image layer) at render time.

Export and the viewport share this function; `CanvasView` then draws editor overlays (handles, guides, grid, safe zones, tool cursors) on top.

The compositor still recomposes on every paint of a *changed* document, but unchanged layers no longer pay rasterization again:

- `IRenderer` (`rendering/IRenderer.h`) is the compositor seam: `renderDocument(doc, painter, transform, options)` + `clearCaches()`, with `name()` for diagnostics. `SoftwareRenderer` is the historical QPainter implementation; `CachingRenderer` adds a per-layer pixmap cache and is the process default — the free `renderDocument()` delegates to it, so canvas, exporters and the smoke test all benefit with zero call-site changes.
- The cache only applies to the **fast path** (no clipping masks, no visible adjustment layers) and only to layers whose rasterization is expensive: **Text, Shape, Vector and Fill**. Image layers have their own mask caches and are excluded; groups, adjustments, clipped layers, layers with alpha/vector masks, `BlendIf`, or the live tool preview are also excluded rather than risk a stale pixmap.
- Cache key: (layer id, `Document::revision()`, device canvas size, `interactive` mode). The revision is monotonic, so any document edit invalidates every old entry — stale pixmaps are impossible by construction. Budget: 16 entries / 64 MB with LRU eviction; `invalidateLayerCache(id)` / `invalidateAllLayerCaches()` / `clearRendererCaches()` drop entries explicitly.
- `ImageLayer` additionally keeps two **transient** caches (full-res and interactive ≤2048 px preview) for masked bitmaps, keyed by asset/mask revisions.

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
| `paintGradient` | Linear/radial gradient between two colors, optional alpha and selection mask |
| `makeShapeSelection` / `combineSelections` | Rectangle/ellipse pixel selections (Alpha8), feather blur, add/subtract union |
| `retouchStroke` | Local brush retouch: Blur, Sharpen, Smudge, Dodge, Burn, Sponge |
| `spotHeal` / `redEyeRemove` | Auto-source healing brush with color transfer; red-pupil neutralization |
| `adjustColors` | Brightness, contrast, saturation, temperature |
| `applyBlur` / `applySharpen` | Spatial filters |
| `applyPresetFilter` | 11 presets: Grayscale, Sepia, Vintage, High Contrast, Vivid Pop, Cyberpunk, Golden Hour, Dramatic Noir, Invert, Posterize, Threshold (`ImagePresets`) |

Paint strokes are sampled from pointer movement (line segments between previous and current point). There is no Catmull-Rom stroke spline.

Image Inspector live preview writes a temporary asset via `Document::setImageLayerAsset`; committing (or cancelling) swaps back to the original or a final baked asset through history.

## 6. Canvas & interaction

`src/ui/canvasview.cpp` is the viewport (`CanvasView`).

- **Pan & zoom:** Space+drag or middle-click; wheel / `+` `-` `1` / `F` fit. Zoom range is implemented in the view (about 10%–3200%).
- **Select:** click, Shift-add, rubber-band; transform handles (scale, rotate, move) for the primary or multi-selection.
- **Select Subject (AI):** `Select ▸ Select Subject (AI)` (`Ctrl+Alt+S`) turns the ONNX silhouette into a pixel selection instead of a cutout — the layer keeps its background and the subject can be copied to a new layer, feathered, inverted or composited. See §7.
- **Snap:** `SnapEngine` against canvas edges/centers and sibling bounds; optional snap-to-grid.
- **Overlays:** selection, multi-selection bounds, snap guides, grid, safe zones (`0` none, `1` YouTube, `2` Instagram, `3` TikTok), **web-slice rects with name/format labels** (drawn whenever the document has slices — the panel is the model, the canvas is the feedback), crop/scissors/clone previews, brush cursor.
- **Inline text:** double-click a text layer.
- **DnD:** dropped image files import as layers.
- **Clipping mask (Photoshop-style):**
  - `Layer::clipped` marks a layer as clipped to the layer below it. The
    renderer composes the layer in isolation and masks it with the alpha of
    the accumulated content below (`CompositionMode_DestinationIn`).
  - **Works inside groups** too: `composeLayers()` (`CanvasRenderer.cpp`)
    is recursive — a group whose subtree needs compositing is rendered into
    its own buffer so masks apply between siblings of that group.
  - **UI:** the layers panel has a clip toggle button; `Layer ▸ Create
    Clipping Mask` (`Ctrl+Alt+G`) and `Layer ▸ Release Clipping Mask`
    (`Ctrl+Shift+Alt+G`) are also available. Clipped rows show a `↳`
    prefix, like Photoshop.
  - **Rules enforced by `Document::setLayerClipped()`:** a clipped layer
    needs a visible, non-clipped layer below it in the same group, and
    adjustment layers cannot be clipped in isolation (they already act on
    the accumulated result).
  - **Export:** exporting a clipped layer (`renderSiblingLayers`) applies
    the same mask, so the exported image matches what is on screen.
- **`CanvasTool` & `ICanvasTool` architecture:**
  - Located in `src/tools/` compiled as static library `cc_tools`.
  - `ICanvasTool` lifecycle: `activate(ctx)`, `deactivate(ctx)`, `drawOverlay(painter, ctx)`, `cursor()`.
  - Input event dispatch: `mousePress`, `mouseMove`, `mouseRelease`, `mouseDoubleClick`, `keyPress`.
  - `ToolContext` encapsulates references to `Document`, `CommandStack`, viewport zoom/pan transforms, selection, and active tool settings.
  - Tools implemented: `SelectTool`, `CropTool`, `PaintTool`, `ScissorsTool`, `PenTool` (cubic Bézier paths), `MagicWandTool`, `CloneStampTool`, `FloodFillTool` (all in `RasterTools.h`), `MarqueeTool` (rect/ellipse), `GradientTool` and `RetouchTool` (6 modes) (both in `PaintExtras.h`), `HealingTool` (spot heal / red eye). The eyedropper is handled by the view, not a tool class.
  - Every raster tool previews through `ToolContext::setLayerPreview` and commits **one** undo step (`ModifyImageLayerCommand`, via `tools/ToolCommit.h`); without a `CommandStack` the edit is applied directly so it can never be lost.
  - Selection convention: the pixel selection (`Document::pixelSelection`) lives in the **active layer bitmap space** (same as wand/scissors), so masking/`applyMask` stays consistent; with no image layer selected the marquee uses document space. The AI subject selection (§7) produces its mask in the same space through `SelectionEngine::fromAlphaMask()`.
  - Shortcuts: `V` select, `M` marquee, `X` scissors, `W` wand, `C` crop, `B` brush, `G` gradient, `Shift+G` bucket, `S` clone, `U` retouch, `J` healing, `I` eyedropper, `P` pen (Bézier vector paths — creating a `VectorLayer`).
  - Tool groups are separated in the toolbar (selection / cut / paint / retouch / utility / create) and mirrored in the **Tools** menu.
  - Viewport delegates mouse and paint events to `m_activeTool` before legacy fallback handlers. Every `CanvasToolType` in the enum is wired to its own class (`Vector` → `PenTool`); a tool without classe própria é um bug, não um fallback.
  - **Numeric transform bar** (`TransformBar`, on the tool-options row): X/Y/W/H/rotation of the selected layer. W/H set scale from the natural size (image layers only; other types keep the fields disabled rather than lying), aspect ratio locked by default, mirror sign preserved. Each edit is one `SetLayerTransformCommand` (mergeable — dragging a spin is a single undo step). The bar refreshes on undo/redo.
- **Photoshop-style Layer Drag & Drop:**
  - `LayerRowWidget` captures mouse drag exceeding `QApplication::startDragDistance()`, generates a semi-transparent drag pixmap of the row, and sets MIME data `application/x-creatorcanvas-layer-id`.
  - `LayerListWidget` provides dynamic drop indicator and computes target index based on cursor position relative to item vertical midpoint.
  - Layer reordering executes through `ReorderLayerCommand` on the `CommandStack`, preserving non-destructive undo/redo history.
- **Adjustments panel** (`Image ▸ Adjustments…`) — 11 entries: Brightness/Contrast, Hue/Saturation, Shadows/Highlights, Vignette, Invert, Posterize, Threshold, Levels (per channel), Curves (per channel, spline shown from the same LUT the engine uses), Channel Mixer, Gradient Map. Live preview (scaled to 480 px), non-destructive by default.
- **Web-slice preview** — the slices panel renders a miniature of the document with each slice rect and label drawn on top (same colour cycling as the canvas), plus the **pending** rectangle described by the X/Y/W/H fields, updated live on every keystroke/spinner change (solid accent outline + `nova [FORMAT]` label), so the final position is judged *before* adding the slice.
- **Workspace layouts:** `Window ▸ Workspace Layout` (`WorkspaceLayoutDialog`) saves the current dock geometry/state under a user-chosen name and restores or deletes saved layouts; layouts persist in `SettingsService`.
- **Customizable shortcuts:** every registered action is listed in `Help ▸ Keyboard Shortcuts` (`ShortcutDialog`); individual shortcuts can be reassigned, reset to default, or reset in bulk. Defaults are captured at startup and user overrides persist in `QSettings` (`ActionManager::persistShortcut`).
- **Perspective mockup:** `ImageInspector` → Mockup (`MockupDialog`) projects the layer onto a 4-point perspective warp (Photoshop-Distort semantics, click order preserved, free-form mode, live preview, `Esc` cancels). Transient layer, never serialized.
- **Batch processing:** `File ▸ Batch` (`BatchDialog`) applies a set of operations to a list of input files into an output directory, with overwrite protection.
- **Puppet warp:** `PuppetWarpDialog` for mesh-based deformation.

`MainWindow` wires menus, docks, tool option bars, align/distribute/group, start-screen templates, mockups, batch processing, workspace layouts, layer comps (`Layer ▸ Layer Comps`) and web slices (`File ▸ Web Slices`).

### Responsive layout

Breakpoints and tokens live in `src/ui/designtokens.h` (`BreakXS` 620, `BreakSM` 980, `BreakMD` 1440, `UltraCompactWidth` 840, `EditorMinWidth` 800, `HintBreakWidth` 1380).

- **Toolbars:** below `BreakSM`, or when the window height cannot fit the 14 vertical tools (`< 760px` and width `< 1300`), the left toolbar is replaced by a horizontal compact toolbar that shares the *same* `QAction`s. The tool options bar always gets its own row.
- **Tool options:** under 1100 px the option labels are hidden and sliders shrink to 64 px, so controls are not clipped; the bar is horizontally scrollable as a last resort.
- **Docks:** at `<= UltraCompactWidth` all inspectors except Layers are auto-hidden, with the user's previous visibility saved and restored when the window grows (Window ▸ Panels still works).
- **Canvas zoom:** `CanvasView` keeps a `m_autoFit` mode. While fit, a resize re-fits the document; after any manual zoom/pan a resize preserves the zoom and keeps the content centered. Changing tools never resets zoom.
- **Dialogs/inspectors:** text/shape inspectors are scrollable; New Document/Export/Settings are resizable within screen-aware maximums.
- **Verification:** `--ui-snapshot <dir>` renders the editor at 1600×1000, 1280×800, 1024×700, 900×620 and 800×520 for each of select/marquee/gradient/retouch/healing, so layout regressions are checkable headlessly.

## 7. AI Background Removal

`src/core/image/BackgroundRemover.{h,cpp}` and `src/ui/aibackgrounddialog.{h,cpp}` (+ `maskeditcanvas` for mask touch-up). Quick removal can also run from `CanvasView`.

- **Engine:** ONNX Runtime C API.
- **Default model:** `u2netp.onnx`. User may pick another `.onnx` on disk.
- **Resolution order:** install/bundle `models/` → user data `CreatorCanvas/models/` → file picker.
- **Pipeline:** resize to 320×320, ImageNet normalize, NCHW inference, resize/feather mask, apply alpha.
- **Cancellation:** `cancelFlag` checked around inference and mask scaling.
- **UI:** dialog controls locked while a run is in flight; quick-AI uses a worker `QThread` and a shared atomic flag.

### Two result modes

The same inference drives two outcomes, selected by the caller (`AiBackgroundDialog`'s `selectionMode` flag):

| Mode | Entry point | Result |
|---|---|---|
| Remove background | `Layer ▸ Remove Background with AI` (canvas context menu, inspector) | The layer receives a non-destructive alpha mask (`LayerPropertyCommand`, one undo step) |
| Select Subject | `Select ▸ Select Subject (AI)` (`Ctrl+Alt+S`) | `Document::pixelSelection` receives the subject as an Alpha8 selection (`SetPixelSelectionCommand`, one undo step) — the layer itself is untouched |

In selection mode the background-color preview control is hidden (irrelevant) and the apply button becomes *Create Selection*. The dialog still exposes the mask touch-up brush (restore/erase), cut level and speck removal, so the silhouette can be refined before it becomes a selection. Conversion from the neural mask to layer space goes through `SelectionEngine::fromAlphaMask()` — smooth-rescale to the layer bitmap plus the usual `SelectionMode` combination semantics.

Setup scripts: `scripts/fetch_onnx.sh` (Linux), `scripts/fetch_onnx.bat` (Windows).

## 8. Internationalization

`src/localization/i18nservice.{h,cpp}`:

- Namespaces: `common`, `editor`, `settings`, `templates` under `resources/locales/<lang>/`.
- Languages: `en` (reference), `pt-BR` (complete), `sv` (community contribution; 38 of 384 editor keys — mostly export presets, tools and filters — are still English placeholders pending translation).
- New language directories under `resources/locales/` are discovered automatically by `I18nService`; every JSON file must also be listed in `resources/locales.qrc` so it is embedded in the application.
- Translation contributions are welcome. See [Adding or updating a translation](CONTRIBUTING.md#-adicionando-ou-atualizando-uma-tradução).
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

The manifest also carries the document color space, the pixel selection, and the `layerComps` / `webSlices` arrays (data model only — see §4 and §16).

### Zip-bomb / integrity

- Max entries: 2,000
- Max single entry: 128 MB
- Max total uncompressed: 1 GB
- Reject `..` and absolute entry paths
- Each media asset must match the SHA-256 stored in the manifest

## 10. Export

`src/imageio/DocumentExporter.{h,cpp}`:

- PNG (lossless), JPEG (flattened onto background), WebP
- Scale multiplier and JPEG/WebP quality
- Calls `renderDocument()` off-screen without editor overlays
- Exporting a clipped layer applies the same mask (`renderSiblingLayers`), so the file matches the screen

Batch runs over many files at once are driven by `BatchDialog` (UI layer), which reuses the same import/render/export pipeline.

**Web slices** (`exportSliceToImage`): each slice renders the whole document translated so the slice rect fills the output image (`scale` supported, JPEG quality honoured), transparent outside the slice content. The slices panel exports every `SliceKind::Image` slice in one go to a chosen directory.

## 11. Commands and undo/redo

`src/core/history/`:

- `Command` / `CommandStack`
- Typed helpers in `DocumentCommands.h` (layer properties, blend mode, image asset swap, …) and `SelectionCommands.h` (`SetPixelSelectionCommand` — selection is a first-class history citizen: create/clear/invert all undo with `Ctrl+Z`)
- Continuous edits (inspector sliders, live preview) can merge while dragging
- Raster tools commit a new asset + optional transform when the gesture ends

## 12. Testing & Verification

**40** QTest executables in `tests/CMakeLists.txt` (all run with `QT_QPA_PLATFORM=offscreen`), in registration order:

1. `test_settings` — preferences persistence
2. `test_i18n` — catalogs, key symmetry, fallback, and a **source scan** that fails if any `t("section", "key")` used in `src/` is missing from either catalog (regression: the status bar showed the raw key `editor.canvas.status.paintHelp`)
3. `test_logging` — log formatting
4. `test_layers` — hierarchy, z-order, clone
5. `test_document` — lifecycle, queries, bounds
6. `test_history` — undo/redo
7. `test_serialization` — round-trip and corrupt archives
8. `test_renderer` — off-screen `renderDocument`, adjustments rendering (levels/curves/channel mixer)
9. `test_newdocument` — presets
10. `test_import` — sniff/decode + SHA-256 on assets
11. `test_transform` — affine math
12. `test_textbox` — wrap box
13. `test_effects` — outline/shadow raster
14. `test_export` — PNG/JPEG (and related) export
15. `test_autosave` — recovery files
16. `test_snap` — guide math
17. `test_image_processing` — filters and raster helpers
18. `test_tools` — modular tool activation, event delegation, and context handling
19. `test_tools_extras` — marquee/gradient/retouch/healing behaviour, undo steps and raster ops
20. `test_ai_quick` — quick AI / canvas-adjacent paths (`canvasview`, AI dialog, mask canvas compiled in)
21. `test_ui_phase1` — start screen, layers panel, theme icons, collapsible sections
22. `test_adjustments` — adjustment engine (levels/curves params, strength blend), non-destructive apply through the command stack (create/link/undo/redo), reuse of a linked layer in one undo step, the destructive bake path, rejection of non-image layers, and the Shadows/Highlights (luminance-weighted), Vignette (edge-weighted) and Channel Mixer (per-channel gain) types
23. `test_photocraft` — pixel selection, layer comps/web-slices round-trip, comp apply/undo/redo and removal, slice creation and single-slice export
24. `test_selection_engine` — magic wand contiguous vs global, feathering, selection combination, and the AI subject-mask → Alpha8 selection conversion (`fromAlphaMask`)
25. `test_layer_render` — per-layer raster rendering
26. `test_hit_alpha` — alpha-aware hit testing
27. `test_canvas_zoom` — fit mode refits on resize, manual zoom is preserved
28. `test_undo_render` — undo/redo render consistency
29. `test_template_textfit` — template text fit rules
30. `test_command_locks` — command stack locking semantics
31. `test_alignment` — layer alignment/distribution
32. `test_layer_operations` — layer add/remove/duplicate
33. `test_canvas_context_menu` — context-menu target resolution per layer type
34. `test_inline_text_editor` — inline text editor initial state and commit
35. `test_export_dialog` — export dialog presets/chips synchronization
36. `test_canvas_gesture_handler` — transform handle computation and hit testing
37. `test_image_presets` — preset filter pixel behaviour
38. `test_mockup_dialog` — mockup dialog state (logo disabled until a source is set)
39. `test_mainwindow_tabs` — selecting a layer keeps the active inspector tab; adjustments dialog request contract (non-destructive by default)
40. `test_overlays` — web-slice overlay: nothing drawn without slices, rect + label drawn with slices (clamped inside the view), multiple slices cycle colours and invalid rects are ignored

Several UI tests compile the corresponding `src/ui/*.cpp` file directly into the test executable instead of linking `creatorcanvas` (e.g. `test_mainwindow_tabs` compiles `mainwindow.cpp` and its dialog dependencies). When adding a test, keep this pattern and keep the symbols single-defined — a duplicated non-inline definition across translation units is an ODR violation (this bit `HealingTool` once already).

### Smoke test

```bash
QT_QPA_PLATFORM=offscreen CreatorCanvas --smoke-test
```

Checks catalog load, optional ONNX inference, in-memory document, `.creatorcanvas` save/load, thumbnail extract, PNG/JPEG/WebP export, and a headless `MainWindow` load.

### UI snapshot (layout regression)

```bash
QT_QPA_PLATFORM=offscreen CreatorCanvas --ui-snapshot /tmp/shots
```

Renders the editor at 1600×1000, 1280×800, 1024×700, 900×620 and 800×520 for every tool (13 tools → 65 PNGs) so responsive layout can be reviewed headlessly through the same `MainWindow` the user runs.

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

Already on `main` (do not treat as future work): multi-select, group/align/distribute, grid, safe zones, templates, extra raster tools, modular `cc_tools` architecture, Photoshop-style layer drag-and-drop, clipping masks, perspective mockups, pen (Bézier) tool, batch processing, workspace layouts, customizable shortcuts, the adjustments panel (curves/levels with a non-destructive path), layer comps and web slices with export.

Still ahead:

1. **Split `CanvasView` / `MainWindow`** — isolate tools and overlays so the viewport is not a 2k+ line catch-all (Phase 1 delivered: `cc_tools` static library and `ICanvasTool` architecture integrated).
2. ~~Layer raster cache + `IRenderer`~~ — **done for the fast path**: `IRenderer` seam exists, `CachingRenderer` caches text/shape/vector/fill layers per revision. Still open: caching the compositing path (clipping masks/adjustments) and a GPU backend behind the same interface.
3. **GPU viewport** — optional Vulkan/Direct3D (or Qt RHI) behind that interface.
4. **Richer blend modes** — only if compositing moves beyond the current seven `QPainter` modes.
5. **Inspector integration for adjustments** — the dialog edits parameters, but the image inspector has no live section bound to the linked `AdjustmentLayer`.
7. **macOS** — real bundle, notarization, and CI; `MACOSX_BUNDLE` on the target is not a shipping product.

## 15. Conventions

- Conventional Commits (`feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `chore:`) and/or milestone-tagged messages.
- No hardcoded user-visible strings; every catalog shipped with the app (`en`, `pt-BR`, `sv`) gets the key in the same change.
- Include-what-you-use; complete types for `std::unique_ptr` members of `Q_OBJECT` classes.
- Cross-platform paths via `QStandardPaths` / `QDir`. Keep new `#ifdef _WIN32` rare until a `platform/` module exists.
- Qualify `cc::` free functions when a class method of the same name would shadow them.

Logs live under the OS application data directory used by `LogService` / `SettingsService`.

## 16. Known limitations

- **No ICC profile pipeline** — the document and layers *model* a color space (`sRGB`, Adobe RGB, Display P3, CMYK, Grayscale, Lab) and the renderer converts when `isColorConversionSupported()` allows it, but there is no ICC profile loading or colour management engine.
- **Still images only** — no timeline or video export.
- **Software compositor, no general cache** — every paint of a dirty document walks the full tree (masked image layers have transient caches); large documents will hitch.
- **The inspector adjustment path is destructive** — preview is live, but committing bakes a new raster asset. The non-destructive `AdjustmentLayer` type exists and renders at render time; there is no inspector integration.
- **Public `Layer` fields** — UI or tests can mutate `visible` / `transform` / etc. without a `Command`. That bypasses undo.
- **UI concentration** — most interaction lives in `canvasview.cpp` and `mainwindow.cpp`.
- **Marquee + transformed layers** — the marquee is mapped through the layer inverse transform and collapsed to a bounding rectangle, so a rotated layer gets an axis-aligned selection in its own bitmap space.
- **Retouch/Healing run on the UI thread** — the brush patch is small, but each stroke event still converts/duplicates the full layer bitmap; multi-megapixel layers can feel heavy.
- **Spot Healing auto-source** — the source is sampled from a fixed offset to the right (clamped to bounds); there is no Alt-click source picking or patch-match synthesis.
- **Blend/gradient surface** — all 12 blend modes are mapped, but gradients are two-stop linear/radial only, no conical.
- **No `platform/` isolation** — Windows vs Unix still appears in CMake and isolated `#ifdef`s (e.g. ONNX).
- **`Q_OBJECT` + `unique_ptr`** — incomplete type in the header still breaks the generated destructor; include the full type.
- **macOS** — not a supported distribution target yet.
