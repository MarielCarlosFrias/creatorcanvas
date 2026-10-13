#pragma once

#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>

#include <algorithm>

namespace cc {

enum class GradientKind { Linear, Radial };

struct GradientStop
{
    float position = 0.0f;   ///< 0..1 ao longo do eixo/raio
    QColor color = Qt::black;

    bool operator==(const GradientStop& o) const
    {
        return position == o.position && color == o.color;
    }
    bool operator!=(const GradientStop& o) const { return !(*this == o); }
};

/// Preenchimento em gradiente para BackgroundLayer e ShapeLayer.
/// Quando `enabled` e `stops` nao vazio, substitui a cor solida (fill).
struct Gradient
{
    bool enabled = false;
    GradientKind kind = GradientKind::Linear;
    double angleDeg = 90.0;        ///< Linear: direcao em graus (0 = Leste, 90 = Sul)
    QPointF center{0.5, 0.5};      ///< Radial: centro relativo ao retangulo (0..1)
    double radius = 0.75;          ///< Radial: raio relativo ao maior lado
    QVector<GradientStop> stops;    ///< ordenado por position ao desenhar

    bool operator==(const Gradient& o) const
    {
        return enabled == o.enabled && kind == o.kind
            && angleDeg == o.angleDeg && center == o.center
            && radius == o.radius && stops == o.stops;
    }
    bool operator!=(const Gradient& o) const { return !(*this == o); }

    bool isValid() const { return enabled && stops.size() >= 2; }
};

} // namespace cc
