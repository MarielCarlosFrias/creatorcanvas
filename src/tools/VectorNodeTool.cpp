#include "VectorNodeTool.h"

#include "core/Document.h"
#include "core/commands/LayerPropertyCommands.h"
#include "core/history/CommandStack.h"
#include "core/layers/Layer.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace cc {

/// Comando de propriedade: os nós são passados por valor, como o setter exige.
using SetVectorNodesCommand = LayerPropertyCommand<std::vector<VectorNode>>;

void VectorNodeTool::activate(const ToolContext& ctx)
{
    m_layer = nullptr;
    if (ctx.document && !ctx.selectedLayerId.isNull()) {
        Layer* layer = ctx.document->findLayer(ctx.selectedLayerId);
        if (layer && layer->type() == LayerType::Vector)
            m_layer = static_cast<VectorLayer*>(layer);
    }
    m_drag = Drag::None;
    m_dragIndex = -1;
    if (m_layer)
        rebuildPath();

    if (ctx.requestStatusMessage && m_layer)
        ctx.requestStatusMessage(QStringLiteral(
            "Nós: arraste para mover, Alt+largar quebra a suavidade, duplo-clique apaga, "
            "clique no caminho insere."));
}

void VectorNodeTool::deactivate(const ToolContext& ctx)
{
    Q_UNUSED(ctx)
    m_layer = nullptr;
    m_drag = Drag::None;
    m_dragIndex = -1;
    m_path = QPainterPath();
}

void VectorNodeTool::rebuildPath()
{
    m_path = QPainterPath();
    if (!m_layer || m_layer->nodes.empty())
        return;

    const auto& nodes = m_layer->nodes;
    m_path.moveTo(nodes.front().position);
    for (size_t i = 0; i < nodes.size(); ++i) {
        const VectorNode& n = nodes[i];
        const VectorNode& next = nodes[(i + 1) % nodes.size()];
        const QPointF c1 = n.position + n.handleOut;
        const QPointF c2 = next.position + next.handleIn;
        if (m_layer->closed || i + 1 < nodes.size())
            m_path.cubicTo(c1, c2, next.position);
    }
    if (m_layer->closed)
        m_path.closeSubpath();
}

int VectorNodeTool::nodeAt(const QPointF& docPos, double radius) const
{
    if (!m_layer)
        return -1;
    for (size_t i = 0; i < m_layer->nodes.size(); ++i) {
        if (QLineF(m_layer->nodes[i].position, docPos).length() <= radius)
            return static_cast<int>(i);
    }
    return -1;
}

int VectorNodeTool::handleAt(const QPointF& docPos, double radius, Drag& which) const
{
    if (!m_layer)
        return -1;
    for (size_t i = 0; i < m_layer->nodes.size(); ++i) {
        const VectorNode& n = m_layer->nodes[i];
        // Handle de comprimento zero coincide com o nó: ignorá-lo, senão
        // rouba o arraste do nó (foi um bug real apanhado pelos testes).
        if (!n.handleIn.isNull()
            && QLineF(n.position + n.handleIn, docPos).length() <= radius) {
            which = Drag::HandleIn;
            return static_cast<int>(i);
        }
        if (!n.handleOut.isNull()
            && QLineF(n.position + n.handleOut, docPos).length() <= radius) {
            which = Drag::HandleOut;
            return static_cast<int>(i);
        }
    }
    which = Drag::None;
    return -1;
}

