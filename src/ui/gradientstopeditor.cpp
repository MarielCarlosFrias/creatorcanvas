#include "gradientstopeditor.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace cc {

namespace {

constexpr qreal kBarHeight = 20.0;
constexpr qreal kHandleWidth = 12.0;

} // namespace

GradientStopEditor::GradientStopEditor(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(120, 44);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);
    if (m_gradient.stops.size() < kMinStops) {
        m_gradient.stops = {{0.0f, QColor(255, 210, 0)}, {1.0f, QColor(255, 45, 0)}};
    }
}

void GradientStopEditor::setGradient(const Gradient& gradient)
{
    m_gradient = gradient;
    if (m_gradient.stops.size() < kMinStops)
        m_gradient.stops = {{0.0f, QColor(255, 210, 0)}, {1.0f, QColor(255, 45, 0)}};
    if (m_selected >= static_cast<int>(m_gradient.stops.size()))
        m_selected = static_cast<int>(m_gradient.stops.size()) - 1;
    update();
}

QColor GradientStopEditor::selectedColor() const
{
    if (m_selected < 0 || m_selected >= m_gradient.stops.size())
        return QColor();
    return m_gradient.stops[static_cast<size_t>(m_selected)].color;
}

void GradientStopEditor::setSelectedColor(const QColor& color)
{
    if (m_selected < 0 || m_selected >= m_gradient.stops.size())
        return;
    m_gradient.stops[static_cast<size_t>(m_selected)].color = color;
    update();
    emit gradientChanged();
}

QRectF GradientStopEditor::barRect() const
{
    const qreal m = kHandleWidth / 2.0 + 4.0;
    return QRectF(m, 0, width() - 2 * m, kBarHeight);
}

float GradientStopEditor::positionAt(const QPoint& widgetPos) const
{
    const QRectF bar = barRect();
    const qreal t = bar.width() <= 0 ? 0.0 : (widgetPos.x() - bar.left()) / bar.width();
    return static_cast<float>(std::clamp(t, 0.0, 1.0));
}

int GradientStopEditor::stopAt(const QPoint& widgetPos) const
{
    const QRectF bar = barRect();
    for (int i = 0; i < static_cast<int>(m_gradient.stops.size()); ++i) {
        const qreal x = bar.left() + bar.width() * m_gradient.stops[static_cast<size_t>(i)].position;
        if (std::abs(widgetPos.x() - x) <= kHandleWidth / 2.0 && widgetPos.y() <= height())
            return static_cast<int>(i);
    }
    return -1;
}

void GradientStopEditor::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const QRectF bar = barRect();

    // Barra do gradiente.
    QLinearGradient lg(bar.topLeft(), bar.topRight());
    for (const auto& s : m_gradient.stops)
        lg.setColorAt(std::clamp(double(s.position), 0.0, 1.0), s.color);
    p.fillRect(bar, lg);
    p.setPen(QPen(QColor(0x55, 0x55, 0x55), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(bar);

    // Handles das paragens por baixo da barra.
    for (int i = 0; i < static_cast<int>(m_gradient.stops.size()); ++i) {
        const qreal x = bar.left() + bar.width() * m_gradient.stops[static_cast<size_t>(i)].position;
        const QColor color = m_gradient.stops[static_cast<size_t>(i)].color;
        const bool selected = static_cast<int>(i) == m_selected;
        p.setPen(selected ? QPen(Qt::white, 2) : QPen(QColor(0x44, 0x44, 0x44), 1));
        p.setBrush(color);
        p.drawRect(QRectF(x - kHandleWidth / 2.0, bar.bottom() + 4,
                          kHandleWidth, height() - bar.bottom() - 8));
    }
}

void GradientStopEditor::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    const int hit = stopAt(event->pos());
    if (hit >= 0) {
        m_selected = hit;
        m_dragIndex = hit;
        emit selectionChanged();
        update();
        return;
    }

    // Clique na barra cria uma paragem nova nessa posição.
    if (barRect().contains(event->pos()) && m_gradient.stops.size() < 32) {
        GradientStop stop;
        stop.position = positionAt(event->pos());
        stop.color = selectedColor().isValid() ? selectedColor() : QColor(Qt::white);
        auto stops = m_gradient.stops;
        stops.push_back(stop);
        std::sort(stops.begin(), stops.end(),
                  [](const GradientStop& a, const GradientStop& b) {
                      return a.position < b.position;
                  });
        m_gradient.stops = stops;
        for (int i = 0; i < static_cast<int>(stops.size()); ++i) {
            if (std::abs(stops[static_cast<size_t>(i)].position - stop.position) < 0.0001f
                && stops[static_cast<size_t>(i)].color == stop.color) {
                m_selected = i;
                break;
            }
        }
        m_dragIndex = m_selected;
        emit selectionChanged();
        emit gradientChanged();
        update();
    }
}

void GradientStopEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragIndex < 0 || m_dragIndex >= m_gradient.stops.size())
        return;
    m_gradient.stops[static_cast<size_t>(m_dragIndex)].position = positionAt(event->pos());
    std::sort(m_gradient.stops.begin(), m_gradient.stops.end(),
              [](const GradientStop& a, const GradientStop& b) {
                  return a.position < b.position;
              });
    update();
    emit gradientChanged();
}

void GradientStopEditor::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event)
    m_dragIndex = -1;
}

void GradientStopEditor::mouseDoubleClickEvent(QMouseEvent* event)
{
    const int hit = stopAt(event->pos());
    if (hit < 0 || m_gradient.stops.size() <= kMinStops)
        return;
    m_gradient.stops.erase(m_gradient.stops.begin() + hit);
    m_selected = std::min(m_selected, static_cast<int>(m_gradient.stops.size()) - 1);
    emit selectionChanged();
    emit gradientChanged();
    update();
}

} // namespace cc
