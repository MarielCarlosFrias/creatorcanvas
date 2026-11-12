#pragma once

#include "CanvasTool.h"

namespace cc {

/// Edição de caminhos Bézier (avançado): mostra os nós e handles da camada
/// vectorial selecionada e permite arrastá-los, inserir nós no caminho,
/// apagar nós e quebrar a suavidade (Alt ao largar).
///
/// O caminho é derivado dos nós (com os handles de Bézier), por mexer nos
/// nós é mexer no desenho — cada gesto commitado é um passo de histórico
/// (comando de propriedade, fundível durante o arrasto).
class VectorNodeTool final : public ICanvasTool
{
public:
    VectorNodeTool() = default;
    ~VectorNodeTool() override = default;

    CanvasToolType type() const override { return CanvasToolType::VectorNodes; }

    void activate(const ToolContext& ctx) override;
    void deactivate(const ToolContext& ctx) override;

    void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void mouseDoubleClick(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) override;
    void drawOverlay(QPainter* painter, const ToolContext& ctx) override;

    /// Custo de acesso a um nó/handle em píxeis de documento.
    static constexpr double kHitDistance = 7.0;

private:
    enum class Drag { None, Node, HandleIn, HandleOut };

    int nodeAt(const QPointF& docPos, double radius) const;
    int handleAt(const QPointF& docPos, double radius, Drag& which) const;
    int segmentAt(const QPointF& docPos, double radius) const;

    void rebuildPath();

    VectorLayer* m_layer = nullptr;
    std::vector<VectorNode> m_startNodes;   // estado ao prgemid o gesto
    QPainterPath m_path;
    Drag m_drag = Drag::None;
    int m_dragIndex = -1;
    bool m_altPressed = false;
};

} // namespace cc
