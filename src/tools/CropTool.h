#pragma once

#include "CanvasTool.h"
#include <QRectF>

namespace cc {

/// Modular Tool for Crop manipulation with interactive 8-handle resizing and aspect ratio support
class CropTool final : public ICanvasTool
{
public:
    CropTool() = default;
    ~CropTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::Crop; }

    void activate(const ToolContext& ctx) override;
    void deactivate(const ToolContext& ctx) override;

    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseDoubleClick(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void keyPress(QKeyEvent* event, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;
    void cancel(const ToolContext& ctx) override;

    void setAspectRatio(double ratio, const ToolContext& ctx);
    double aspectRatio() const { return m_aspectRatio; }
    void applyCrop(const ToolContext& ctx);
    void initCropRect(const ToolContext& ctx);
    QRectF cropRect() const { return m_cropRect; }
    int cropHandleAt(const QPointF& widgetPos, const ToolContext& ctx) const;

private:

    QRectF m_cropRect;
    double m_aspectRatio = 0.0; // 0 = Free
    int m_activeCropHandle = -1;
    QPointF m_cropDragStartLocal;
    QRectF m_cropStartRect;
};

} // namespace cc
