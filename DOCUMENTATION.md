# CreatorCanvas — Technical Documentation

This document is the technical reference for CreatorCanvas: architecture, data model, rendering pipeline, persistence format, internationalization, testing, build/packaging, conventions, and known limitations. It reflects the state of the codebase for **v0.2.0** (Milestones M0 through M20 complete).

## Table of contents

1. [Overview](#1-overview)
2. [Milestone history](#2-milestone-history)
3. [Architecture](#3-architecture)
4. [Data model & Layers](#4-data-model--layers)
5. [Rendering pipeline & Paint Engine](#5-rendering-pipeline--paint-engine)
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

CreatorCanvas is a modern, layer-based image editor crafted specifically for digital content creators. The core workflow it optimizes for is: *pick a platform canvas preset (YouTube, Instagram, TikTok, Facebook, X) → compose images, shapes, and brush strokes → add rich typographic text with gradients and effects → apply non-destructive color adjustments and AI background removal → export high-fidelity images (PNG, JPEG, WebP)*.

**Current state (v0.2.0):** Milestones M0 through M20 are fully implemented, stabilized, and verified:
- **Phase 1 (M0–M14):** Core document model, undo/redo stack, serialization, software canvas renderer, transform gestures, layers panel, text tool with shadow/outline, settings, export, autosave, start screen with recent projects, and packaging base.
- **Milestones M15–M20:**
  - **M15:** Smart guides & snapping (canvas boundary and inter-layer edge/center alignment actively rendered on canvas during drag).
  - **M16:** Raster brush paint engine (Round, Square, Marker, Airbrush, Eraser) with stroke smoothing.
  - **M17:** Full blend modes (Normal, Multiply, Screen, Overlay, Darken, Lighten, Color Dodge, Color Burn, Hard Light, Soft Light, Difference, Exclusion) and layer opacity.
  - **M18:** Non-destructive image adjustments (Brightness, Contrast, Saturation, Hue, Exposure, Temperature/Tint, Invert, Blur).
  - **M19:** Advanced text effects: linear & radial multi-stop gradients, angle control, and 3D perspective tilt simulation.
  - **M20:** AI Background Removal via ONNX Runtime (`u2netp.onnx` / custom models) with multi-stage cooperative cancellation (`std::atomic<bool>`), UI safety locks, and automated smoke testing.

**Stack:** C++20, Qt 6 (Widgets), ONNX Runtime (C API), CMake with presets, QTest, ZIP+JSON project archives with `miniz`.

## 2. Milestone history

| Milestone | Delivered |
|---|---|
| `m0` / `m1` | Application skeleton — CMake project, logging (`LogService`), settings persistence (`SettingsService`), core document/layer model + serialization |
| `m4-canvas` | Canvas MVP: pan/zoom, checkerboard, software renderer, status bar |
| `m5-new-document` | New Document dialog — social presets, geometry, background fills |
| `m6a-import` | Image import, asset store, media embedded in project archive, undo/redo |
| `m6b-transform` | `AffineTransform`, `contentBounds`, interactive selection handles, gestures, Layer menu |
| `m7-layers-panel` | Layers panel: drag reorder, visibility toggle, lock toggle, context menu, focus-on-double-click |
| `m8-text` | Text tool: add text (Ctrl+T), inline WYSIWYG editing, property inspector, text wrap box |
| `m9-text-effects` | Text effects: outline (with width) and drop shadow (with blur), cached, non-destructive, serialized |
| `m10-save-open` | Save/Open/Save As UI, unsaved-changes prompt, settings dialog (language + autosave interval) |
| `m11-export` | Export UI: PNG/JPEG/WebP with quality and scale, headless offscreen renderer |
| `m12-autosave` | Autosave engine + emergency session recovery |
| `m13-start-screen` | Start screen: preset cards, recent projects list with visual thumbnails, custom canvas creation flow |
| `m14-packaging` | Linux DEB packaging, AppStream metadata, SVG icon, Windows portable ZIP packaging |
| `m15-snap` | Smart guides & snapping engine (`SnapEngine`) + active on-canvas cyan visual guides during drag/resize |
| `m16-paint` | Raster brush drawing engine: Round, Square, Marker, Airbrush, Eraser, color picker, stroke smoothing |
| `m17-blend-modes` | Blend modes integration: 12 blending modes, opacity slider, compositing engine |
| `m18-adjustments` | Non-destructive image adjustment pipeline (brightness, contrast, saturation, hue, warmth, blur) |
| `m19-text-styling` | Text gradient fills (linear, radial, angle, colors) and 3D perspective tilt simulation |
| `m20-ai-removal` | AI Background Removal via ONNX Runtime (`u2netp.onnx`), cooperative cancellation, UI control locking, full `--smoke-test` validation |

## 3. Architecture

### Module diagram

```
┌──────────────────────────────── ui/ ─────────────────────────────────────┐
│  MainWindow · StartScreen · LayersPanel · TextInspector · BrushPanel     │
│  AIBackgroundDialog · Dialogs (New/Export/Settings) · CanvasView         │
└───────▲───────────────────────────────────────────▲──────────────────────┘
        │ user intents (commands)                   │ repaint requests
┌───────┴──────────── core/ ────────────────────────┴─────── rendering/ ──┐
│  Document · Layer tree · CommandStack (history)                          │
│  ProjectFile (ZIP+JSON+Thumbnail) · AssetStore                           │
│  AffineTransform (geometry) · SnapEngine                                 │
│  ImageAdjustments · BackgroundRemover (ONNX Runtime)                     │
│                                                                          │
│  CanvasRenderer — software renderer (QPainter, blend modes, effects)     │
│  PaintEngine — brush stroke rasterizer (core/paint/)                     │
└──────────────────────────────────────────────────────────────────────────┘
   services/     (SettingsService · AutosaveService · RecentFiles · LogService)
   imageio/      (ImageImporter · DocumentExporter)
   localization/ (I18nService + JSON catalogs)
```

### Architectural Principles

- **Separation of Concerns:** `core/` contains no `QtWidgets` references, enabling 100% headless testing with `ctest` and automated CI runs without an X11/Wayland display server.
- **Command Pattern:** All mutating operations against `Document` or `Layer` must pass through `core/history/Command` instances. Undo and redo record minimal property deltas rather than full state clones.
- **Thread Safety & Cancellation:** Heavy asynchronous operations (such as AI background segmentation) run on worker threads using `std::atomic<bool>` cooperative cancellation flags, ensuring the UI remains responsive and aborts cleanly.
- **Resource Attribution & Safety:** Archive extraction uses strict size and count guards to prevent zip-bomb attacks; media assets are verified by SHA-256 hashes.

## 4. Data model & Layers

### Document
`src/core/Document.h` manages the root layer group (`GroupLayer`), the asset dictionary (`AssetStore`), canvas dimensions (width, height, DPI), and document revision tracking.

### Layer Hierarchy
All layers derive from `cc::Layer`:
- `GroupLayer`: Container holding child layers in paint order (index 0 is bottom).
- `ImageLayer`: References an image asset from `AssetStore` with natural dimensions, optional non-destructive adjustments (`ImageAdjustments`), and raster modifications.
- `TextLayer`: Typographic text layer with font family, size, weight, tracking, alignment, wrap box, drop shadow, outline, gradient fills, and 3D tilt angles.
- `ShapeLayer`: Vector shapes (Rectangle, Rounded Rectangle, Ellipse, Line, Polygon) with fill and stroke.
- `BackgroundLayer`: Canvas background color or pattern.

### Layer Properties
- `name`: User-visible label in the layers panel.
- `visible`: Toggles layer rendering.
- `locked`: Prevents transform and selection on canvas.
- `opacity`: Normalized alpha `[0.0, 1.0]`.
- `blendMode`: 12 blend modes evaluated during scene compositing.
- `transform`: `AffineTransform` (translation, rotation, scale X/Y, shear X/Y).

## 5. Rendering pipeline & Paint Engine

`src/rendering/CanvasRenderer.cpp` handles compositing:
1. Walks the layer hierarchy from bottom to top.
2. Applies layer transforms (`AffineTransform`) to the `QPainter` transform stack.
3. Configures blend modes and opacity.
4. Renders layer geometry, raster images with adjustments applied, vector paths, or styled text.
5. In the interactive canvas (`CanvasView`), overlays are drawn on top:
   - Selected layer bounding box and transform handles (corners, edges, rotation knob).
   - Active smart guide alignment lines (cyan) when snapping is active.
   - Brush cursor preview when in painting mode.

### Paint Engine (`core/paint/`)
Provides real-time brush rendering directly onto image layers:
- **Brushes:** Round, Square, Marker (angled chisel), Airbrush (soft falloff), Eraser.
- **Parameters:** Size, opacity, flow, hardness/softness, spacing.
- **Stroke Smoothing:** Catmull-Rom spline interpolation between sampled pointer positions.

## 6. Canvas & interaction

`src/ui/canvasview.cpp` provides the viewport and interaction:
- **Pan & Zoom:** Smooth panning via Space+Drag or middle-click; zoom from 10% to 3200% with mouse wheel or hotkeys (`+`, `-`, `1`, `F` to fit).
- **Selection & Transform:** Direct manipulation of layer handles (scale, rotate, move).
- **Smart Guides & Snapping:** Calculates proximity thresholds against canvas edges, centerlines, and neighboring layer bounding boxes. Snaps the transform position and renders guide lines.
- **Inline Text Editing:** Double-click a text layer to activate in-place editing.
- **Drag and Drop:** External image files dropped onto the canvas are automatically imported and placed as new layers.

## 7. AI Background Removal

Located in `src/core/image/BackgroundRemover.{h,cpp}` and `src/ui/aibackgrounddialog.{h,cpp}`:
- **Engine:** Evaluated via ONNX Runtime C API.
- **Default Model:** `u2netp.onnx` (lightweight, high-speed salient object segmentation). Custom ONNX models can be selected in the dialog.
- **Pipeline:**
  1. Preprocessing: Resizes input to 320×320, normalizes RGB channels (ImageNet mean & std dev), packs NCHW tensor.
  2. Inference: Runs ONNX session asynchronously on a background worker thread.
  3. Postprocessing: Normalizes sigmoid output, resizes mask to original image dimensions, applies edge feathering.
  4. Compositing: Applies alpha channel to the original image pixels.
- **Cooperative Cancellation:** The `cancelFlag` parameter (`const std::atomic<bool>*`) is inspected before inference, between tensor operations, and during mask scaling. If the user cancels or closes the dialog, the worker halts immediately.
- **UI Safety:** Dialog controls (`m_modelCombo`, `m_browseModelBtn`, `m_runAiButton`) are locked during inference to prevent race conditions.

## 8. Internationalization

Located in `src/localization/i18nservice.{h,cpp}`:
- **Catalogs:** JSON catalogs categorized by namespace (`common`, `editor`, `settings`, `templates`) in `resources/locales/<lang>/`.
- **Supported Languages:** English (`en`) and Portuguese (`pt-BR`).
- **Dynamic Switching:** Language can be changed in Preferences at runtime without restarting the application; all UI views re-translate reactively.
- **Fallback Guarantee:** If a translation key is missing in the active language, it falls back to English, and finally to a clean humanized fallback.

## 9. Persistence — the `.creatorcanvas` format

A `.creatorcanvas` project file is a standard ZIP archive containing:
```
manifest.json        ← Complete document state, layers, metadata
thumbnail.png        ← 320x180 thumbnail preview for recent projects
media/<uuid>.png     ← Raw embedded image assets
```

### High-Performance Thumbnail Extraction
`cc::loadProjectThumbnail(const QString& filePath)` directly inspects the ZIP central directory using `miniz` and extracts only `thumbnail.png` into memory without decompressing the document manifest or media assets.

### Security & Zip Bomb Defenses
- Maximum entry count: 1,024 entries.
- Maximum single asset size: 100 MB.
- Maximum total uncompressed archive size: 500 MB.
- Path traversal protection: rejects entries with `..` or absolute paths.

## 10. Export

Located in `src/imageio/DocumentExporter.{h,cpp}`:
- **Formats:** PNG (lossless), JPEG (flattened onto background), WebP.
- **Options:** Custom output dimensions, scale multiplier (e.g. 0.5×, 1×, 2×, 4×), quality compression level (1–100).
- **Offscreen Rendering:** Export uses the exact same `CanvasRenderer` pipeline as the interactive viewport, ensuring complete visual fidelity while excluding editor UI overlays.

## 11. Commands and undo/redo

Located in `src/core/history/`:
- Every user action (layer add/remove/reorder, property edit, transform commit, brush stroke, adjustment change) is encapsulated as a `Command`.
- `CommandStack` maintains the undo/redo history.
- Actions can be merged if they represent continuous operations (e.g., continuous slider scrubbing).

## 12. Testing & Verification

17 unit test suites located in `tests/unit/`, run via `ctest`:
1. `test_settings`: Preferences persistence and schema validation.
2. `test_i18n`: Catalog completeness, key symmetry, fallback chains.
3. `test_logging`: Log formatting and file rotation.
4. `test_layers`: Layer hierarchy, z-ordering, cloning.
5. `test_document`: Document lifecycle, layer queries, bounds calculation.
6. `test_history`: Undo/redo stack operations.
7. `test_serialization`: Save/load roundtrips, corrupted file handling.
8. `test_renderer`: Offscreen rendering verification.
9. `test_newdocument`: Canvas preset generator.
10. `test_import`: Image format sniffing and decoding.
11. `test_transform`: Matrix math, rotation, scale, bounds mapping.
12. `test_textbox`: Text wrap calculations.
13. `test_effects`: Drop shadows and outline rasterization.
14. `test_export`: PNG, JPEG export accuracy.
15. `test_autosave`: Crash recovery file lifecycle.
16. `test_snap`: Smart guide alignment math.
17. `test_image_processing`: Color adjustments and image filter math.

### End-to-End Release Smoke Test
The binary includes `--smoke-test` for post-install validation:
```bash
QT_QPA_PLATFORM=offscreen CreatorCanvas --smoke-test
```
Verifies:
- Localization catalog loading (`en`, `pt-BR`).
- AI background remover inference (if model present).
- In-memory document generation with text and layers.
- Project serialization (`.creatorcanvas`) to disk.
- Instant `thumbnail.png` extraction and dimensions check.
- Deserialization and layer hierarchy verification.
- Document image export (PNG, JPEG, WebP).
- Headless `MainWindow` instantiation and project load.

## 13. Build & distribution

### CMake Presets (`CMakePresets.json`)
- `linux-debug`: Debug symbols, warnings enabled.
- `linux-release`: Release optimizations (`-O3`), stripped symbols.
- `linux-asan`: AddressSanitizer & UndefinedBehaviorSanitizer instrumentation.
- `windows-debug` / `windows-release`: MSVC configurations.

### Packaging
- **Linux DEB:** `packaging/linux/build_deb.sh` produces `creatorcanvas_0.2.0_amd64.deb`. Requires Qt >= 6.2 (compatible with Ubuntu 22.04 LTS, 24.04 LTS, Debian 12) and bundles `libonnxruntime.so` in `/usr/lib/creatorcanvas/`.
- **Windows Portable:** `packaging/windows/build_portable.ps1` produces `CreatorCanvas-portable.zip` with bundled Qt DLLs and ONNX Runtime.
- **Windows Installer:** `packaging/windows/installer.nsi` builds `CreatorCanvas-0.2.0-Setup.exe` with NSIS, creating desktop shortcuts, start menu entries, uninstaller, and `.creatorcanvas` file associations.
- **CI/CD:** `.github/workflows/release.yml` automatically compiles, tests, packages, and attaches both Linux and Windows release assets on tagged releases.

## 14. Roadmap

1. **GPU-Accelerated Viewport:** Optional Vulkan/Direct3D rendering backend for high-frequency canvas operations.
2. **Multi-layer Selection:** Simultaneous transform and group operations across multiple selected layers.
3. **macOS Support:** Native packaging (.dmg / notarization) using the existing platform-agnostic abstractions.

## 15. Conventions

- **Clean Commits:** Conventional Commits (`feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `chore:`).
- **Zero Hardcoded Strings:** All visible text must use `I18nService`.
- **Strict Headers:** Include what you use; full type definitions for types held in `std::unique_ptr` members.
- **Safe Resource Paths:** Cross-platform path handling via `QStandardPaths` and `QDir`.

## 16. Known limitations

- **Color Spaces:** Canvas operations currently execute in standard sRGB color space.
- **Animation:** Focus is strictly on static graphics and thumbnails (no multi-frame timeline or video export).
