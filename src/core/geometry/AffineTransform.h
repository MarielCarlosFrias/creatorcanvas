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

    bool operator==(const AffineTransform& other) const
    {
        return position == other.position
            && rotationDeg == other.rotationDeg
            && scaleX == other.scaleX
            && scaleY == other.scaleY;
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
        t.scale(scaleX, scaleY);
        const QPointF center = contentBounds.center();
        t.translate(-center.x(), -center.y());
        return t;
    }
};

} // namespace cc
