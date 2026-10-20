#pragma once

#include "CanvasTool.h"
#include <QPointF>

namespace cc {

/// Ferramenta de seleção retangular/elíptica (Rectangular / Elliptical Marquee).
/// Shift = adicionar à seleção atual; Alt = subtrair.
class MarqueeTool final : public ICanvasTool
{
public:
    MarqueeTool() = default;
    ~MarqueeTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::Marquee; }

    void activate(const ToolContext& ctx) override;
    void deactivate(const ToolContext& ctx) override;
    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void keyPress(QKeyEvent* event, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;
    void cancel(const ToolContext& ctx) override;

    void setEllipse(bool ellipse) { m_ellipse = ellipse; }
    bool isEllipse() const { return m_ellipse; }
    void setFeather(int feather) { m_feather = qBound(0, feather, 50); }
    int feather() const { return m_feather; }

private:
    bool m_ellipse = false;
    int m_feather = 0;
    bool m_dragging = false;
    bool m_adding = false;
    bool m_subtracting = false;
    QPointF m_startDoc;
    QPointF m_endDoc;
};

} // namespace cc
