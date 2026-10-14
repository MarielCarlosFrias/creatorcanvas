#pragma once

#include <QColor>

namespace cc {

/// Sombra projetada aplicável a ShapeLayers (silhueta desfocada).
struct ShapeShadow
{
    bool enabled = false;
    QColor color{0, 0, 0, 170};
    double offsetX = 5.0;
    double offsetY = 5.0;
    double blur = 8.0;      // 0 = borda dura

    bool operator==(const ShapeShadow& o) const
    {
        return enabled == o.enabled && color == o.color
            && offsetX == o.offsetX && offsetY == o.offsetY && blur == o.blur;
    }
    bool operator!=(const ShapeShadow& o) const { return !(*this == o); }
};

} // namespace cc
