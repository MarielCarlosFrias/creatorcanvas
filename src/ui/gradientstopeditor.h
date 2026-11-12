#pragma once

#include "core/layers/Gradient.h"

#include <QWidget>

#include <vector>

namespace cc {

/// Editor de paragens de gradiente (multi-stop): barra com o gradiente
/// desenhado, clicar cria/selecciona uma paragem, arrastar move-a,
/// duplo-clique remove (mínimo 2), e a cor da seleccionada abre no
/// QColorDialog do inspector.
class GradientStopEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit GradientStopEditor(QWidget* parent = nullptr);

    void setGradient(const Gradient& gradient);
    Gradient gradient() const { return m_gradient; }

    /// Cor da paragem seleccionada (para o inspector pintar o botão).
    QColor selectedColor() const;
    int selectedStop() const { return m_selected; }

    /// Aplica uma cor à paragem seleccionada (chamado pelo inspector).
    void setSelectedColor(const QColor& color);

    static constexpr int kMinStops = 2;

signals:
    void gradientChanged();
    void selectionChanged();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return QSize(220, 44); }

private:
    QRectF barRect() const;
    float positionAt(const QPoint& widgetPos) const;
    int stopAt(const QPoint& widgetPos) const;

    Gradient m_gradient;
    int m_selected = 0;
    int m_dragIndex = -1;
};

} // namespace cc
