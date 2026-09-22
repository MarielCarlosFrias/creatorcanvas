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
    mutable QImage displayPreview;

    bool isValid() const { return !id.isNull(); }
};

class AssetStore
{
public:
    LayerId add(const QByteArray& encoded, const QString& format);
    LayerId addImage(const QImage& image, const QString& format = QStringLiteral("png"));
    void restore(const Asset& asset);
    const Asset* find(const LayerId& id) const;
    QImage decodedImage(const LayerId& id) const;
    QImage previewImage(const LayerId& id, int maxDimension = 2048) const;
    const QVector<Asset>& assets() const { return m_assets; }

private:
    QVector<Asset> m_assets;
};

} // namespace cc
