#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QPolygonF>
#include <QRect>
#include <QSize>
#include "../layers/ToneAdjustments.h"

namespace cc {

struct MockupWarpResult {
    QImage image;
    QPointF topLeft;
};

struct ScissorsCutResult {
    QImage image;
    QPoint offset;
};

class ImageProcessing {
public:
    static QImage cropImage(const QImage& source, const QRect& cropRect);
    static QRect calculateAspectCropRect(const QSize& sourceSize, double aspectRatio);
    static QImage removeBackground(const QImage& source, const QPoint& seedPoint,
                                   int tolerance = 20, bool contiguous = true);
    static QImage prepareMockupLogo(const QImage& logo, bool removeBg, int tolerance = 18);
    static ScissorsCutResult scissorsCut(const QImage& source, const QPolygonF& polygon,
                                         bool keepInside = true, bool cropToBoundingRect = false);
    static QImage cloneStamp(const QImage& target, const QImage& source,
                             const QPoint& srcPoint, const QPoint& dstPoint,
                             int radius, qreal opacity = 1.0, qreal hardness = 0.8);

    enum class BrushType {
        Brush, Pencil, Highlighter, Airbrush, Eraser,
        Texture, Scatter, Rotation
    };

    struct BrushParams {
        BrushType type = BrushType::Brush;
        QColor color;
        int size = 12;
        qreal opacity = 1.0;
        qreal pressure = 1.0;
        qreal spacing = 10.0;
        qreal jitter = 0.0;
        qreal rotation = 0.0;
        qreal rotationJitter = 0.0;
        QImage texture;
        qreal textureScale = 1.0;
    };

    static QImage paintStroke(const QImage& target, const QPointF& prevPoint, const QPointF& curPoint,
                             BrushType brush, const QColor& color, int size, qreal opacity = 1.0);
    static QImage paintStroke(const QImage& target, const QPointF& prevPoint, const QPointF& curPoint,
                             const BrushParams& params);
    static void paintStrokeInPlace(QImage& target, const QPointF& prevPoint, const QPointF& curPoint,
                                   BrushType brush, const QColor& color, int size, qreal opacity = 1.0);
    static void paintStrokeInPlace(QImage& target, const QPointF& prevPoint, const QPointF& curPoint,
                                   const BrushParams& params);
    static QImage floodFill(const QImage& source, const QPoint& seedPoint,
                            const QColor& fillColor, int tolerance = 20);

    enum class RetouchMode {
        Blur, Sharpen, Smudge, Dodge, Burn, Sponge
    };
    static QImage retouchStroke(const QImage& target, const QPointF& prevPoint,
                                const QPointF& curPoint, RetouchMode mode,
                                int size, qreal strength = 0.4);
    static QImage makeShapeSelection(const QSize& size, const QRectF& rect,
                                     bool ellipse, int feather = 0);
    static QImage combineSelections(const QImage& base, const QImage& overlay,
                                    bool subtract);
    static QImage paintGradient(const QImage& target, const QPointF& start,
                                const QPointF& end, const QColor& startColor,
                                const QColor& endColor, bool radial = false,
                                qreal opacity = 1.0, const QImage& mask = {});

    static QImage adjustColors(const QImage& source, double brightness, double contrast,
                              double saturation, double temperature, double hue = 0.0,
                              double shadows = 0.0, double highlights = 0.0,
                              double vignette = 0.0);
    static QImage applyBlur(const QImage& source, double radius);
    static QImage applySharpen(const QImage& source, double amount);
    static QImage applyLevels(const QImage& source, const LevelsChannel& master,
                             const LevelsChannel& red = {}, const LevelsChannel& green = {},
                             const LevelsChannel& blue = {});
    static QImage applyCurves(const QImage& source, const std::vector<CurvePoint>& master,
                             const std::vector<CurvePoint>& red = {},
                             const std::vector<CurvePoint>& green = {},
                             const std::vector<CurvePoint>& blue = {});

    enum class PresetFilter { Grayscale, Sepia, Vintage, HighContrast, Invert, Posterize, Threshold };
    static QImage applyPresetFilter(const QImage& source, PresetFilter preset);
    static QImage applyGradientMap(const QImage& source, const QColor& color1,
                                   const QColor& color2, bool reverse = false);

    static QImage chromaKey(const QImage& source, const QColor& keyColor,
                            int tolerance = 40, double transparency = 0.0);
    static QImage colorToAlpha(const QImage& source, const QColor& keyColor,
                               double transparency = 0.0, double opacityThreshold = 1.0,
                               double tolerance = 0.0);
    static QImage removeRedEye(const QImage& source, const QPoint& center, int radius);
    static QImage spotHeal(const QImage& target, const QPoint& targetPoint,
                           const QPoint& sourcePoint, int radius,
                           qreal strength = 0.85);
    static MockupWarpResult mockupWarp(const QImage& source, const QPolygonF& destinationPoints,
                                       bool freeShape = false);
};

} // namespace cc