int VectorNodeTool::segmentAt(const QPointF& docPos, double radius) const
{
    if (!m_layer || m_layer->nodes.size() < 2)
        return -1;
    for (size_t i = 0; i + 1 < m_layer->nodes.size(); ++i) {
        const VectorNode& a = m_layer->nodes[i];
        const VectorNode& b = m_layer->nodes[i + 1];
        const QLineF line(a.position, b.position);
        // Distância ponto-segmento (aproximação pela bounding do segmento).
        const QPointF v = line.p2() - line.p1();
        const qreal len2 = v.x() * v.x() + v.y() * v.y();
        if (len2 <= 0.0001)
            continue;
        qreal t = ((docPos.x() - line.p1().x()) * v.x()
                   + (docPos.y() - line.p1().y()) * v.y()) / len2;
        t = std::clamp(t, 0.0, 1.0);
        const QPointF proj = line.p1() + t * v;
        if (QLineF(proj, docPos).length() <= radius)
            return static_cast<int>(i);
    }
    return -1;
}

void VectorNodeTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (!m_layer || !ctx.document || event->button() != Qt::LeftButton)
        return;

    // Zoom entra no raio de acesso: em zoom baixo os nós ficam mais perto.
    const double radius = std::max(kHitDistance, kHitDistance / std::max(0.05, ctx.zoom));

    Drag which = Drag::None;
    const int handle = handleAt(docPos, radius, which);
    if (handle >= 0) {
        m_drag = which;
        m_dragIndex = handle;
    } else {
        const int node = nodeAt(docPos, radius);
        if (node >= 0) {
            m_drag = Drag::Node;
            m_dragIndex = node;
        } else {
            // Clicar no caminho insere um nó no ponto mais próximo.
            const int seg = segmentAt(docPos, radius);
            if (seg >= 0 && m_layer->nodes.size() < 256) {
                auto nodes = m_layer->nodes;
                VectorNode inserted;
                inserted.position = docPos;
                // Handle do novo nó: deriva dos vizinhos para não quebrar
                // a curva visualmente.
                const VectorNode& a = nodes[static_cast<size_t>(seg)];
                const VectorNode& b = nodes[static_cast<size_t>(seg) + 1];
                const QPointF tangent = (b.position - a.position) / 3.0;
                inserted.handleIn = -tangent;
                inserted.handleOut = tangent;
                nodes.insert(nodes.begin() + seg + 1, inserted);
                const bool changed = ctx.document->setVectorLayerNodes(
                    ctx.selectedLayerId, nodes);
                if (changed) {
                    m_layer = static_cast<VectorLayer*>(
                        ctx.document->findLayer(ctx.selectedLayerId));
                    m_startNodes = nodes;
                    m_drag = Drag::Node;
                    m_dragIndex = seg + 1;
                    rebuildPath();
                }
            }
        }
    }
    if (m_drag != Drag::None)
        m_startNodes = m_layer->nodes;
    m_altPressed = (event->modifiers() & Qt::AltModifier) != 0;
}

void VectorNodeTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (!m_layer || !ctx.document || m_drag == Drag::None || m_dragIndex < 0
        || m_dragIndex >= static_cast<int>(m_layer->nodes.size()))
        return;

    auto nodes = m_layer->nodes;
    VectorNode& n = nodes[static_cast<size_t>(m_dragIndex)];

    switch (m_drag) {
    case Drag::Node:
        n.position = docPos;
        break;
    case Drag::HandleIn:
        n.handleIn = docPos - n.position;
        if (n.smooth)
            n.handleOut = -n.handleIn;
        break;
    case Drag::HandleOut:
        n.handleOut = docPos - n.position;
        if (n.smooth)
            n.handleIn = -n.handleOut;
        break;
    case Drag::None:
        return;
    }

    const bool changed = ctx.document->setVectorLayerNodes(ctx.selectedLayerId, nodes);
    if (changed) {
        m_layer = static_cast<VectorLayer*>(ctx.document->findLayer(ctx.selectedLayerId));
        rebuildPath();
    }
    m_altPressed = (event->modifiers() & Qt::AltModifier) != 0;
}

void VectorNodeTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(docPos)
    Q_UNUSED(event)
    if (!m_layer || !ctx.document || !ctx.history)
        return;

    // Alt ao largar quebra a suavidade do nó (corner point).
    if (m_altPressed && m_dragIndex >= 0
        && m_dragIndex < static_cast<int>(m_layer->nodes.size())) {
        auto nodes = m_layer->nodes;
        nodes[static_cast<size_t>(m_dragIndex)].smooth = false;
        if (ctx.document->setVectorLayerNodes(ctx.selectedLayerId, nodes))
            m_layer = static_cast<VectorLayer*>(ctx.document->findLayer(ctx.selectedLayerId));
        rebuildPath();
    }

    // Um passo de histórico por gesto: do estado antes do gesto ao actual.
    if (m_drag != Drag::None && m_layer && m_layer->nodes != m_startNodes) {
        ctx.history->execute(std::make_unique<SetVectorNodesCommand>(
            *ctx.document, ctx.selectedLayerId, QStringLiteral("vector.nodes"),
            &Document::setVectorLayerNodes, m_startNodes, m_layer->nodes));
    }

    m_drag = Drag::None;
    m_dragIndex = -1;
    m_altPressed = false;
}

void VectorNodeTool::mouseDoubleClick(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (!m_layer || !ctx.document || event->button() != Qt::LeftButton)
        return;
    const double radius = std::max(kHitDistance, kHitDistance / std::max(0.05, ctx.zoom));
    const int node = nodeAt(docPos, radius);
    if (node < 0 || m_layer->nodes.size() <= 2)
        return;   // manter pelo menos 2 nós

    const auto startNodes = m_layer->nodes;
    auto nodes = m_layer->nodes;
    nodes.erase(nodes.begin() + node);
    const bool changed = ctx.document->setVectorLayerNodes(ctx.selectedLayerId, nodes);
    if (!changed)
        return;
    m_layer = static_cast<VectorLayer*>(ctx.document->findLayer(ctx.selectedLayerId));
    rebuildPath();
    if (ctx.history) {
        ctx.history->execute(std::make_unique<SetVectorNodesCommand>(
            *ctx.document, ctx.selectedLayerId, QStringLiteral("vector.nodes"),
            &Document::setVectorLayerNodes, startNodes, nodes));
    }
}

void VectorNodeTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!painter || !m_layer || !ctx.document || m_layer->nodes.empty())
        return;

    painter->save();

    // Caminho (mapeado para coordenadas de dispositivo).
    if (!m_path.isEmpty()) {
        QPainterPath device = ctx.docToDevice.map(m_path);
        painter->setPen(QPen(QColor(0x43, 0x78, 0xff), 1.5));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(device);
    }

    // Handles: linhas do nó até aos pontos de controlo.
    painter->setPen(QPen(QColor(0xf5, 0x9e, 0x0b), 1));
    for (const VectorNode& n : m_layer->nodes) {
        if (!n.handleIn.isNull()) {
            const QPointF a = ctx.docToDevice.map(n.position);
            const QPointF b = ctx.docToDevice.map(n.position + n.handleIn);
            painter->drawLine(a, b);
        }
        if (!n.handleOut.isNull()) {
            const QPointF a = ctx.docToDevice.map(n.position);
            const QPointF b = ctx.docToDevice.map(n.position + n.handleOut);
            painter->drawLine(a, b);
        }
    }

    // Nós: quadrados; handles: círculos pequenos.
    for (const VectorNode& n : m_layer->nodes) {
        const QPointF p = ctx.docToDevice.map(n.position);
        painter->setPen(QPen(Qt::white, 1));
        painter->setBrush(n.smooth ? QColor(0x43, 0x78, 0xff) : QColor(0xec, 0x48, 0x99));
        painter->drawRect(QRectF(p.x() - 4, p.y() - 4, 8, 8));
        for (const QPointF& h : {n.position + n.handleIn, n.position + n.handleOut}) {
            if ((h - n.position).isNull())
                continue;
            const QPointF d = ctx.docToDevice.map(h);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0xf5, 0x9e, 0x0b));
            painter->drawEllipse(d, 3, 3);
        }
    }

    painter->restore();
}

} // namespace cc
