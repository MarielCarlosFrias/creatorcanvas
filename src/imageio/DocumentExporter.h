#pragma once

#include <QString>
#include <QUuid>

namespace cc {

class Document;

/// Renders the document offscreen (no checkerboard, no overlays, no border)
/// and saves it as an image file. |format| is "png", "jpeg" or "webp";
/// |quality| applies to jpeg/webp; |scale| multiplies the canvas size.
/// Returns false with a human-readable |errorMessage| on failure.
bool exportDocumentToImage(const Document& doc, const QString& filePath,
                           const QString& format, int quality, double scale,
                           QString* errorMessage = nullptr);

/// Renders a single layer to an image file with transparency.
bool exportLayerToImage(const Document& doc, const QUuid& layerId,
                        const QString& filePath, const QString& format,
                        int quality, double scale,
                        QString* errorMessage = nullptr);

} // namespace cc
