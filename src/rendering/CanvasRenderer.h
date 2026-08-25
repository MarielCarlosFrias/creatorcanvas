#pragma once

#include <QColor>
#include <QTransform>

class QPainter;

namespace cc {

class Document;

struct RenderOptions
{
    /// Checkerboard beneath the layers (visible where transparent).
    bool drawCheckerboard = true;
    int checkerSize = 8;                              // screen px per square
    QColor checkerLight{0xcc, 0xcc, 0xcc};
    QColor checkerDark{0x99, 0x99, 0x99};
    QColor canvasBorder{0x50, 0x51, 0x57};
};

/// Renders the document through |painter| (already active on the target
/// device). |docToDevice| maps document pixel coordinates to device
/// coordinates.
///
/// Phase-1 software rendering contract:
///   - bottom-up paint order, per-layer opacity (group opacity composes
///     multiplicatively down the tree), blend via composition mode
///   - ImageLayers without pixel data render as a neutral placeholder
///   - no caching yet: full recomposition per call (M6 adds caching)
void renderDocument(const Document& doc,
                    QPainter* painter,
                    const QTransform& docToDevice,
                    const RenderOptions& options = {});

} // namespace cc
