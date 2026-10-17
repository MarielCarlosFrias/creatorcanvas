#include "SelectionOperations.h"

#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "core/layers/Layer.h"

namespace cc {

QImage SelectionOperations::inverted(const QImage& selection)
{
    QImage result = selection.convertToFormat(QImage::Format_Alpha8);
    for (int y = 0; y < result.height(); ++y) {
        uchar* row = result.scanLine(y);
        for (int x = 0; x < result.width(); ++x)
            row[x] = 255 - row[x];
    }
    return result;
}

bool SelectionOperations::extract(const QImage& source, const QImage& selection,
                                  QImage* image, QRect* bounds)
{
    if (!image || !bounds || source.isNull() || selection.isNull()
        || source.size() != selection.size())
        return false;
    const QImage alpha = selection.convertToFormat(QImage::Format_Alpha8);
    QRect area;
    for (int y = 0; y < alpha.height(); ++y) {
        const uchar* row = alpha.constScanLine(y);
        for (int x = 0; x < alpha.width(); ++x)
            if (row[x] > 8)
                area |= QRect(x, y, 1, 1);
    }
    if (!area.isValid())
        return false;
    QImage result = source.copy(area).convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < result.height(); ++y) {
        QRgb* dst = reinterpret_cast<QRgb*>(result.scanLine(y));
        const uchar* src = alpha.constScanLine(area.y() + y) + area.x();
        for (int x = 0; x < result.width(); ++x)
            dst[x] = qRgba(qRed(dst[x]), qGreen(dst[x]), qBlue(dst[x]),
                          (qAlpha(dst[x]) * src[x]) / 255);
    }
    *image = std::move(result);
    *bounds = area;
    return true;
}

bool SelectionOperations::extractToLayer(Document& document, CommandStack* history,
                                          const LayerId& sourceId, bool cut,
                                          LayerId* createdId)
{
    if (!history)
        return false;
    Layer* sourceLayer = document.findLayer(sourceId);
    if (!sourceLayer || sourceLayer->type() != LayerType::Image
        || document.pixelSelection().isNull())
        return false;
    auto* imageLayer = static_cast<ImageLayer*>(sourceLayer);
    QImage copy;
    QRect bounds;
    if (!extract(document.assets().decodedImage(imageLayer->assetId),
                 document.pixelSelection(), &copy, &bounds))
        return false;
    auto newLayer = std::make_unique<ImageLayer>();
    newLayer->name = QStringLiteral("Selection copy");
    newLayer->assetId = document.assets().addImage(copy);
    newLayer->naturalWidth = copy.width();
    newLayer->naturalHeight = copy.height();
    newLayer->transform = imageLayer->transform;
    const LayerId newId = newLayer->id();
    // M3: um único passo de undo — antes eram AddLayerCommand +
    // visible=false separados; o Ctrl+Z parava num estado intermediário
    // (cópia presente E fonte oculta) que nunca existiu de verdade.
    auto compound = std::make_unique<CompoundCommand>(
        QStringLiteral("selection.cutToLayer"));
    compound->addCommand(std::make_unique<AddLayerCommand>(
        document, std::move(newLayer), document.rootGroup(), -1));
    if (cut) {
        compound->addCommand(std::make_unique<LayerPropertyCommand<bool>>(
            document, sourceId, QStringLiteral("layer.visible"),
            &Document::setLayerVisible, imageLayer->visible, false));
    }
    history->execute(std::move(compound));
    document.clearPixelSelection();
    if (createdId)
        *createdId = newId;
    return true;
}

bool SelectionOperations::applyMask(Document& document, CommandStack* history,
                                    const LayerId& layerId, bool keepInside)
{
    Layer* layer = document.findLayer(layerId);
    if (!layer || layer->type() != LayerType::Image || document.pixelSelection().isNull())
        return false;
    auto* image = static_cast<ImageLayer*>(layer);
    const QImage source = document.assets().decodedImage(image->assetId);
    if (source.isNull() || source.size() != document.pixelSelection().size())
        return false;

    QImage operation = keepInside ? document.pixelSelection().convertToFormat(QImage::Format_Alpha8)
                                  : inverted(document.pixelSelection());
    QImage mask = image->alphaMask.isNull() || image->alphaMask.size() != operation.size()
        ? operation : image->alphaMask.convertToFormat(QImage::Format_Alpha8);
    if (!image->alphaMask.isNull() && image->alphaMask.size() == operation.size()) {
        for (int y = 0; y < mask.height(); ++y) {
            uchar* dst = mask.scanLine(y);
            const uchar* src = operation.constScanLine(y);
            for (int x = 0; x < mask.width(); ++x)
                dst[x] = static_cast<uchar>((int(dst[x]) * int(src[x])) / 255);
        }
    }
    const QImage oldMask = image->alphaMask;
    if (history) {
        history->execute(std::make_unique<LayerPropertyCommand<QImage>>(
            document, layerId, QStringLiteral("layer.mask"),
            &Document::setLayerAlphaMask, oldMask, mask));
    } else {
        document.setLayerAlphaMask(layerId, mask);
    }
    document.clearPixelSelection();
    return true;
}

} // namespace cc
