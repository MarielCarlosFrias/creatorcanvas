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

    void setBrushType(int type) { m_brushType = static_cast<ImageProcessing::BrushType>(type); }
    ImageProcessing::BrushType brushType() const { return m_brushType; }
    void setColor(const QColor& color) { m_color = color; }
    QColor color() const { return m_color; }
    void setSize(int size) { m_size = size; }
    int size() const { return m_size; }
    void setOpacity(qreal opacity) { m_opacity = opacity; }
    qreal opacity() const { return m_opacity; }

private:
    ImageProcessing::BrushType m_brushType = ImageProcessing::BrushType::Brush;
    QColor m_color = QColor(47, 111, 237);
    int m_size = 12;
    qreal m_opacity = 1.0;

    bool m_isPainting = false;
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
