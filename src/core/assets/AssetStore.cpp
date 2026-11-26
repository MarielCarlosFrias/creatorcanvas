#include "AssetStore.h"

#include <QBuffer>
#include <QCryptographicHash>

namespace cc {

LayerId AssetStore::addImage(const QImage& image, const QString& format)
{
    if (image.isNull())
        return {};

    Asset asset;
    asset.id = newLayerId();
    asset.format = format;
    asset.width = image.width();
    asset.height = image.height();
    // Codificação PREGUIÇOSA: guardar o bitmap e codificar só quando o
    // projeto é salvo. Codificar PNG a cada pincel (3000×2000 ≈ 300 ms)
    // congelava a UI ao soltar o mouse.
    asset.source = image;
    asset.decoded = image;

    m_assets.append(asset);
    m_index.insert(asset.id, m_assets.size() - 1);
    return asset.id;
}

const QByteArray& AssetStore::encodedBytes(const LayerId& id) const
{
    static const QByteArray empty;
    const int index = m_index.value(id, -1);
    if (index < 0 || index >= m_assets.size() || m_assets.at(index).id != id)
        return empty;

    Asset& asset = m_assets[index];
    if (asset.encoded.isEmpty() && !asset.source.isNull()) {
        QByteArray encoded;
        QBuffer buffer(&encoded);
        buffer.open(QIODevice::WriteOnly);
        asset.source.save(&buffer, asset.format.toLatin1().constData());
        asset.encoded = encoded;
        asset.sha256 = QString::fromLatin1(
            QCryptographicHash::hash(asset.encoded, QCryptographicHash::Sha256).toHex());
        asset.source = QImage(); // libera a memória: bytes já codificados
    }
    return asset.encoded;
}

void AssetStore::reindex()
{
    m_index.clear();
    m_index.reserve(m_assets.size());
    for (int i = 0; i < m_assets.size(); ++i)
        m_index.insert(m_assets.at(i).id, i);
}

qint64 AssetStore::approximateBytes(const LayerId& id) const
{
    const int index = m_index.value(id, -1);
    if (index < 0 || index >= m_assets.size() || m_assets.at(index).id != id)
        return 0;
    const Asset& asset = m_assets.at(index);
    qint64 bytes = asset.encoded.size();
    if (!asset.source.isNull())
        bytes += asset.source.sizeInBytes();
    if (!asset.decoded.isNull())
        bytes += asset.decoded.sizeInBytes();
    if (!asset.displayPreview.isNull())
        bytes += asset.displayPreview.sizeInBytes();
    return bytes;
}

bool AssetStore::remove(const LayerId& id)
{
    // GC: chamado só com ids que nem o documento nem a pilha de undo
    // referem mais. QImage é shared — cópias vivas (ex.: prévia na UI)
    // mantêm os pixels; só o cache do store é liberado.
    int index = m_index.value(id, -1);
    if (index < 0 || index >= m_assets.size() || m_assets.at(index).id != id) {
        // Índice em falta (id duplicado vindo de restore?): procura linear.
        for (int i = 0; i < m_assets.size(); ++i) {
            if (m_assets.at(i).id == id) {
                index = i;
                break;
            }
        }
    }
    if (index < 0 || index >= m_assets.size() || m_assets.at(index).id != id)
        return false;

    m_assets.removeAt(index);
    reindex(); // removeAt desloca as posições seguintes
    return true;
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
    m_index.insert(asset.id, m_assets.size() - 1);
    return asset.id;
}

void AssetStore::restore(const Asset& asset)
{
    m_assets.append(asset);
    m_index.insert(asset.id, m_assets.size() - 1);
}

const Asset* AssetStore::find(const LayerId& id) const
{
    const int index = m_index.value(id, -1);
    if (index >= 0 && index < m_assets.size() && m_assets.at(index).id == id)
        return &m_assets[index];
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
