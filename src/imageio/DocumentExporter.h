#pragma once

#include <QRectF>
#include <QString>
#include <QUuid>

namespace cc {

class Document;

/// Renders the document offscreen (no checkerboard, no overlays, no border)
/// and saves it as an image file. |format| is "png", "jpeg" or "webp";
/// |quality| applies to jpeg/webp; |scale| multiplies the canvas size;
/// |transparentBackground| keeps the alpha channel when the format allows it
/// (JPEG is always opaque). Returns false with a human-readable
/// |errorMessage| on failure.
bool exportDocumentToImage(const Document& doc, const QString& filePath,
                           const QString& format, int quality, double scale,
                           QString* errorMessage = nullptr,
                           bool transparentBackground = true);

/// Renders a single layer to an image file with transparency.
bool exportLayerToImage(const Document& doc, const QUuid& layerId,
                        const QString& filePath, const QString& format,
                        int quality, double scale,
                        QString* errorMessage = nullptr,
                        bool transparentBackground = true);

/// Renders a rectangular region/slice of the document (Web Slice export)
bool exportSliceToImage(const Document& doc, const QRectF& sliceRect,
                        const QString& filePath, const QString& format,
                        int quality, double scale,
                        QString* errorMessage = nullptr);

} // namespace cc
