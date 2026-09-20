#pragma once

#include <QColor>
#include <QtGlobal>

namespace cc {

struct ImageOutline
{
    bool enabled = false;
    QColor color{255, 255, 255};
    double width = 8.0;
    double blur = 0.0;
    double opacity = 1.0;

    bool operator==(const ImageOutline& other) const
    {
        return enabled == other.enabled && color == other.color
               && qFuzzyCompare(width, other.width)
               && qFuzzyCompare(blur, other.blur)
               && qFuzzyCompare(opacity, other.opacity);
    }
};

struct ImageEffects
{
    ImageOutline outline;

    bool operator==(const ImageEffects& other) const
    {
        return outline == other.outline;
    }

    bool operator!=(const ImageEffects& other) const
    {
        return !(*this == other);
    }
};

} // namespace cc
