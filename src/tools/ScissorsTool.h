#pragma once

#include "CanvasTool.h"
#include <QPolygonF>

namespace cc {

/// Modular Tool for Polygon Cut (Scissors)
class ScissorsTool final : public CanvasTool
{
public:
    ScissorsTool() = default;
    ~ScissorsTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::Scissors; }

    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseDoubleClick(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;
    void cancel(const ToolContext& ctx) override;

    void setKeepInside(bool keepInside) { m_keepInside = keepInside; }
    void setAutoCrop(bool autoCrop) { m_autoCrop = autoCrop; }
    void applyCut(const ToolContext& ctx);

private:
    bool m_keepInside = true;
    bool m_autoCrop = false;
    QPolygonF m_polygon;
    QPointF m_currentCursor;
};

} // namespace cc
