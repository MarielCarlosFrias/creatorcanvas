#pragma once

#include "CanvasTool.h"
#include "core/image/ImageProcessing.h"
#include <QColor>
#include <QImage>
#include <QPointF>

namespace cc {

/// Ferramenta de gradiente (Linear/Radial). Arraste no canvas para definir
/// o eixo; Shift mantém o ângulo em 45°. Aplica na camada de imagem ativa,
/// respeitando a seleção de pixel ativa quando existir.
class GradientTool final : public ICanvasTool
{
public:
    GradientTool() = default;
    ~GradientTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::Gradient; }

    void activate(const ToolContext& ctx) override;
    void deactivate(const ToolContext& ctx) override;
    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void keyPress(QKeyEvent* event, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;
    void cancel(const ToolContext& ctx) override;

    void setRadial(bool radial) { m_radial = radial; }
    bool isRadial() const { return m_radial; }
    void setStartColor(const QColor& color) { m_startColor = color; }
    QColor startColor() const { return m_startColor; }
    void setEndColor(const QColor& color) { m_endColor = color; }
    QColor endColor() const { return m_endColor; }
    void setOpacity(qreal opacity) { m_opacity = qBound(0.05, opacity, 1.0); }
    qreal opacity() const { return m_opacity; }

private:
    bool m_radial = false;
    QColor m_startColor = QColor(61, 126, 255);
    QColor m_endColor = QColor(0, 0, 0, 0);
    qreal m_opacity = 1.0;

    bool m_dragging = false;
    QPointF m_startDoc;
    QPointF m_endDoc;
    QPointF m_lastDoc;
    LayerId m_layerId;
    LayerId m_origAssetId;
    int m_origWidth = 0;
    int m_origHeight = 0;
    AffineTransform m_origTransform;
};

/// Ferramenta de retoque por pincel: Blur, Sharpen, Smudge, Dodge, Burn, Sponge.
class RetouchTool final : public ICanvasTool
{
public:
    RetouchTool() = default;
    ~RetouchTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::Retouch; }

    void activate(const ToolContext& ctx) override;
    void deactivate(const ToolContext& ctx) override;
    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;
    void cancel(const ToolContext& ctx) override;

    void setMode(int mode) { m_mode = static_cast<ImageProcessing::RetouchMode>(mode); }
    int mode() const { return static_cast<int>(m_mode); }
    void setSize(int size) { m_size = qBound(2, size, 400); }
    int size() const { return m_size; }
    void setStrength(qreal strength) { m_strength = qBound(0.05, strength, 1.0); }
    qreal strength() const { return m_strength; }

private:
    void applyAt(const QPointF& fromLocal, const QPointF& toLocal, const ToolContext& ctx);

    ImageProcessing::RetouchMode m_mode = ImageProcessing::RetouchMode::Blur;
    int m_size = 40;
    qreal m_strength = 0.4;

    bool m_active = false;
    bool m_hoverValid = false;
    QPointF m_prevLocal;
    QPointF m_currentDoc;
    QImage m_workingImage;
    LayerId m_layerId;
    LayerId m_origAssetId;
    int m_origWidth = 0;
    int m_origHeight = 0;
    AffineTransform m_origTransform;
};

/// Ferramenta de reparo: Spot Healing (pincel) e Red Eye (clique).
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

    void setMode(int mode)
    {
        m_mode = mode == 1 ? Mode::RedEye : Mode::SpotHeal;
    }
    int mode() const { return m_mode == Mode::RedEye ? 1 : 0; }
    void setRadius(int radius) { m_radius = qBound(2, radius, 400); }
    int radius() const { return m_radius; }
    void setStrength(qreal strength) { m_strength = qBound(0.05, strength, 1.0); }
    qreal strength() const { return m_strength; }

private:
    void applyAt(const QPointF& local, bool firstSample, const ToolContext& ctx);

    Mode m_mode = Mode::SpotHeal;
    int m_radius = 24;
    qreal m_strength = 0.85;

    bool m_active = false;
    bool m_hoverValid = false;
    QPoint m_sourceOffset{0, 0};
    QPointF m_currentDoc;
    QImage m_workingImage;
    LayerId m_layerId;
    LayerId m_origAssetId;
    int m_origWidth = 0;
    int m_origHeight = 0;
    AffineTransform m_origTransform;
};

} // namespace cc
