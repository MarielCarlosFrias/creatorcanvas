#include "services/ImageResizer.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace cc {
namespace {

QSize scaledSize(const QSize& size, double scaleX, double scaleY)
{
    return QSize(qMax(1, static_cast<int>(std::lround(size.width() * scaleX))),
                 qMax(1, static_cast<int>(std::lround(size.height() * scaleY))));
}

double cubicWeight(double distance)
{
    // Catmull-Rom (a=-0.5): nítida sem o ringing excessivo de kernels maiores.
    constexpr double a = -0.5;
    distance = std::abs(distance);
    if (distance <= 1.0) {
        return (a + 2.0) * distance * distance * distance
            - (a + 3.0) * distance * distance + 1.0;
    }
    if (distance < 2.0) {
        return a * distance * distance * distance
            - 5.0 * a * distance * distance
            + 8.0 * a * distance - 4.0 * a;
    }
    return 0.0;
}

QImage bicubicColor(const QImage& input, const QSize& targetSize)
{
    const QImage::Format outputFormat = input.format();
    const QImage source = input.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage output(targetSize, QImage::Format_ARGB32_Premultiplied);

    const double scaleX = double(source.width()) / targetSize.width();
    const double scaleY = double(source.height()) / targetSize.height();
    for (int y = 0; y < targetSize.height(); ++y) {
        const double sourceY = (y + 0.5) * scaleY - 0.5;
        const int baseY = static_cast<int>(std::floor(sourceY));
        QRgb* destination = reinterpret_cast<QRgb*>(output.scanLine(y));
        for (int x = 0; x < targetSize.width(); ++x) {
            const double sourceX = (x + 0.5) * scaleX - 0.5;
            const int baseX = static_cast<int>(std::floor(sourceX));
            double red = 0.0;
            double green = 0.0;
            double blue = 0.0;
            double alpha = 0.0;

            for (int yy = -1; yy <= 2; ++yy) {
                const int sampleY = qBound(0, baseY + yy, source.height() - 1);
                const QRgb* line = reinterpret_cast<const QRgb*>(
                    source.constScanLine(sampleY));
                const double weightY = cubicWeight(sourceY - (baseY + yy));
                for (int xx = -1; xx <= 2; ++xx) {
                    const int sampleX = qBound(0, baseX + xx, source.width() - 1);
                    const double weight = weightY
                        * cubicWeight(sourceX - (baseX + xx));
                    const QRgb pixel = line[sampleX];
                    red += qRed(pixel) * weight;
                    green += qGreen(pixel) * weight;
                    blue += qBlue(pixel) * weight;
                    alpha += qAlpha(pixel) * weight;
                }
            }
            const auto channel = [](double value) {
                return qBound(0, static_cast<int>(std::lround(value)), 255);
            };
            destination[x] = qRgba(channel(red), channel(green), channel(blue),
                                   channel(alpha));
        }
    }
    return outputFormat == QImage::Format_Invalid
        ? output : output.convertToFormat(outputFormat);
}

QImage bicubicMask(const QImage& input, const QSize& targetSize)
{
    const QImage source = input.convertToFormat(QImage::Format_Alpha8);
    QImage output(targetSize, QImage::Format_Alpha8);
    const double scaleX = double(source.width()) / targetSize.width();
    const double scaleY = double(source.height()) / targetSize.height();

    for (int y = 0; y < targetSize.height(); ++y) {
        const double sourceY = (y + 0.5) * scaleY - 0.5;
        const int baseY = static_cast<int>(std::floor(sourceY));
        uchar* destination = output.scanLine(y);
        for (int x = 0; x < targetSize.width(); ++x) {
            const double sourceX = (x + 0.5) * scaleX - 0.5;
            const int baseX = static_cast<int>(std::floor(sourceX));
            double value = 0.0;
            for (int yy = -1; yy <= 2; ++yy) {
                const int sampleY = qBound(0, baseY + yy, source.height() - 1);
                const uchar* line = source.constScanLine(sampleY);
                const double weightY = cubicWeight(sourceY - (baseY + yy));
                for (int xx = -1; xx <= 2; ++xx) {
                    const int sampleX = qBound(0, baseX + xx, source.width() - 1);
                    value += line[sampleX] * weightY
                        * cubicWeight(sourceX - (baseX + xx));
                }
            }
            destination[x] = static_cast<uchar>(
                qBound(0, static_cast<int>(std::lround(value)), 255));
        }
    }
    return output;
}

void captureLayers(const GroupLayer* group, std::vector<ImageResizer::LayerState>& out)
{
    if (!group)
        return;
    for (const auto& child : group->children) {
        ImageResizer::LayerState state;
        state.id = child->id();
        state.type = child->type();
        state.transform = child->transform;
        state.vectorMask = child->vectorMask;
        if (child->type() == LayerType::Image) {
            const auto* image = static_cast<const ImageLayer*>(child.get());
            state.assetId = image->assetId;
            state.naturalWidth = image->naturalWidth;
            state.naturalHeight = image->naturalHeight;
            state.alphaMask = image->alphaMask;
        }
        out.push_back(std::move(state));
        if (child->type() == LayerType::Group)
            captureLayers(static_cast<const GroupLayer*>(child.get()), out);
    }
}

void restoreLayers(Document& document,
                   const std::vector<ImageResizer::LayerState>& states)
{
    for (const auto& state : states) {
        Layer* layer = document.findLayer(state.id);
        if (!layer || layer->type() != state.type)
            continue;
        layer->transform = state.transform;
        layer->vectorMask = state.vectorMask;
        if (layer->type() == LayerType::Image) {
            auto* image = static_cast<ImageLayer*>(layer);
            image->assetId = state.assetId;
            image->naturalWidth = state.naturalWidth;
            image->naturalHeight = state.naturalHeight;
            image->alphaMask = state.alphaMask;
            image->maskedRenderCache = QImage();
            image->maskedPreviewCache = QImage();
            image->adjustedRenderCache = QImage();
            image->maskedRenderSourceKey = 0;
            image->maskedRenderMaskKey = 0;
            image->maskedPreviewSourceKey = 0;
            image->maskedPreviewMaskKey = 0;
            image->adjustedRenderSourceKey = 0;
            image->adjustedRenderAdjKey = 0;
        }
    }
    // Notifica depois de restaurar toda a árvore: grupos e filhos ficam
    // visualmente coerentes até no primeiro repaint.
    for (const auto& state : states)
        document.touchLayer(state.id);
}

QImage scaledSelectionMask(const QImage& mask, SelectionSpace space,
                           const QSize& documentSize,
                           double scaleX, double scaleY,
                           ResampleMethod method)
{
    if (mask.isNull())
        return {};
    const QSize target = space == SelectionSpace::Document
        ? documentSize : scaledSize(mask.size(), scaleX, scaleY);
    return ImageResizer::resampleMask(mask, target, method);
}

SelectionState scaledSelection(const SelectionState& source,
                               const QSize& documentSize,
                               double scaleX, double scaleY,
                               ResampleMethod method)
{
    SelectionState result;
    if (!source.previousImage().isNull()) {
        result.setImage(scaledSelectionMask(source.previousImage(),
                                            source.previousImageSpace(),
                                            documentSize, scaleX, scaleY, method),
                        source.previousImageSpace());
    }
    if (!source.image().isNull()) {
        result.setImage(scaledSelectionMask(source.image(), source.imageSpace(),
                                            documentSize, scaleX, scaleY, method),
                        source.imageSpace());
    } else if (!source.previousImage().isNull()) {
        result.clear(); // mantém previous sem o tornar ativo
    }

    for (const SavedSelection& saved : source.savedSelections()) {
        result.addSavedSelection(saved.id, saved.name,
                                 scaledSelectionMask(saved.mask, saved.space,
                                                     documentSize, scaleX, scaleY,
                                                     method),
                                 saved.space);
    }
    return result;
}

} // namespace

