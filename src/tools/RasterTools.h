#pragma once

#include "CanvasTool.h"
#include <QColor>
#include <QImage>

namespace cc {

/// Modular Tool for Magic Wand selection and color tolerance removal
class MagicWandTool final : public ICanvasTool
{
public:
    MagicWandTool() = default;
    ~MagicWandTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::MagicWand; }

    void activate(const ToolContext& ctx) override;
    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;

    void setTolerance(int tol) { m_tolerance = tol; }
    int tolerance() const { return m_tolerance; }
    void setContiguous(bool contiguous) { m_contiguous = contiguous; }
    bool contiguous() const { return m_contiguous; }

private:
    int m_tolerance = 25;
    bool m_contiguous = true;
};

/// Modular Tool for Clone Stamp sampling
class CloneStampTool final : public ICanvasTool
{
public:
    CloneStampTool() = default;
    ~CloneStampTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::CloneStamp; }

    void activate(const ToolContext& ctx) override;
    void deactivate(const ToolContext& ctx) override;
    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;
    void cancel(const ToolContext& ctx) override;

    void setRadius(int radius) { m_radius = radius; }
    int radius() const { return m_radius; }
    void setHardness(qreal hardness) { m_hardness = hardness; }
    qreal hardness() const { return m_hardness; }
    void setOpacity(qreal opacity) { m_opacity = opacity; }
    qreal opacity() const { return m_opacity; }

private:
    int m_radius = 20;
    qreal m_hardness = 0.8;
    qreal m_opacity = 1.0;

    bool m_hasSource = false;
    QPoint m_srcPointLocal;
    LayerId m_srcLayerId;

    bool m_isCloning = false;
    QPointF m_hoverDocPos;
    bool m_hoverValid = false;
    QImage m_workingImage;

    LayerId m_origAssetId;
    int m_origWidth = 0;
    int m_origHeight = 0;
    AffineTransform m_origTransform;
};

/// Modular Tool for Flood Fill bucket
class FloodFillTool final : public ICanvasTool
{
public:
    FloodFillTool() = default;
    ~FloodFillTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::FloodFill; }

    void activate(const ToolContext& ctx) override;
    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;

    void setColor(const QColor& color) { m_color = color; }
    QColor color() const { return m_color; }
    void setTolerance(int tol) { m_tolerance = tol; }
    int tolerance() const { return m_tolerance; }

private:
    QColor m_color = QColor(47, 111, 237);
    int m_tolerance = 20;
};

} // namespace cc
