#pragma once

#include "CanvasTool.h"
#include <QColor>
#include <QImage>
#include <QPainterPath>

namespace cc {

/// Ferramenta Caneta (Pen Tool) para desenhar curvas Bézier cúbicas.
/// Clique cria pontos âncora; arraste para ajustar handles; Alt+clique para
/// quebrar suavidade (corner point). Clique no primeiro ponto fecha o caminho.
class PenTool final : public ICanvasTool
{
public:
    PenTool() = default;
    ~PenTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::Vector; }

    void activate(const ToolContext& ctx) override;
    void deactivate(const ToolContext& ctx) override;
    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;
    void cancel(const ToolContext& ctx) override;

    void setStrokeColor(const QColor& c) { m_strokeColor = c; }
    QColor strokeColor() const { return m_strokeColor; }
    void setStrokeWidth(double w) { m_strokeWidth = w; }
    double strokeWidth() const { return m_strokeWidth; }
    void setFillColor(const QColor& c) { m_fillColor = c; }
    QColor fillColor() const { return m_fillColor; }

    bool hasPath() const { return !m_path.isEmpty(); }
    QPainterPath path() const { return m_path; }

private:
    QPainterPath m_path;
    QPointF m_lastPoint;
    QPointF m_currentHandle;
    bool m_drawing = false;
    bool m_hasSource = false;

    QColor m_strokeColor = Qt::black;
    QColor m_fillColor = Qt::transparent;
    double m_strokeWidth = 2.0;
};

} // namespace cc
