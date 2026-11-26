#pragma once

#include <QImage>
#include <QString>
#include <QTransform>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace cc {

/// Espaço em que os índices da máscara são interpretados. Seleções de bitmap
/// (varinha, tesoura, IA) pertencem à camada; marquee sem camada e canais de
/// documento usam coordenadas do canvas.
enum class SelectionSpace {
    Document,
    Layer
};

/// Uma seleção de píxeis guardada com nome (canal de seleção).
struct SavedSelection {
    QUuid id;
    QString name;
    QImage mask; // Format_Alpha8
    SelectionSpace space = SelectionSpace::Document;
};

/// Estado persistente da seleção de pixels do documento.
/// A máscara é sempre mantida em Alpha8 para que consumidores tenham um
/// formato previsível, independentemente de como a seleção foi criada.
class SelectionState final
{
public:
    const QImage& image() const { return m_image; }
    SelectionSpace imageSpace() const { return m_imageSpace; }

    /// Última seleção não-nula que foi substituída ou limpa — base do
    /// comando "Reselect" (Ctrl+Shift+D no Photoshop).
    const QImage& previousImage() const { return m_previous; }
    SelectionSpace previousImageSpace() const { return m_previousSpace; }

    void setImage(QImage image, SelectionSpace space = SelectionSpace::Document)
    {
        if (!image.isNull()) {
            image = image.convertToFormat(QImage::Format_Alpha8);
            if (!m_image.isNull()) {
                m_previous = m_image;
                m_previousSpace = m_imageSpace;
            }
            m_imageSpace = space;
        } else {
            m_imageSpace = SelectionSpace::Document;
        }
        m_image = std::move(image);
    }

    void clear()
    {
        if (!m_image.isNull()) {
            m_previous = m_image;
            m_previousSpace = m_imageSpace;
        }
        m_image = QImage();
        m_imageSpace = SelectionSpace::Document;
    }

    /// Reselect: torna a seleção anterior a ativa outra vez.
    /// Devolve false quando não há seleção anterior.
    bool reselect()
    {
        if (m_previous.isNull())
            return false;
        setImage(m_previous, m_previousSpace);
        return true;
    }

    // ---------- Seleções guardadas (canais, em memória) ----------
    const std::vector<SavedSelection>& savedSelections() const { return m_saved; }

    QUuid addSavedSelection(const QString& name, const QImage& mask,
                            SelectionSpace space = SelectionSpace::Document)
    {
        return addSavedSelection(QUuid::createUuid(), name, mask, space);
    }

    /// Versão com id explícito — usada ao restaurar do projeto (preserva o id).
    QUuid addSavedSelection(const QUuid& id, const QString& name, const QImage& mask,
                            SelectionSpace space = SelectionSpace::Document)
    {
        SavedSelection s;
        s.id = id.isNull() ? QUuid::createUuid() : id;
        s.name = name.isEmpty() ? QStringLiteral("Selection") : name;
        s.mask = mask.isNull() ? QImage()
                              : mask.convertToFormat(QImage::Format_Alpha8);
        s.space = space;
        m_saved.push_back(std::move(s));
        return m_saved.back().id;
    }

    bool removeSavedSelection(const QUuid& id)
    {
        for (auto it = m_saved.begin(); it != m_saved.end(); ++it) {
            if (it->id == id) {
                m_saved.erase(it);
                return true;
            }
        }
        return false;
    }

    const SavedSelection* findSavedSelection(const QUuid& id) const
    {
        for (const auto& s : m_saved) {
            if (s.id == id)
                return &s;
        }
        return nullptr;
    }

    void clearSavedSelections() { m_saved.clear(); }

    /// Aplica uma transformação de documento antigo -> novo apenas às
    /// máscaras nesse espaço. Máscaras de camada não são redimensionadas nem
    /// rodadas com o canvas: continuam alinhadas ao bitmap local.
    void transformDocumentMasks(const QSize& outputSize,
                                const QTransform& oldToNew)
    {
        if (outputSize.isEmpty() || outputSize.width() <= 0
            || outputSize.height() <= 0)
            return;
        bool invertible = false;
        const QTransform newToOld = oldToNew.inverted(&invertible);
        if (!invertible)
            return;

        m_image = transformedMask(m_image, m_imageSpace, outputSize, newToOld);
        m_previous = transformedMask(m_previous, m_previousSpace,
                                     outputSize, newToOld);
        for (SavedSelection& saved : m_saved)
            saved.mask = transformedMask(saved.mask, saved.space,
                                         outputSize, newToOld);
    }

private:
    static uchar coverageAt(const QImage& mask, int x, int y)
    {
        if (x < 0 || y < 0 || x >= mask.width() || y >= mask.height())
            return 0;
        return mask.constScanLine(y)[x];
    }

    static QImage transformedMask(const QImage& input, SelectionSpace space,
                                  const QSize& outputSize,
                                  const QTransform& newToOld)
    {
        if (input.isNull() || space != SelectionSpace::Document)
            return input;
        const QImage source = input.convertToFormat(QImage::Format_Alpha8);
        QImage output(outputSize, QImage::Format_Alpha8);
        output.fill(0);

        // Amostragem bilinear nos centros dos píxeis. Translações inteiras,
        // recorte/padding e rotações de 90/180 graus são exatas; offsets
        // fracionários preservam cobertura parcial em vez de a binarizar.
        for (int y = 0; y < output.height(); ++y) {
            uchar* dst = output.scanLine(y);
            for (int x = 0; x < output.width(); ++x) {
                const QPointF sourceCenter =
                    newToOld.map(QPointF(x + 0.5, y + 0.5));
                if (!std::isfinite(sourceCenter.x())
                    || !std::isfinite(sourceCenter.y()))
                    continue;
                const double sx = sourceCenter.x() - 0.5;
                const double sy = sourceCenter.y() - 0.5;
                const int x0 = static_cast<int>(std::floor(sx));
                const int y0 = static_cast<int>(std::floor(sy));
                const double fx = sx - x0;
                const double fy = sy - y0;
                const double top = coverageAt(source, x0, y0) * (1.0 - fx)
                    + coverageAt(source, x0 + 1, y0) * fx;
                const double bottom = coverageAt(source, x0, y0 + 1) * (1.0 - fx)
                    + coverageAt(source, x0 + 1, y0 + 1) * fx;
                const double value = top * (1.0 - fy) + bottom * fy;
                dst[x] = static_cast<uchar>(
                    std::clamp(static_cast<int>(std::lround(value)), 0, 255));
            }
        }
        return output;
    }

    QImage m_image;
    SelectionSpace m_imageSpace = SelectionSpace::Document;
    QImage m_previous;
    SelectionSpace m_previousSpace = SelectionSpace::Document;
    std::vector<SavedSelection> m_saved;
};

} // namespace cc
