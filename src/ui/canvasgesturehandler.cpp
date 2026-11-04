#include "canvasgesturehandler.h"
#include "core/layers/Layer.h"

#include <QLineF>
#include <QtMath>
#include <cmath>

namespace cc {

CanvasHandles CanvasGestureHandler::computeHandles(const Layer& layer, const QTransform& docToDevice)
{
    CanvasHandles set;
    const QRectF bounds = layer.contentBounds();
    if (bounds.isEmpty())
        return set;

    const QTransform toScreen = layer.transform.matrix(bounds) * docToDevice;
    set.points[0] = toScreen.map(bounds.topLeft());
    set.points[1] = toScreen.map(bounds.topRight());
    set.points[2] = toScreen.map(bounds.bottomRight());
    set.points[3] = toScreen.map(bounds.bottomLeft());
    set.points[4] = (set.points[0] + set.points[1]) / 2;
    set.points[5] = (set.points[1] + set.points[2]) / 2;
    set.points[6] = (set.points[2] + set.points[3]) / 2;
    set.points[7] = (set.points[3] + set.points[0]) / 2;
    set.points[8] = set.points[4] + QPointF(0, -kRotateOffset);
    set.valid = true;
    return set;
}

int CanvasGestureHandler::hitTestHandle(const QPointF& widgetPos, const CanvasHandles& handles, double radius)
{
    if (!handles.valid)
        return -1;
    for (int i = 0; i < 9; ++i) {
        if (QLineF(widgetPos, handles.points[i]).length() <= radius)
            return i;
    }
    return -1;
}

Qt::CursorShape CanvasGestureHandler::handleCursor(int handle)
{
    switch (handle) {
    case 0: case 2: return Qt::SizeFDiagCursor;
    case 1: case 3: return Qt::SizeBDiagCursor;
    case 4: case 6: return Qt::SizeVerCursor;
    case 5: case 7: return Qt::SizeHorCursor;
    case 8:         return Qt::CrossCursor;
    }
    return Qt::ArrowCursor;
}

Qt::CursorShape CanvasGestureHandler::determineCursor(const QPointF& widgetPos,
                                                     const CanvasHandles& handles,
                                                     bool isLocked)
{
    const int handle = hitTestHandle(widgetPos, handles);
    if (handle >= 0) {
        if (isLocked)
            return Qt::ForbiddenCursor;
        return handleCursor(handle);
    }
    return Qt::ArrowCursor;
}

void CanvasGestureHandler::startRotate(const Layer& layer, const QPointF& docPos)
{
    m_type = CanvasGestureType::Rotate;
    m_activeHandle = 8;
    m_gestureStart = layer.transform;
    m_gestureBounds = layer.contentBounds();
    if (layer.type() == LayerType::Text)
        m_gestureStartBox = static_cast<const TextLayer&>(layer).box;
    else
        m_gestureStartBox = QSizeF();

    const QTransform matrix = m_gestureStart.matrix(m_gestureBounds);
    m_gestureStartDoc = docPos;
    m_localPress = matrix.inverted().map(docPos);
    m_rotateCenter = matrix.map(m_gestureBounds.center());
    m_rotateStartAngle = std::atan2(
        m_gestureStartDoc.y() - m_rotateCenter.y(),
        m_gestureStartDoc.x() - m_rotateCenter.x());
}

void CanvasGestureHandler::startScale(const Layer& layer, int handle, const QPointF& docPos)
{
    m_type = CanvasGestureType::Scale;
    m_activeHandle = handle;
    m_gestureStart = layer.transform;
    m_gestureBounds = layer.contentBounds();
    if (layer.type() == LayerType::Text)
        m_gestureStartBox = static_cast<const TextLayer&>(layer).box;
    else
        m_gestureStartBox = QSizeF();

    const QTransform matrix = m_gestureStart.matrix(m_gestureBounds);
    m_gestureStartDoc = docPos;
    m_localPress = matrix.inverted().map(docPos);

    static const int opposite[8] = {2, 3, 0, 1, 6, 7, 4, 5};
    const QRectF b = m_gestureBounds;
    switch (opposite[m_activeHandle]) {
    case 0: m_fixedLocal = b.topLeft(); break;
    case 1: m_fixedLocal = b.topRight(); break;
    case 2: m_fixedLocal = b.bottomRight(); break;
    case 3: m_fixedLocal = b.bottomLeft(); break;
    case 4: m_fixedLocal = (b.topLeft() + b.topRight()) / 2; break;
    case 5: m_fixedLocal = (b.topRight() + b.bottomRight()) / 2; break;
    case 6: m_fixedLocal = (b.bottomRight() + b.bottomLeft()) / 2; break;
    default: m_fixedLocal = (b.bottomLeft() + b.topLeft()) / 2; break;
    }
    m_fixedDoc = matrix.map(m_fixedLocal);
}

void CanvasGestureHandler::startMove(const Layer& layer, const QPointF& docPos, const QList<Layer*>& multiLayers)
{
    m_type = CanvasGestureType::Move;
    m_activeHandle = -1;
    m_gestureStart = layer.transform;
    m_gestureStartDoc = docPos;
    if (layer.type() == LayerType::Text)
        m_gestureStartBox = static_cast<const TextLayer&>(layer).box;
    else
        m_gestureStartBox = QSizeF();

    m_multiGestureStarts.clear();
    for (const auto* ml : multiLayers) {
        if (ml)
            m_multiGestureStarts.append(ml->transform);
        else
            m_multiGestureStarts.append(AffineTransform());
    }
}

AffineTransform CanvasGestureHandler::updateTransform(const QPointF& docNow) const
{
    AffineTransform t = m_gestureStart;

    if (m_type == CanvasGestureType::Move) {
        const QPointF delta = docNow - m_gestureStartDoc;
        t.position += delta;
        return t;
    }

    if (m_type == CanvasGestureType::Rotate) {
        const double angle = std::atan2(docNow.y() - m_rotateCenter.y(),
                                        docNow.x() - m_rotateCenter.x());
        t.rotationDeg = m_gestureStart.rotationDeg
                        + qRadiansToDegrees(angle - m_rotateStartAngle);
        t.position = m_gestureStart.position;
        return t;
    }

    if (m_type == CanvasGestureType::Scale) {
        const QTransform inv = m_gestureStart.matrix(m_gestureBounds).inverted();
        const QPointF localNow = inv.map(docNow);
        const QPointF center = m_gestureBounds.center();
        const double dx = localNow.x() - m_fixedLocal.x();
        const double dy = localNow.y() - m_fixedLocal.y();
        const double px = m_localPress.x() - m_fixedLocal.x();
        const double py = m_localPress.y() - m_fixedLocal.y();

        if (m_activeHandle == 5 || m_activeHandle == 7) {
            if (qAbs(px) > 1e-6)
                t.scaleX = m_gestureStart.scaleX * (dx / px);
        } else if (m_activeHandle == 4 || m_activeHandle == 6) {
            if (qAbs(py) > 1e-6)
                t.scaleY = m_gestureStart.scaleY * (dy / py);
        } else {
            double ratio = 1.0;
            if (qAbs(px) >= qAbs(py) && qAbs(px) > 1e-6)
                ratio = dx / px;
            else if (qAbs(py) > 1e-6)
                ratio = dy / py;
            t.scaleX = m_gestureStart.scaleX * ratio;
            t.scaleY = m_gestureStart.scaleY * ratio;
        }

        if (qAbs(t.scaleX) < 0.01)
            t.scaleX = t.scaleX < 0 ? -0.01 : 0.01;
        if (qAbs(t.scaleY) < 0.01)
            t.scaleY = t.scaleY < 0 ? -0.01 : 0.01;

        const double rad = qDegreesToRadians(m_gestureStart.rotationDeg);
        const double cosR = std::cos(rad);
        const double sinR = std::sin(rad);
        const QPointF v(m_fixedLocal.x() - center.x(),
                        m_fixedLocal.y() - center.y());
        const QPointF scaled(v.x() * t.scaleX, v.y() * t.scaleY);
        const QPointF rotated(scaled.x() * cosR - scaled.y() * sinR,
                              scaled.x() * sinR + scaled.y() * cosR);
        t.position = m_fixedDoc - rotated;
        return t;
    }

    return t;
}

QPointF CanvasGestureHandler::computeMoveDelta(const QPointF& docNow) const
{
    return docNow - m_gestureStartDoc;
}

void CanvasGestureHandler::finish()
{
    m_type = CanvasGestureType::None;
    m_activeHandle = -1;
    m_multiGestureStarts.clear();
}

} // namespace cc
