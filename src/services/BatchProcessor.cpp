#include "BatchProcessor.h"

#include <QFileInfo>

#include "core/Document.h"
#include "core/layers/Layer.h"
#include "imageio/DocumentExporter.h"
#include "imageio/ImageImporter.h"
#include "imageio/SvgExporter.h"

namespace cc {

bool processBatchImageFile(const QString& inputPath, const QString& outputPath,
                           const BatchImageOptions& options,
                           QString* errorMessage)
{
    auto fail = [&](const QString& message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };

    ImportResult imported = importImageFromFile(inputPath);
    if (imported.status != ImportStatus::Ok)
        return fail(imported.errorMessage.isEmpty()
                        ? QStringLiteral("Could not import the input image.")
                        : imported.errorMessage);
    QImage result = imported.image;
    if (result.isNull())
        return fail(QStringLiteral("The input image is empty."));

    for (const QString& op : options.operations) {
        if (op == QLatin1String("resize")) {
            if (!options.resizeSize.isValid())
                return fail(QStringLiteral("Invalid resize dimensions."));
            result = result.scaled(options.resizeSize, Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
        }
        // convert e quality são aplicados na gravação/exportação.
    }

    const QByteArray format = options.outputFormat.isEmpty()
                                  ? QByteArray("png")
                                  : options.outputFormat.toLower();
    QString finalPath = outputPath;
    if (QFileInfo(finalPath).suffix().isEmpty())
        finalPath += QStringLiteral(".") + QString::fromLatin1(format);
    if (QFileInfo::exists(finalPath) && !options.overwrite)
        return fail(QStringLiteral("The output file already exists."));

    if (format == QByteArray("pdf") || format == QByteArray("svg")) {
        Document document(result.width(), result.height());
        auto layer = std::make_unique<ImageLayer>();
        layer->name = QFileInfo(inputPath).baseName();
        layer->assetId = document.assets().addImage(result);
        if (layer->assetId.isNull())
            return fail(QStringLiteral("Could not create the output image asset."));
        layer->naturalWidth = result.width();
        layer->naturalHeight = result.height();
        layer->transform.position = QPointF(result.width() / 2.0,
                                            result.height() / 2.0);
        if (!document.addLayer(std::move(layer)))
            return fail(QStringLiteral("Could not create the output document."));

        QString exportError;
        const bool ok = format == QByteArray("pdf")
            ? exportDocumentToPdf(document, finalPath, 1.0, &exportError)
            : exportDocumentToSvg(document, finalPath, 1.0, &exportError);
        return ok ? true : fail(exportError.isEmpty()
                                    ? QStringLiteral("Could not export the document.")
                                    : exportError);
    }

    if (!result.save(finalPath, format.constData(), options.outputQuality))
        return fail(QStringLiteral("Could not write the output image."));
    return true;
}

} // namespace cc
