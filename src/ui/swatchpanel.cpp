#include "swatchpanel.h"

#include "ui/designtokens.h"

#include <QMouseEvent>
#include <QPainter>
#include <QSettings>

namespace cc {

namespace {

QVector<QColor> defaultPalette()
{
    return {
        QColor(0, 0, 0), QColor(255, 255, 255), QColor(128, 128, 128),
        QColor(230, 50, 50), QColor(255, 140, 0), QColor(255, 220, 0),
        QColor(60, 180, 75), QColor(0, 130, 200), QColor(70, 70, 200),
        QColor(145, 30, 180), QColor(240, 120, 160), QColor(120, 80, 40),
        QColor(170, 210, 230), QColor(255, 200, 150), QColor(200, 255, 200),
        QColor(30, 30, 60), QColor(90, 60, 30),
    };
}

} // namespace

SwatchPanel::SwatchPanel(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(140, 90);
    load();
}

void SwatchPanel::load()
{
    QSettings settings;
    const QStringList saved = settings.value(QStringLiteral("swatches/colors")).toStringList();
    if (saved.isEmpty()) {
        m_colors = defaultPalette();
        return;
    }
    m_colors.clear();
    for (const QString& name : saved)
        m_colors.append(QColor(name));
}

void SwatchPanel::save() const
{
    QStringList names;
    for (const QColor& c : m_colors)
        names.append(c.name(QColor::HexRgb));
    QSettings settings;
    settings.setValue(QStringLiteral("swatches/colors"), names);
    settings.sync();
}

int SwatchPanel::swatchAt(const QPoint& p) const
{
    const int originX = m_spacing;
    const int originY = m_spacing;
    const int cell = m_swatchSize + m_spacing;
    const int col = (p.x() - originX) / cell;
    const int row = (p.y() - originY) / cell;
    if (col < 0 || col >= m_columns || row < 0)
        return -1;
    const int localX = (p.x() - originX) % cell;
    const int localY = (p.y() - originY) % cell;
    if (localX > m_swatchSize || localY > m_swatchSize)
        return -1;
    return row * m_columns + col;
}

void SwatchPanel::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    const bool dark = palette().color(QPalette::Window).value() < 128;
    painter.fillRect(rect(), dark ? Design::DWindow() : Design::LWindow());

    const int total = m_colors.size() + 1; // + botão "+"
    for (int i = 0; i < total; ++i) {
        const int col = i % m_columns;
        const int row = i / m_columns;
        const QRect r(m_spacing + col * (m_swatchSize + m_spacing),
                      m_spacing + row * (m_swatchSize + m_spacing),
                      m_swatchSize, m_swatchSize);
        if (i < m_colors.size()) {
            painter.fillRect(r, m_colors.at(i));
            painter.setPen(QPen(dark ? QColor(0x55, 0x57, 0x5c) : QColor(0xb0, 0xb0, 0xb0), 1));
            painter.drawRect(r);
        } else {
            // botão "+": adiciona a cor atual
            painter.setPen(QPen(dark ? QColor(0xb0, 0xb5, 0xbd) : QColor(0x40, 0x42, 0x45), 1));
            painter.drawRect(r);
            painter.drawLine(r.center() + QPoint(-6, 0), r.center() + QPoint(6, 0));
            painter.drawLine(r.center() + QPoint(0, -6), r.center() + QPoint(0, 6));
        }
    }
}

void SwatchPanel::mousePressEvent(QMouseEvent* event)
{
    const int idx = swatchAt(event->pos());
    if (idx < 0)
        return;
    if (event->button() == Qt::LeftButton) {
        if (idx < m_colors.size()) {
            m_current = m_colors.at(idx);
            emit colorSelected(m_current);
            update();
        } else if (idx == m_colors.size()) {
            // adiciona a cor atual (se ainda não existir)
            if (!m_colors.contains(m_current)) {
                m_colors.append(m_current);
                save();
                update();
            }
        }
    } else if (event->button() == Qt::RightButton && idx < m_colors.size()) {
        m_colors.removeAt(idx);
        save();
        update();
    }
}

} // namespace cc
