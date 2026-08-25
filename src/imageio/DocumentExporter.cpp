#include "DocumentExporter.h"

#include "core/Document.h"
#include "rendering/CanvasRenderer.h"

#include <QImage>
#include <QPainter>

namespace cc {

bool exportDocumentToImage(const Document& doc, const QString& filePath,
                           const QString& format, int quality, double scale,
                           QString* errorMessage)
{
    const int width = qMax(1, static_cast<int>(doc.width() * scale));
    const int height = qMax(1, static_cast<int>(doc.height() * scale));

    QImage image(width, height, QImage::Format_ARGB32);
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
    if (format == QLatin1String("jpeg")) {
        // JPEG has no alpha: flatten onto white.
        QImage flat(toSave.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        QPainter p(&flat);
        p.drawImage(0, 0, toSave);
        p.end();
        toSave = flat;
    }

    if (!toSave.save(filePath, format.toLatin1().constData(), quality)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not write the image file.");
        return false;
    }
    return true;
}

} // namespace cc
