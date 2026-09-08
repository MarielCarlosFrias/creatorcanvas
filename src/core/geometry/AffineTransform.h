#pragma once

#include <QRectF>
#include <QTransform>

namespace cc {

struct AffineTransform
{
    QPointF position{0, 0};
    double rotationDeg = 0.0;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double shearX = 0.0; // 3D / Perspective tilt horizontal
    double shearY = 0.0; // 3D / Perspective tilt vertical

    bool operator==(const AffineTransform& other) const
    {
        return position == other.position
            && rotationDeg == other.rotationDeg
            && scaleX == other.scaleX
            && scaleY == other.scaleY
            && shearX == other.shearX
            && shearY == other.shearY;
    }

    bool operator!=(const AffineTransform& other) const
    {
        return !(*this == other);
    }

    QTransform matrix(const QRectF& contentBounds) const
    {
        QTransform t;
        t.translate(position.x(), position.y());
        t.rotate(rotationDeg);
        if (shearX != 0.0 || shearY != 0.0)
            t.shear(shearX, shearY);
        t.scale(scaleX, scaleY);
        const QPointF center = contentBounds.center();
        t.translate(-center.x(), -center.y());
        return t;
    }
};

} // namespace cc
