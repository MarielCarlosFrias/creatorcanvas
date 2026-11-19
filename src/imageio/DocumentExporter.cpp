#include "DocumentExporter.h"

#include "core/Document.h"
#include "core/color/IccProfiles.h"
#include "rendering/CanvasRenderer.h"

#include <QImage>
#include <QPainter>

namespace cc {

namespace {

/// Composites |image| onto an opaque white background.
bool flattenOntoWhite(const QImage& image, QImage& out, QString* errorMessage)
{
    out = QImage(image.size(), QImage::Format_RGB32);
    if (out.isNull()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to allocate memory for flattening.");
        return false;
    }
    out.fill(Qt::white);
    QPainter painter(&out);
    painter.drawImage(0, 0, image);
    painter.end();
    return true;
}

} // namespace

bool exportDocumentToImage(const Document& doc, const QString& filePath,
                           const QString& format, int quality, double scale,
                           QString* errorMessage, bool transparentBackground)
{
    const int width = qMax(1, static_cast<int>(doc.width() * scale));
    const int height = qMax(1, static_cast<int>(doc.height() * scale));

    QImage image(width, height, QImage::Format_ARGB32);
    if (image.isNull()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to allocate memory for export (image dimensions too large).");
        return false;
    }
    image.fill(Qt::transparent);

    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        QTransform transform;
        transform.scale(scale, scale);
        RenderOptions options;
        options.drawCheckerboard = false;
        options.canvasBorder = Qt::transparent; // export: no editor border
        renderDocument(doc, &painter, transform, options);
    }

    QImage toSave = image;
    if (format == QLatin1String("jpeg") || !transparentBackground) {
        // JPEG has no alpha: flatten onto white. A disabled transparency
        // option flattens the same way so the file matches the preview.
        QImage flat;
        if (!flattenOntoWhite(toSave, flat, errorMessage))
            return false;
        toSave = flat;
    }

    if (!toSave.save(filePath, format.toLatin1().constData(), quality)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not write the image file.");
        return false;
    }

    // Colour management: embed the ICC profile of the document's colour space
    // (PNG iCCP / JPEG ICC_PROFILE) when the platform provides one. A missing
    // profile is a no-op, never a failure.
    (void)tagImageFileWithProfile(filePath, format, doc.iccProfile(), doc.colorSpace());
    return true;
}

bool exportLayerToImage(const Document& doc, const QUuid& layerId,
                        const QString& filePath, const QString& format,
                        int quality, double scale,
                        QString* errorMessage, bool transparentBackground)
{
    const Layer* layer = doc.findLayer(layerId);
    if (!layer) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Layer not found.");
        return false;
    }

    const int width = qMax(1, static_cast<int>(doc.width() * scale));
    const int height = qMax(1, static_cast<int>(doc.height() * scale));

    QImage image(width, height, QImage::Format_ARGB32);
    if (image.isNull()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to allocate memory for layer export.");
        return false;
    }
    image.fill(Qt::transparent);

    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        QTransform transform;
        transform.scale(scale, scale);
        RenderOptions options;
        options.drawCheckerboard = false;
        options.canvasBorder = Qt::transparent;
        renderSiblingLayers(doc, {layerId}, &painter, options);
    }

    QImage toSave = image;
    if (format == QLatin1String("jpeg") || !transparentBackground) {
        QImage flat;
        if (!flattenOntoWhite(toSave, flat, errorMessage))
            return false;
        toSave = flat;
    }

    if (!toSave.save(filePath, format.toLatin1().constData(), quality)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not write the image file.");
        return false;
    }

    // Colour management: embed the ICC profile of the document's colour space
    // (PNG iCCP / JPEG ICC_PROFILE) when the platform provides one. A missing
    // profile is a no-op, never a failure.
    (void)tagImageFileWithProfile(filePath, format, doc.iccProfile(), doc.colorSpace());
    return true;
}

bool exportSliceToImage(const Document& doc, const QRectF& sliceRect,
                        const QString& filePath, const QString& format,
                        int quality, double scale,
                        QString* errorMessage)
{
    const int w = qMax(1, static_cast<int>(sliceRect.width() * scale));
    const int h = qMax(1, static_cast<int>(sliceRect.height() * scale));

    QImage image(w, h, QImage::Format_ARGB32);
    if (image.isNull()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to allocate memory for slice export.");
        return false;
    }
    image.fill(Qt::transparent);

    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        QTransform transform;
        transform.scale(scale, scale);
        transform.translate(-sliceRect.x(), -sliceRect.y());
        RenderOptions options;
        options.drawCheckerboard = false;
        options.canvasBorder = Qt::transparent;
        renderDocument(doc, &painter, transform, options);
    }

    QImage toSave = image;
    if (format == QLatin1String("jpeg")) {
        QImage flat(toSave.size(), QImage::Format_RGB32);
        if (flat.isNull()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Failed to allocate memory for JPEG flattening.");
            return false;
        }
        flat.fill(Qt::white);
        QPainter p(&flat);
        p.drawImage(0, 0, toSave);
        p.end();
        toSave = flat;
    }

    if (!toSave.save(filePath, format.toLatin1().constData(), quality)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not write the slice image file.");
        return false;
    }

    (void)tagImageFileWithProfile(filePath, format, doc.iccProfile(), doc.colorSpace());
    return true;
}

} // namespace cc
