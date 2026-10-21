#include "SelectionOperations.h"

#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "core/layers/Layer.h"

namespace cc {

bool SelectionOperations::canEditPixels(const Document& doc,
                                         const LayerId& id,
                                         QString* whyNot)
{
    const Layer* layer = doc.findLayer(id);
    if (!layer) {
        if (whyNot)
            *whyNot = QStringLiteral("Layer not found.");
        return false;
    }
    const LayerLocks locks = doc.effectiveLocks(id);
    if (locks.pixels || locks.all) {
        if (whyNot)
            *whyNot = QStringLiteral("The layer is locked (image pixels).");
        return false;
    }
    return true;
}

QImage SelectionOperations::applySelectionMask(const QImage& original,
                                               const QImage& modified,
                                               const QImage& selection)
{
    if (selection.isNull() || original.isNull() || modified.isNull())
        return modified;
    if (selection.size() != original.size() || selection.size() != modified.size())
        return modified; // selection in a different space: don't guess
    if (original.size() != modified.size())
        return modified; // operation resized (crop etc.): can't mask

    const QImage sel = selection.convertToFormat(QImage::Format_Alpha8);
    // Fast path: fully selected (all 255) = just return modified.
    bool allSelected = true;
    for (int y = 0; y < sel.height() && allSelected; ++y) {
        const uchar* row = sel.constScanLine(y);
        for (int x = 0; x < sel.width(); ++x) {
            if (row[x] != 255) { allSelected = false; break; }
        }
    }
    if (allSelected)
        return modified;

    QImage out = original.convertToFormat(QImage::Format_ARGB32);
    const QImage mod = modified.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < out.height(); ++y) {
        QRgb* dst = reinterpret_cast<QRgb*>(out.scanLine(y));
        const QRgb* src = reinterpret_cast<const QRgb*>(mod.constScanLine(y));
        const uchar* mask = sel.constScanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            if (mask[x] == 255)
                dst[x] = src[x];              // fully selected: modified
            else if (mask[x] == 0)
                continue;                      // not selected: keep original
            else {
                // Partial: lerp per channel (including alpha).
                const int m = mask[x];
                dst[x] = qRgba(
                    qRed(dst[x]) +   (qRed(src[x])   - qRed(dst[x]))   * m / 255,
                    qGreen(dst[x]) + (qGreen(src[x]) - qGreen(dst[x])) * m / 255,
                    qBlue(dst[x]) +  (qBlue(src[x])  - qBlue(dst[x]))  * m / 255,
                    qAlpha(dst[x]) + (qAlpha(src[x]) - qAlpha(dst[x])) * m / 255);
            }
        }
    }
    return out;
}

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
