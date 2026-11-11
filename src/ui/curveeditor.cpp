#include "curveeditor.h"

#include "designtokens.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace cc {

namespace {

constexpr int kMaxPoints = 16;
constexpr qreal kHitRadius = 7.0;

} // namespace

CurveEditor::CurveEditor(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(180, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::CrossCursor);
    resetToIdentity();
}

void CurveEditor::resetToIdentity()
{
    m_points = {CurvePoint{0.0f, 0.0f}, CurvePoint{1.0f, 1.0f}};
    m_dragIndex = -1;
    update();
    emit pointsChanged();
}

void CurveEditor::setPoints(const std::vector<CurvePoint>& points)
{
    m_points = points;
    if (m_points.size() < 2)
        resetToIdentity();
    else
        update();
    emit pointsChanged();
}

QRectF CurveEditor::curveRect() const
{
    const qreal m = 10.0;
    return QRectF(m, m, width() - 2 * m, height() - 2 * m);
}

QPointF CurveEditor::normFromWidget(const QPoint& widgetPos) const
{
    const QRectF r = curveRect();
    const qreal x = r.width() <= 0 ? 0.0 : (widgetPos.x() - r.left()) / r.width();
    // Y do widget cresce para baixo; na curva, para cima.
    const qreal y = r.height() <= 0 ? 0.0 : (r.bottom() - widgetPos.y()) / r.height();
    return QPointF(std::clamp(x, 0.0, 1.0), std::clamp(y, 0.0, 1.0));
}

QPointF CurveEditor::widgetFromNorm(const QPointF& norm) const
{
    const QRectF r = curveRect();
    return QPointF(r.left() + norm.x() * r.width(),
                   r.bottom() - norm.y() * r.height());
}

int CurveEditor::pointIndexAt(const QPoint& widgetPos) const
{
    for (size_t i = 0; i < m_points.size(); ++i) {
        const QPointF w = widgetFromNorm(QPointF(m_points[i].input, m_points[i].output));
        if (QLineF(w, widgetPos).length() <= kHitRadius)
            return static_cast<int>(i);
    }
    return -1;
}

void CurveEditor::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const bool dark = palette().color(QPalette::Window).value() < 128;
    p.fillRect(rect(), dark ? QColor(0x1a, 0x1c, 0x20) : QColor(0xf4, 0xf6, 0xfa));

    const QRectF r = curveRect();

    // Grade 4x4.
    p.setPen(QPen(dark ? QColor(0x33, 0x37, 0x40) : QColor(0xd4, 0xd9, 0xe2), 1));
    for (int i = 0; i <= 4; ++i) {
        const qreal t = r.width() * i / 4.0;
        p.drawLine(QPointF(r.left() + t, r.top()), QPointF(r.left() + t, r.bottom()));
        const qreal v = r.height() * i / 4.0;
        p.drawLine(QPointF(r.left(), r.top() + v), QPointF(r.right(), r.top() + v));
    }
    p.setPen(QPen(dark ? QColor(0x4a, 0x50, 0x5c) : QColor(0xa8, 0xb0, 0xbe), 1));
    p.drawRect(r);

    // Spline: a mesma LUT que o motor aplica.
    const std::vector<quint8> lut = ToneCurve::buildCurveLut(m_points);
    QPainterPath path;
    for (int i = 0; i < ToneCurve::LUT_SIZE; ++i) {
        const QPointF n(static_cast<qreal>(i) / (ToneCurve::LUT_SIZE - 1),
                        static_cast<qreal>(lut[static_cast<size_t>(i)]) / 255.0);
        const QPointF w = widgetFromNorm(n);
        if (i == 0)
            path.moveTo(w);
        else
            path.lineTo(w);
    }
    p.setPen(QPen(Design::Accent(), 2));
    p.drawPath(path);

    // Pontos.
    for (size_t i = 0; i < m_points.size(); ++i) {
        const QPointF w = widgetFromNorm(QPointF(m_points[i].input, m_points[i].output));
        p.setPen(QPen(Qt::white, 1.5));
        p.setBrush(Design::Accent());
        p.drawEllipse(w, 4.5, 4.5);
    }
}

void CurveEditor::mousePressEvent(QMouseEvent* event)
{
    const int hit = pointIndexAt(event->pos());
    if (hit >= 0) {
        if (event->button() == Qt::RightButton) {
            if (m_points.size() > 2) {
                m_points.erase(m_points.begin() + hit);
                update();
                emit pointsChanged();
            }
            return;
        }
        m_dragIndex = hit;
        return;
    }
    if (event->button() == Qt::LeftButton && m_points.size() < kMaxPoints) {
        const QPointF n = normFromWidget(event->pos());
        CurvePoint pt;
        pt.input = static_cast<float>(n.x());
        pt.output = static_cast<float>(n.y());
        m_points.push_back(pt);
        std::sort(m_points.begin(), m_points.end(),
                  [](const CurvePoint& a, const CurvePoint& b) { return a.input < b.input; });
        m_dragIndex = pointIndexAt(event->pos());
        update();
        emit pointsChanged();
    }
}

void CurveEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragIndex < 0 || m_dragIndex >= static_cast<int>(m_points.size()))
        return;
    const QPointF n = normFromWidget(event->pos());
    m_points[static_cast<size_t>(m_dragIndex)].input = static_cast<float>(n.x());
    m_points[static_cast<size_t>(m_dragIndex)].output = static_cast<float>(n.y());
    update();
    emit pointsChanged();
}

void CurveEditor::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event)
    m_dragIndex = -1;
}

void CurveEditor::mouseDoubleClickEvent(QMouseEvent* event)
{
    const int hit = pointIndexAt(event->pos());
    if (hit >= 0 && m_points.size() > 2) {
        m_points.erase(m_points.begin() + hit);
        update();
        emit pointsChanged();
    }
}

QSize CurveEditor::sizeHint() const
{
    return QSize(260, 260);
}

QImage CurveEditor::lutImage() const
{
    const std::vector<quint8> lut = ToneCurve::buildCurveLut(m_points);
    QImage img(ToneCurve::LUT_SIZE, 1, QImage::Format_Grayscale8);
    for (int i = 0; i < ToneCurve::LUT_SIZE; ++i)
        img.scanLine(0)[i] = lut[static_cast<size_t>(i)];
    return img;
}

} // namespace cc
