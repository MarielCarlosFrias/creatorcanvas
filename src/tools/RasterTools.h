#pragma once

#include "CanvasTool.h"

namespace cc {

/// Modular Tool for Magic Wand selection
class MagicWandTool final : public ICanvasTool
{
public:
    MagicWandTool() = default;
    ~MagicWandTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::MagicWand; }

    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void setTolerance(int tol) { m_tolerance = tol; }
    void setContiguous(bool contiguous) { m_contiguous = contiguous; }

private:
    int m_tolerance = 32;
    bool m_contiguous = true;
};

/// Modular Tool for Clone Stamp sampling
class CloneStampTool final : public ICanvasTool
{
public:
    CloneStampTool() = default;
    ~CloneStampTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::CloneStamp; }

    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;

    void setRadius(int radius) { m_radius = radius; }
    void setHardness(qreal hardness) { m_hardness = hardness; }
    void setOpacity(qreal opacity) { m_opacity = opacity; }

private:
    int m_radius = 16;
    qreal m_hardness = 0.8;
    qreal m_opacity = 1.0;

    bool m_hasSource = false;
    QPointF m_sourcePoint;
    bool m_cloning = false;
    QPointF m_currentMousePos;
};

/// Modular Tool for Flood Fill bucket
class FloodFillTool final : public ICanvasTool
{
public:
    FloodFillTool() = default;
    ~FloodFillTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::FloodFill; }

    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void setColor(const QColor& color) { m_color = color; }
    void setTolerance(int tol) { m_tolerance = tol; }

private:
    QColor m_color = Qt::black;
    int m_tolerance = 20;
};

} // namespace cc