QImage ImageResizer::resample(const QImage& source, const QSize& targetSize,
                              ResampleMethod method)
{
    if (source.isNull() || targetSize.width() <= 0 || targetSize.height() <= 0)
        return {};
    if (source.size() == targetSize)
        return source.copy();

    switch (method) {
    case ResampleMethod::NearestNeighbor:
        return source.scaled(targetSize, Qt::IgnoreAspectRatio,
                             Qt::FastTransformation);
    case ResampleMethod::Bilinear:
        return source.scaled(targetSize, Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation);
    case ResampleMethod::Bicubic:
        return bicubicColor(source, targetSize);
    }
    return {};
}

QImage ImageResizer::resampleMask(const QImage& source, const QSize& targetSize,
                                  ResampleMethod method)
{
    if (source.isNull() || targetSize.width() <= 0 || targetSize.height() <= 0)
        return {};
    const QImage mask = source.convertToFormat(QImage::Format_Alpha8);
    if (mask.size() == targetSize)
        return mask.copy();
    if (method == ResampleMethod::Bicubic)
        return bicubicMask(mask, targetSize);

    const Qt::TransformationMode quality = method == ResampleMethod::NearestNeighbor
        ? Qt::FastTransformation : Qt::SmoothTransformation;
    return mask.scaled(targetSize, Qt::IgnoreAspectRatio, quality)
        .convertToFormat(QImage::Format_Alpha8);
}

ImageResizer::Snapshot ImageResizer::capture(const Document& document)
{
    Snapshot snapshot;
    snapshot.width = document.width();
    snapshot.height = document.height();
    captureLayers(document.rootGroup(), snapshot.layers);
    snapshot.selection = document.selectionSnapshot();
    snapshot.guides = document.guides();
    snapshot.webSlices = document.webSlices();
    return snapshot;
}

void ImageResizer::restore(Document& document, const Snapshot& snapshot)
{
    if (snapshot.width <= 0 || snapshot.height <= 0)
        return;
    document.setCanvasSize(snapshot.width, snapshot.height);
    restoreLayers(document, snapshot.layers);
    document.restoreSelectionSnapshot(snapshot.selection);
    document.setGuides(snapshot.guides);
    document.setWebSlices(snapshot.webSlices);
}

