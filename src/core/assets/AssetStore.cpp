#include "AssetStore.h"

#include <QCryptographicHash>

namespace cc {

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
    if (asset->decoded.isNull())
        asset->decoded.loadFromData(asset->encoded,
                                    asset->format.toLatin1().constData());
    return asset->decoded;
}

} // namespace cc
