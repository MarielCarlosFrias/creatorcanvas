#pragma once

#include <QImage>

#include <utility>

namespace cc {

/// Estado persistente da seleção de pixels do documento.
/// A máscara é sempre mantida em Alpha8 para que consumidores tenham um
/// formato previsível, independentemente de como a seleção foi criada.
class SelectionState final
{
public:
    const QImage& image() const { return m_image; }

    void setImage(QImage image)
    {
        if (!image.isNull())
            image = image.convertToFormat(QImage::Format_Alpha8);
        m_image = std::move(image);
    }

    void clear() { m_image = QImage(); }

private:
    QImage m_image;
};

} // namespace cc
