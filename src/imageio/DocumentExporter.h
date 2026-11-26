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

/// Renders the document into a single-page PDF without the interactive
/// CachingRenderer, so shapes, paths and simple text are painted directly as
/// PDF vector operators whenever the renderer supports it. Bitmap layers and
/// features requiring raster compositing (for example clipping/adjustment
/// stacks or raster effects) are necessarily embedded as images. |scale|
/// multiplies the canvas size.
bool exportDocumentToPdf(const Document& doc, const QString& filePath,
                         double scale, QString* errorMessage = nullptr);

} // namespace cc