bool ImageResizer::resize(Document& document, const ImageResizeOptions& options,
                          QString* error)
{
    if (error)
        error->clear();
    const int newWidth = options.size.width();
    const int newHeight = options.size.height();
    if (newWidth <= 0 || newHeight <= 0) {
        if (error)
            *error = QStringLiteral("Image dimensions must be positive.");
        return false;
    }
    if (newWidth == document.width() && newHeight == document.height())
        return false;

    const Snapshot before = capture(document);
    const double scaleX = double(newWidth) / before.width;
    const double scaleY = double(newHeight) / before.height;

    // Valida todos os inputs antes de adicionar assets ou tocar no documento.
    for (const LayerState& state : before.layers) {
        if (state.type == LayerType::Image
            && document.assets().decodedImage(state.assetId).isNull()) {
            if (error) {
                *error = QStringLiteral("Image layer '%1' has no readable asset.")
                    .arg(state.id.toString(QUuid::WithoutBraces));
            }
            return false;
        }
    }

    std::vector<LayerState> after = before.layers;
    struct CachedAsset {
        LayerId sourceId;
        QSize targetSize;
        LayerId resizedId;
    };
    std::vector<CachedAsset> cache;
    std::vector<LayerId> createdAssets;
    const auto rollbackAssets = [&document](const std::vector<LayerId>& ids) {
        for (const LayerId& id : ids)
            document.assets().remove(id);
    };

    for (LayerState& state : after) {
        state.transform.position = QPointF(state.transform.position.x() * scaleX,
                                           state.transform.position.y() * scaleY);
        // Imagens já crescem porque os seus pixels naturais são reamostrados;
        // grupos propagam escala aos filhos. Aplicar scale nelas duplicaria o
        // fator. Texto/formas/vetores continuam procedurais e usam o transform.
        if (state.type != LayerType::Image && state.type != LayerType::Group) {
            state.transform.scaleX *= scaleX;
            state.transform.scaleY *= scaleY;
        }
        if (!state.vectorMask.isNull()) {
            state.vectorMask = resampleMask(
                state.vectorMask, scaledSize(state.vectorMask.size(), scaleX, scaleY),
                options.method);
        }

        if (state.type != LayerType::Image)
            continue;
        const QImage source = document.assets().decodedImage(state.assetId);
        const QSize oldNatural(
            state.naturalWidth > 0 ? state.naturalWidth : source.width(),
            state.naturalHeight > 0 ? state.naturalHeight : source.height());
        const QSize newNatural = scaledSize(oldNatural, scaleX, scaleY);
        const QImage resized = resample(source, newNatural, options.method);
        if (resized.isNull()) {
            rollbackAssets(createdAssets);
            if (error)
                *error = QStringLiteral("Could not resample an image asset.");
            return false;
        }

        LayerId resizedId;
        const auto cached = std::find_if(
            cache.begin(), cache.end(), [&](const CachedAsset& entry) {
                return entry.sourceId == state.assetId
                    && entry.targetSize == newNatural;
            });
        if (cached != cache.end()) {
            resizedId = cached->resizedId;
        } else {
            const Asset* oldAsset = document.assets().find(state.assetId);
            const QString format = oldAsset && !oldAsset->format.isEmpty()
                ? oldAsset->format : QStringLiteral("png");
            resizedId = document.assets().addImage(resized, format);
            if (resizedId.isNull()) {
                rollbackAssets(createdAssets);
                if (error)
                    *error = QStringLiteral("Could not store a resampled image asset.");
                return false;
            }
            createdAssets.push_back(resizedId);
            cache.push_back(CachedAsset{state.assetId, newNatural, resizedId});
        }
        state.assetId = resizedId;
        state.naturalWidth = newNatural.width();
        state.naturalHeight = newNatural.height();
        if (!state.alphaMask.isNull()) {
            state.alphaMask = resampleMask(
                state.alphaMask, scaledSize(state.alphaMask.size(), scaleX, scaleY),
                options.method);
        }
    }

    SelectionState selection = scaledSelection(before.selection, options.size,
                                               scaleX, scaleY, options.method);
    std::vector<Guide> guides = before.guides;
    for (Guide& guide : guides)
        guide.position *= guide.horizontal ? scaleY : scaleX;

    std::vector<WebSlice> slices = before.webSlices;
    for (WebSlice& slice : slices) {
        const QRectF old = slice.rect;
        slice.rect = QRectF(old.x() * scaleX, old.y() * scaleY,
                            old.width() * scaleX, old.height() * scaleY);
    }

    // setCanvasSize trata apenas da dimensão/sinal. O seu recorte do estado
    // auxiliar é imediatamente substituído pela versão reamostrada capturada
    // acima; Image Size nunca corta guias, seleções ou slices.
    document.setCanvasSize(newWidth, newHeight);
    restoreLayers(document, after);
    document.restoreSelectionSnapshot(selection);
    document.setGuides(std::move(guides));
    document.setWebSlices(std::move(slices));
    return true;
}

} // namespace cc
