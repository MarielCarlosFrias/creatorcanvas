#pragma once

#include "core/layers/ToneAdjustments.h"

#include <QImage>
#include <QWidget>

#include <vector>

namespace cc {

/// Editor de curva de tons (estilo Photoshop): pontos arrastáveis sobre uma
/// grade 4x4, spline cúbica desenhada exactamente como o motor a avalia
/// (ToneCurve::buildCurveLut), para que a prévia não minta.
///
/// Interacção:
///  - arrastar um ponto move-o ( clampeado a 0..1 );
///  - clicar em área vazia insere um ponto (máximo 16);
///  - duplo-clique (ou botão direito) num ponto remove-o (mínimo 2).
class CurveEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit CurveEditor(QWidget* parent = nullptr);

    void setPoints(const std::vector<CurvePoint>& points);
    std::vector<CurvePoint> points() const { return m_points; }

    /// Repõe a diagonal (identidade).
    void resetToIdentity();

    /// Imagem da LUT resultante (1x256), útil para depuração/testes.
    QImage lutImage() const;

signals:
    void pointsChanged();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    QSize sizeHint() const override;

private:
    QRectF curveRect() const;
    QPointF normFromWidget(const QPoint& widgetPos) const;
    QPointF widgetFromNorm(const QPointF& norm) const;
    int pointIndexAt(const QPoint& widgetPos) const;

    std::vector<CurvePoint> m_points;
    int m_dragIndex = -1;
};

} // namespace cc
