#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QVector>

#include "../layers/Layer.h"

namespace cc {

struct Asset
{
    LayerId id;
    QString format;
    int width = 0;
    int height = 0;
    QString sha256;
    QByteArray encoded;
    mutable QImage decoded;

    bool isValid() const { return !id.isNull(); }
};

class AssetStore
{
public:
    LayerId add(const QByteArray& encoded, const QString& format);
    void restore(const Asset& asset);
    const Asset* find(const LayerId& id) const;
    QImage decodedImage(const LayerId& id) const;
    const QVector<Asset>& assets() const { return m_assets; }

private:
    QVector<Asset> m_assets;
};

} // namespace cc
