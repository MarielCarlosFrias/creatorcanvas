#pragma once

#include "core/image/ImageHistogram.h"

#include <QWidget>

namespace cc {

/// Desenho do histograma (R/G/B sobrepostos + luminância a tracejado).
/// Marca os níveis de entrada/saída quando fornecidos, para o utilizador
/// ver onde os blacks/whites dos Níveis caem na distribuição real.
class HistogramView final : public QWidget
{
    Q_OBJECT

public:
    explicit HistogramView(QWidget* parent = nullptr);

    void setHistogram(const ImageHistogram& histogram);

    /// Marcadores de nível (0..255) desenhados a vermelho; -1 = esconder.
    void setLevelMarkers(int inBlack, int inWhite, int outBlack, int outWhite);

    QSize sizeHint() const override { return QSize(240, 90); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    ImageHistogram m_histogram;
    int m_inBlack = -1;
    int m_inWhite = -1;
    int m_outBlack = -1;
    int m_outWhite = -1;
};

} // namespace cc
