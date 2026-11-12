#pragma once

#include <QImage>
#include <QSize>

#include <array>

namespace cc {

/// Histograma de uma imagem: 256 níveis por canal mais a luminância.
/// Estrutura pura (sem Qt widgets) para poder ser testada sem GUI e
/// desenhada por qualquer widget.
struct ImageHistogram
{
    std::array<quint32, 256> red{};
    std::array<quint32, 256> green{};
    std::array<quint32, 256> blue{};
    std::array<quint32, 256> luminance{};

    /// Maior contagem entre todos os canais (útil para normalizar o desenho).
    quint32 maxCount() const;
};

/// Calcula o histograma da imagem. Pixéis transparentes são ignorados
/// (não contaminam os níveis com "preto" que não existe). Imagem nula ou
/// vazia devolve histograma a zeros.
ImageHistogram computeHistogram(const QImage& image);

} // namespace cc
