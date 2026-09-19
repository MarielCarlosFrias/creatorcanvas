#pragma once

#include "CanvasTool.h"
#include <QPolygonF>

namespace cc {

/// Modular Tool for Polygon Cut (Scissors) with local coordinate mapping and visual node overlay
class ScissorsTool final : public ICanvasTool
{
public:
    ScissorsTool() = default;
    ~ScissorsTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::Scissors; }

    void activate(const ToolContext& ctx) override;
    void deactivate(const ToolContext& ctx) override;

    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseDoubleClick(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void keyPress(QKeyEvent* event, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;
    void cancel(const ToolContext& ctx) override;

    void setKeepInside(bool keepInside) { m_keepInside = keepInside; }
    bool keepInside() const { return m_keepInside; }
    void setAutoCrop(bool autoCrop) { m_autoCrop = autoCrop; }
    bool autoCrop() const { return m_autoCrop; }
    void applyCut(const ToolContext& ctx);

    const QPolygonF& polygon() const { return m_polygon; }

private:
    bool m_keepInside = true;
    bool m_autoCrop = true;
    bool m_isDrawing = false;
    QPolygonF m_polygon;
    QPointF m_currentHover;
};

} // namespace cc
