#pragma once

#include "CanvasTool.h"
#include <QRectF>

namespace cc {

/// Modular Tool for Crop manipulation with fixed aspect ratio support
class CropTool final : public CanvasTool
{
public:
    CropTool() = default;
    ~CropTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::Crop; }

    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;
    void cancel(const ToolContext& ctx) override;

    void setAspectRatio(double ratio) { m_aspectRatio = ratio; }
    void applyCrop(const ToolContext& ctx);

private:
    double m_aspectRatio = 0.0; // 0 = Free
    bool m_dragging = false;
    QPointF m_startPos;
    QRectF m_cropRect;
};

} // namespace cc
