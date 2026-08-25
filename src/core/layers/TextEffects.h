#pragma once

#include <QColor>

namespace cc {

struct TextOutline
{
    bool enabled = false;
    QColor color{0, 0, 0};
    double width = 4.0;

    bool operator==(const TextOutline& other) const
    {
        return enabled == other.enabled && color == other.color
               && width == other.width;
    }
};

struct TextShadow
{
    bool enabled = false;
    QColor color{0, 0, 0, 170};
    double offsetX = 4.0;
    double offsetY = 4.0;
    double blur = 6.0;

    bool operator==(const TextShadow& other) const
    {
        return enabled == other.enabled && color == other.color
               && offsetX == other.offsetX && offsetY == other.offsetY
               && blur == other.blur;
    }
};

struct TextEffects
{
    TextOutline outline;
    TextShadow shadow;

    bool operator==(const TextEffects& other) const
    {
        return outline == other.outline && shadow == other.shadow;
    }
    bool operator!=(const TextEffects& other) const { return !(*this == other); }
};

} // namespace cc
