#pragma once

#include "CanvasTool.h"
#include <QRectF>
#include <QList>
#include <QMap>

namespace cc {

/// Tool handling single and multi-layer selection, transform handles (move, scale, rotate),
/// rubber-band selection, and snap engine integration.
class SelectTool final : public ICanvasTool
{
public:
    SelectTool() = default;
    ~SelectTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::Select; }

    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;
    void cancel(const ToolContext& ctx) override;

    void setSelectedLayers(const QList<LayerId>& ids);
    QList<LayerId> selectedLayers() const { return m_selectedLayers; }
    LayerId primarySelectedId() const;
    void clearSelection();

private:
    struct HandleSet {
        QPointF points[9];
        bool valid = false;
    };

    enum class Gesture { None, Move, Scale, Rotate, RubberBand };

    HandleSet computeHandleSet(const ToolContext& ctx) const;
    int hitTestHandle(const QPointF& docPos, const HandleSet& handles, double zoom) const;
    QRectF computeSelectionBounds(const ToolContext& ctx) const;

    QList<LayerId> m_selectedLayers;
    Gesture m_gesture = Gesture::None;
    int m_activeHandle = -1;
    QPointF m_dragStartDocPos;
    QPointF m_rubberBandStart;
    QRectF m_rubberBandRect;

    // Undo state tracking for commit
    QMap<LayerId, AffineTransform> m_initialTransforms;
};

} // namespace cc
