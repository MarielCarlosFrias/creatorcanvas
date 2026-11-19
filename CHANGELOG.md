# Changelog

All notable changes to CreatorCanvas are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

Everything accumulated on `main` since the `v0.2.1` tag (2026-11-04).

### Added

- **MiniPaint integration (`Image ▸ Edit in MiniPaint`)** — the Paint-like
  editor module is vendored under `3rdparty/minipaint`, built in-tree and
  shipped beside `CreatorCanvas` (DEB, AppImage and the Windows portable ZIP
  now all include it, with its SFML 3 runtime). The bridge is non-blocking
  (the interface stays responsive while the editor is open) and logs the
  phase timings.
- **Export dialog** rework — one row of platform preset chips replaces the
  duplicated dropdown, transparency toggle preserved across format changes,
  live checkerboard preview, translated dimension/size labels.
- **Export transparency** (`transparentBackground`) honoured by
  `exportDocumentToImage()` / `exportLayerToImage()`; JPEG stays opaque.
- **ICC export tagging** — exported PNG/JPEG files and web slices embed the
  document's matching installed ICC profile (PNG `iCCP` before IDAT, JPEG
  APP2 sequence), atomically and with graceful fallback when unavailable.
- **Tests**: MiniPaint launch contract (`test_minipaint_bridge`), export
  transparency behaviour (`test_export_dialog`), the MiniPaint module's
  own smoke/regression suite (`minipaint_smoke`, skips without a display),
  the widget-free viewport (`test_canvas_transform`) and ICC round trips
  (`test_icc`).
- **Photoshop-style flyout tool slots** — 9 consolidated slots in the toolbar.
- Pixel grid at ultra-high zoom; auto-save deferred while the mouse is busy.

### Changed

- **MiniPaint drawing performance** — incremental compositing during strokes
  (~210× cheaper per pointer event at 1920×1080, two layers) and redraw only
  when something changes instead of a constant 60 fps UI loop.
- AppStream metadata validates (`<p>`-wrapped release descriptions) and is
  installed under its component ID; the AppImage version is read from
  `project()` instead of `cmake_minimum_required`.
- `LayerOperations` moved from `core/operations` to `services`.
- `CanvasView`'s zoom/pan/fit block extracted into the widget-free
  `CanvasTransform` unit (`src/ui/canvastransform.{h,cpp}`), covered by
  `test_canvas_transform`.

### Fixed

- Export estimated size and dimension labels were hardcoded English and now
  use the translation catalogs.
- `MiniPaintBridge` no longer freezes the application or spins a wait cursor
  while the external editor is open, and refuses to start a second session.
- Duplicated scale entries accumulating in the export dialog's scale combo.
- MacOS-safe handling of a disabled transparency toggle (state preserved
  across JPEG round-trips).

## [0.2.1] - 2026-11-04

M21–M23 feature work and hardening: clipping masks (also inside groups),
perspective mockups with a free-form mode, pen (Bézier) tool,
marquee/gradient/retouch/healing tools, batch processing, workspace layouts,
customizable shortcuts, scanline flood fill and magic wand, and DEB/NSIS
packaging fixes.

## [0.2.0] - 2026-09-08

Phase 2 release: AI background removal, raster tools (Crop, Scissors, Wand,
Clone Stamp), Paint & Flood Fill brushes, text gradients, 3D tilt
perspective, and color adjustments.

## [0.1.0] - 2026-08-26

Initial release: layers, image import, text, shapes, presets, undo/redo,
export.

[Unreleased]: https://github.com/MarielCarlosFrias/creatorcanvas/compare/v0.2.1...HEAD
[0.2.1]: https://github.com/MarielCarlosFrias/creatorcanvas/releases/tag/v0.2.1
[0.2.0]: https://github.com/MarielCarlosFrias/creatorcanvas/releases/tag/v0.2.0
[0.1.0]: https://github.com/MarielCarlosFrias/creatorcanvas/releases/tag/v0.1.0