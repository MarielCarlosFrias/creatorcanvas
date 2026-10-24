#pragma once

#include "core/layers/Layer.h"
#include "core/snap/SnapEngine.h"
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <QVector>

namespace cc {

class Document;
class ImageLayer;

struct CanvasHandles
{
    QPointF points[9];
    bool valid = false;
};

class CanvasOverlayRenderer
{
public:
    static void drawSelection(QPainter* painter,
                              const CanvasHandles& handles,
                              bool isLocked);

    static void drawMultiSelection(QPainter* painter,
                                   const QVector<CanvasHandles>& handleSets);

    static void drawRubberBand(QPainter* painter,
                               const QPointF& startDocPos,
                               const QPointF& currentDocPos,
                               const QTransform& toDevice);

    static void drawGrid(QPainter* painter,
                         int docW,
                         int docH,
                         int gridSpacing,
                         const QTransform& toDevice);

    static void drawSafeZones(QPainter* painter,
                              int docW,
                              int docH,
                              int safeZoneMode,
                              const QTransform& toDevice);

    static void drawMockupPlacement(QPainter* painter,
                                    const QVector<QPointF>& mockupCorners,
                                    const QTransform& toDevice);

    static void drawSnapGuides(QPainter* painter,
                               int docW,
                               int docH,
                               const QList<GuideLine>& guides,
                               double zoom,
                               const QTransform& toDevice);

    static void drawPixelSelectionOutline(QPainter* painter,
                                          const ImageLayer* layer,
                                          const QImage& pixelSelection,
                                          const QImage& sourceImage,
                                          const QTransform& toDevice);
};

} // namespace cc
