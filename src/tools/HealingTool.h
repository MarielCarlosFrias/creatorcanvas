#pragma once

#include "CanvasTool.h"
#include <QColor>
#include <QImage>

namespace cc {

/// Modular Tool para Spot Healing com fonte manual (Alt+clique para definir fonte,
/// clique para curar) e Remoção de Olho Vermelho. Baseado no algoritmo de
/// ImageProcessing::spotHeal / ImageProcessing::removeRedEye.
class HealingTool final : public ICanvasTool
{
public:
    enum class Mode { SpotHeal, RedEye };

    HealingTool() = default;
    ~HealingTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::Healing; }

    void activate(const ToolContext& ctx) override;
    void deactivate(const ToolContext& ctx) override;
    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;
    void cancel(const ToolContext& ctx) override;

    void setRadius(int radius) { m_radius = qBound(2, radius, 400); }
    int radius() const { return m_radius; }
    void setHardness(qreal hardness) { m_hardness = qBound(0.0, hardness, 1.0); }
    qreal hardness() const { return m_hardness; }
    void setStrength(qreal strength) { m_strength = qBound(0.05, strength, 1.0); }
    qreal strength() const { return m_strength; }

    /// Alterna entre Spot Healing (0) e Remoção de Olho Vermelho (1).
    void setMode(int mode) { m_mode = (mode == 1) ? Mode::RedEye : Mode::SpotHeal; }
    /// 0 = Spot Healing, 1 = Red Eye (compatível com a API anterior).
    int mode() const { return m_mode == Mode::RedEye ? 1 : 0; }
    bool isRedEye() const { return m_mode == Mode::RedEye; }

    bool hasSource() const { return m_hasSource; }
    QPoint sourcePoint() const { return m_srcPointLocal; }
    LayerId sourceLayerId() const { return m_srcLayerId; }

private:
    Mode m_mode = Mode::SpotHeal;
    int m_radius = 25;
    qreal m_hardness = 0.7;
    qreal m_strength = 1.0;

    bool m_hasSource = false;
    QPoint m_srcPointLocal;
    LayerId m_srcLayerId;
    QImage m_srcSnapshot;

    bool m_isHealing = false;
    QPointF m_hoverDocPos;
    bool m_hoverValid = false;
    QImage m_workingImage;

    LayerId m_origAssetId;
    int m_origWidth = 0;
    int m_origHeight = 0;
    AffineTransform m_origTransform;
};

} // namespace cc
