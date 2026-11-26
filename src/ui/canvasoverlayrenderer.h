#pragma once

#include "core/document/Guide.h"
#include "core/document/WebSlice.h"
#include "core/layers/Layer.h"
#include "core/snap/SnapEngine.h"
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <QVector>

#include <vector>

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

    static void drawPixelGrid(QPainter* painter,
                              int docW,
                              int docH,
                              const QTransform& toDevice,
                              bool darkTheme = true);

    static void drawSafeZones(QPainter* painter,
                              int docW,
                              int docH,
                              int safeZoneMode,
                              const QTransform& toDevice);

    static void drawMockupPlacement(QPainter* painter,
                                    const QVector<QPointF>& mockupCorners,
                                    const QTransform& toDevice,
                                    const QImage& logo = QImage(),
                                    bool freeShape = false);

    static void drawSnapGuides(QPainter* painter,
                               int docW,
                               int docH,
                               const QList<GuideLine>& guides,
                               double zoom,
                               const QTransform& toDevice);

    /// Guias do documento (linhas cyan tracejadas em coordenadas de doc).
    static void drawGuides(QPainter* painter,
                           int docW,
                           int docH,
                           const std::vector<Guide>& guides,
                           const QTransform& toDevice);

    /// Réguas sobrepostas no topo/esquerda do widget (faixas de |rulerSize| px
    /// com ticks a cada 10 px de documento e rótulos a cada 100 px).
    static void drawRulers(QPainter* painter,
                           const QSize& widgetSize,
                           const QTransform& docToDevice,
                           int rulerSize,
                           bool darkTheme);

    /// Desenha as fatias de exportação web: rectângulo tracejado + etiqueta
    /// com o nome e o formato. É assim que o utilizador vê o que cada fatia
    /// vai exportar.
    static void drawWebSlices(QPainter* painter,
                              const std::vector<WebSlice>& slices,
                              const QTransform& toDevice);

    static void drawPixelSelectionOutline(QPainter* painter,
                                          const ImageLayer* layer,
                                          const QImage& pixelSelection,
                                          const QImage& sourceImage,
                                          const QTransform& toDevice,
                                          /// Imagem do contorno já construída (cache do caller);
                                          /// quando nula, é construída aqui (caminho lento).
                                          QImage* cachedOverlay = nullptr);
};

} // namespace cc
