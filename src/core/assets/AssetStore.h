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
    /// Bytes codificados do asset. VAZIO enquanto o asset ainda não foi
    /// codificado (codificação preguiçosa) — ver AssetStore::encodedBytes().
    QByteArray encoded;
    /// Bitmap original (pré-codificação). Guardado para codificar sob
    /// demanda no momento do salvamento.
    QImage source;
    mutable QImage decoded;
    mutable QImage displayPreview;

    bool isValid() const { return !id.isNull(); }
    /// Verdadeiro quando o asset ainda precisa de ser codificado.
    bool needsEncoding() const { return encoded.isEmpty() && !source.isNull(); }
};

class AssetStore
{
public:
    LayerId add(const QByteArray& encoded, const QString& format);
    LayerId addImage(const QImage& image, const QString& format = QStringLiteral("png"));
    void restore(const Asset& asset);
    /// Remove um asset do store (GC de comandos expulsos da pilha).
    /// Retorna true se existia.
    bool remove(const LayerId& id);
    const Asset* find(const LayerId& id) const;
    QImage decodedImage(const LayerId& id) const;
    QImage previewImage(const LayerId& id, int maxDimension = 2048) const;
    const QVector<Asset>& assets() const { return m_assets; }
    /// Bytes codificados do asset, codificando sob demanda se necessário
    /// (a codificação PNG é cara em imagens grandes e só é necessária no
    /// salvamento). É const porque a codificação é memoizada no asset.
    const QByteArray& encodedBytes(const LayerId& id) const;

private:
    // mutable: a codificação sob demanda acontece num método const
    // (encodedBytes) e memoiza o resultado dentro do próprio asset.
    mutable QVector<Asset> m_assets;
};

} // namespace cc
