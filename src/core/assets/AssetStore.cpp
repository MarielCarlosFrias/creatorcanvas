#include "AssetStore.h"

#include <QBuffer>
#include <QCryptographicHash>

namespace cc {

LayerId AssetStore::addImage(const QImage& image, const QString& format)
{
    if (image.isNull())
        return {};

    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, format.toLatin1().constData());

    Asset asset;
    asset.id = newLayerId();
    asset.format = format;
    asset.width = image.width();
    asset.height = image.height();
    asset.encoded = encoded;
    asset.sha256 = QString::fromLatin1(
        QCryptographicHash::hash(encoded, QCryptographicHash::Sha256).toHex());
    asset.decoded = image;

    m_assets.append(asset);
    return asset.id;
}

LayerId AssetStore::add(const QByteArray& encoded, const QString& format)
{
    QImage decoded;
    decoded.loadFromData(encoded, format.toLatin1().constData());
    if (decoded.isNull())
        return {};

    Asset asset;
    asset.id = newLayerId();
    asset.format = format;
    asset.width = decoded.width();
    asset.height = decoded.height();
    asset.encoded = encoded;
    asset.sha256 = QString::fromLatin1(
        QCryptographicHash::hash(encoded, QCryptographicHash::Sha256).toHex());
    asset.decoded = decoded;

    m_assets.append(asset);
    return asset.id;
}

void AssetStore::restore(const Asset& asset)
{
    m_assets.append(asset);
}

const Asset* AssetStore::find(const LayerId& id) const
{
    for (const Asset& asset : m_assets)
        if (asset.id == id)
            return &asset;
    return nullptr;
}

QImage AssetStore::decodedImage(const LayerId& id) const
{
    const Asset* asset = find(id);
    if (!asset)
        return {};
    if (asset->decoded.isNull()) {
        asset->decoded.loadFromData(asset->encoded,
                                    asset->format.toLatin1().constData());
        if (!asset->decoded.isNull() && asset->decoded.format() != QImage::Format_ARGB32_Premultiplied) {
            asset->decoded = asset->decoded.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }
    }
    return asset->decoded;
}

QImage AssetStore::previewImage(const LayerId& id, int maxDimension) const
{
    const Asset* asset = find(id);
    if (!asset)
        return {};
    const QImage orig = decodedImage(id);
    if (orig.isNull())
        return {};
    if (orig.width() <= maxDimension && orig.height() <= maxDimension)
        return orig;
    if (asset->displayPreview.isNull()) {
        asset->displayPreview = orig.scaled(maxDimension, maxDimension,
                                            Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation);
        if (asset->displayPreview.format() != QImage::Format_ARGB32_Premultiplied) {
            asset->displayPreview = asset->displayPreview.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }
    }
    return asset->displayPreview;
}

} // namespace cc
