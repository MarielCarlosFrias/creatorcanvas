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

/// Ported from PhotoCraft's Shadow effect (crates/doc/src/effects.rs)
struct ImageDropShadow
{
    bool enabled = false;
    QColor color{0, 0, 0, 180};
    double distance = 10.0;
    double angleDeg = 120.0; // Angulo em graus (Photoshop convention)
    double blur = 15.0;
    double opacity = 0.75;

    bool operator==(const ImageDropShadow& other) const
    {
        return enabled == other.enabled && color == other.color
               && qFuzzyCompare(distance, other.distance)
               && qFuzzyCompare(angleDeg, other.angleDeg)
               && qFuzzyCompare(blur, other.blur)
               && qFuzzyCompare(opacity, other.opacity);
    }
};

/// Ported from PhotoCraft's ColorOverlay (crates/doc/src/effects.rs)
struct ImageColorOverlay
{
    bool enabled = false;
    QColor color{255, 0, 0};
    double opacity = 0.5;

    bool operator==(const ImageColorOverlay& other) const
    {
        return enabled == other.enabled && color == other.color
               && qFuzzyCompare(opacity, other.opacity);
    }
};

/// Ported from PhotoCraft's InnerShadow (crates/doc/src/effects.rs)
struct ImageInnerShadow
{
    bool enabled = false;
    QColor color{0, 0, 0, 180};
    double distance = 8.0;
    double angleDeg = 120.0;
    double blur = 10.0;
    double opacity = 0.6;

    bool operator==(const ImageInnerShadow& other) const
    {
        return enabled == other.enabled && color == other.color
               && qFuzzyCompare(distance, other.distance)
               && qFuzzyCompare(angleDeg, other.angleDeg)
               && qFuzzyCompare(blur, other.blur)
               && qFuzzyCompare(opacity, other.opacity);
    }
};

struct ImageEffects
{
    ImageOutline outline;
    ImageDropShadow dropShadow;
    ImageInnerShadow innerShadow;
    ImageColorOverlay colorOverlay;

    bool operator==(const ImageEffects& other) const
    {
        return outline == other.outline && dropShadow == other.dropShadow
               && innerShadow == other.innerShadow
               && colorOverlay == other.colorOverlay;
    }

    bool operator!=(const ImageEffects& other) const
    {
        return !(*this == other);
    }
};

} // namespace cc
