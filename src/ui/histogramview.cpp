#include "histogramview.h"

#include <QPainter>
#include <QPainterPath>

namespace cc {

HistogramView::HistogramView(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(120, 60);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void HistogramView::setHistogram(const ImageHistogram& histogram)
{
    m_histogram = histogram;
    update();
}

void HistogramView::setLevelMarkers(int inBlack, int inWhite, int outBlack, int outWhite)
{
    m_inBlack = inBlack;
    m_inWhite = inWhite;
    m_outBlack = outBlack;
    m_outWhite = outWhite;
    update();
}

void HistogramView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const bool dark = palette().color(QPalette::Window).value() < 128;
    p.fillRect(rect(), dark ? QColor(0x1a, 0x1c, 0x20) : QColor(0xf4, 0xf6, 0xfa));

    const qreal w = width();
    const qreal h = height();
    const quint32 peak = m_histogram.maxCount();
    if (peak == 0) {
        p.setPen(QColor(0x88, 0x88, 0x88));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("—"));
        return;
    }

    // Barras dos canais, translúcidas e sobrepostas (photoshop-like).
    auto drawChannel = [&](const std::array<quint32, 256>& counts, const QColor& color) {
        QPainterPath path;
        path.moveTo(0, h);
        for (int i = 0; i < 256; ++i) {
            const qreal x = w * i / 255.0;
            // Raiz quadrada suaviza picos isolados sem achatar o resto.
            const qreal v = std::sqrt(static_cast<qreal>(counts[static_cast<size_t>(i)]) / peak);
            path.lineTo(x, h - v * (h - 2));
        }
        path.lineTo(w, h);
        path.closeSubpath();
        p.fillPath(path, QColor(color.red(), color.green(), color.blue(), 90));
    };
    drawChannel(m_histogram.red, QColor(255, 60, 60));
    drawChannel(m_histogram.green, QColor(60, 200, 90));
    drawChannel(m_histogram.blue, QColor(70, 120, 255));

    // Luminância a tracejado, por cima.
    p.setPen(QPen(QColor(0xdd, 0xdd, 0xdd, 200), 1, Qt::DashLine));
    for (int i = 0; i < 256; ++i) {
        const qreal x = w * i / 255.0;
        const qreal v = std::sqrt(static_cast<qreal>(m_histogram.luminance[static_cast<size_t>(i)]) / peak);
        const qreal y = h - v * (h - 2);
        if (i == 0)
            p.drawLine(QPointF(x, y), QPointF(x, y));
        else {
            static qreal prevY = 0;
            static qreal prevX = 0;
            p.drawLine(QPointF(prevX, prevY), QPointF(x, y));
            prevY = y;
            prevX = x;
        }
    }

    // Marcadores de nível (linhas verticais vermelhas).
    p.setPen(QPen(QColor(255, 70, 70, 220), 1));
    const auto marker = [&](int level) {
        if (level < 0 || level > 255)
            return;
        const qreal x = w * level / 255.0;
        p.drawLine(QPointF(x, 0), QPointF(x, h));
    };
    marker(m_inBlack);
    marker(m_inWhite);
    marker(m_outBlack);
    marker(m_outWhite);
}

} // namespace cc
