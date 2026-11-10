#pragma once

#include "CanvasTool.h"
#include <QColor>
#include <QPointF>
#include <QImage>
#include "core/image/ImageProcessing.h"

namespace cc {

/// Modular Tool for Brush, Pencil, Highlighter, Airbrush, and Eraser raster painting
class PaintTool final : public ICanvasTool
{
public:
    PaintTool() = default;
    ~PaintTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::Paint; }

    void activate(const ToolContext& ctx) override;
    void deactivate(const ToolContext& ctx) override;

    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;
    void cancel(const ToolContext& ctx) override;

    void setBrushType(int type) { m_params.type = static_cast<ImageProcessing::BrushType>(type); }
    ImageProcessing::BrushType brushType() const { return m_params.type; }
    void setColor(const QColor& color) { m_params.color = color; }
    QColor color() const { return m_params.color; }
    void setSize(int size) { m_params.size = size; }
    int size() const { return m_params.size; }
    void setOpacity(qreal opacity) { m_params.opacity = opacity; }
    qreal opacity() const { return m_params.opacity; }
    void setPressure(qreal pressure) { m_params.pressure = pressure; }
    qreal pressure() const { return m_params.pressure; }
    void setSpacing(qreal spacing) { m_params.spacing = spacing; }
    qreal spacing() const { return m_params.spacing; }
    void setJitter(qreal jitter) { m_params.jitter = jitter; }
    qreal jitter() const { return m_params.jitter; }
    void setTexture(const QImage& texture) { m_params.texture = texture; }
    const QImage& texture() const { return m_params.texture; }

private:
    ImageProcessing::BrushParams m_params;

    bool m_isPainting = false;
    bool m_hoverValid = false;
    QPointF m_prevPointLocal;
    QPointF m_currentDocPos;
    QImage m_workingImage;

    LayerId m_activeLayerId;
    LayerId m_origAssetId;
    int m_origWidth = 0;
    int m_origHeight = 0;
    AffineTransform m_origTransform;
};

} // namespace cc
