#pragma once

#include "core/layers/Layer.h"
#include "core/geometry/AffineTransform.h"
#include "ui/canvasoverlayrenderer.h"

#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QTransform>
#include <QCursor>
#include <QList>

namespace cc {

enum class CanvasGestureType {
    None,
    Move,
    Scale,
    Rotate
};

class CanvasGestureHandler
{
public:
    static constexpr double kHandleRadius = 7.0;
    static constexpr double kRotateOffset = 26.0;

    CanvasGestureHandler() = default;

    // --- Geometria dos Handles e Hit-Testing ---
    static CanvasHandles computeHandles(const Layer& layer, const QTransform& docToDevice);
    static int hitTestHandle(const QPointF& widgetPos, const CanvasHandles& handles, double radius = kHandleRadius);
    static Qt::CursorShape handleCursor(int handleIndex);
    static Qt::CursorShape determineCursor(const QPointF& widgetPos,
                                           const CanvasHandles& handles,
                                           bool isLocked);

    // --- Ciclo de Vida do Gesto ---
    bool isInteracting() const { return m_type != CanvasGestureType::None; }
    CanvasGestureType gestureType() const { return m_type; }
    int activeHandle() const { return m_activeHandle; }

    void startRotate(const Layer& layer, const QPointF& docPos);
    void startScale(const Layer& layer, int handle, const QPointF& docPos);
    void startMove(const Layer& layer, const QPointF& docPos, const QList<Layer*>& multiLayers = {});

    /// Computa a nova transformação da camada com base na posição atual no documento
    AffineTransform updateTransform(const QPointF& docNow) const;

    /// Computa o delta de movimento para multi-seleção ou translação
    QPointF computeMoveDelta(const QPointF& docNow) const;

    void finish();

    // Acessores de estado inicial para comandos de histórico/commit
    const AffineTransform& gestureStart() const { return m_gestureStart; }
    const QSizeF& gestureStartBox() const { return m_gestureStartBox; }
    const QList<AffineTransform>& multiGestureStarts() const { return m_multiGestureStarts; }

private:
    CanvasGestureType m_type = CanvasGestureType::None;
    int m_activeHandle = -1;
    AffineTransform m_gestureStart;
    QRectF m_gestureBounds;
    QPointF m_gestureStartDoc;
    QPointF m_fixedDoc;
    QPointF m_fixedLocal;
    QPointF m_localPress;
    QPointF m_rotateCenter;
    double m_rotateStartAngle = 0.0;
    QSizeF m_gestureStartBox;
    QList<AffineTransform> m_multiGestureStarts;
};

} // namespace cc
