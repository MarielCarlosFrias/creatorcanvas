#pragma once

#include "CanvasTool.h"
#include <QColor>
#include <QPointF>
#include <QVector>
#include "core/image/ImageProcessing.h"

namespace cc {

/// Modular Tool for Brush, Pencil, Highlighter, Airbrush, and Eraser raster painting
class PaintTool final : public CanvasTool
{
public:
    PaintTool() = default;
    ~PaintTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::Paint; }

    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;
    void cancel(const ToolContext& ctx) override;

    void setBrushType(ImageProcessing::BrushType type) { m_brushType = type; }
    void setColor(const QColor& color) { m_color = color; }
    void setSize(int size) { m_size = size; }
    void setOpacity(qreal opacity) { m_opacity = opacity; }

private:
    ImageProcessing::BrushType m_brushType = ImageProcessing::BrushType::Brush;
    QColor m_color = Qt::black;
    int m_size = 12;
    qreal m_opacity = 1.0;

    bool m_painting = false;
    QVector<QPointF> m_strokePoints;
    QPointF m_currentMousePos;
};

} // namespace cc
