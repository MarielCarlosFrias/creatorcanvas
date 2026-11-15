#include "VectorNodeTool.h"

#include "core/Document.h"
#include "localization/i18nservice.h"
#include "core/commands/LayerPropertyCommands.h"
#include "core/history/CommandStack.h"
#include "core/layers/Layer.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace cc {

/// Comando de propriedade: os nós são passados por valor, como o setter exige.
using SetVectorNodesCommand = LayerPropertyCommand<std::vector<VectorNode>>;

namespace {

/// Matriz que leva um ponto LOCAL do caminho (centrado no contentBounds) ao
/// dispositivo — é a mesma que o renderer aplica.
QTransform nodeMatrix(const ToolContext& ctx, const VectorLayer* layer)
{
    return ctx.docToDevice * layer->transform.matrix(layer->contentBounds());
}

/// Inverso: ponto do documento -> espaço local do caminho.
QPointF toLocal(const VectorLayer* layer, const QPointF& docPos)
{
    return layer->transform.matrix(layer->contentBounds()).inverted().map(docPos);
}

/// Id do último VectorLayer da árvore (o mais recentemente criado costuma
/// ser o que o utilizador quer editar). LayerId nulo se não existir nenhum.
LayerId lastVectorLayerId(const Document& doc)
{
    const GroupLayer* root = doc.rootGroup();
    if (!root)
        return {};
    std::vector<const GroupLayer*> stack{root};
    LayerId last;
    int guard = 0;
    while (!stack.empty() && guard++ < 4096) {
        const GroupLayer* group = stack.back();
        stack.pop_back();
        for (const auto& child : group->children) {
            if (!child)
                continue;
            if (child->type() == LayerType::Vector)
                last = child->id();
            if (child->type() == LayerType::Group)
                stack.push_back(static_cast<const GroupLayer*>(child.get()));
        }
    }
    return last;
}

/// Camada vectorial selecionada agora, ou nullptr. Resolvida a cada evento —
/// a selecção pode mudar enquanto a ferramenta está activa.
VectorLayer* selectedVector(const ToolContext& ctx)
{
    if (!ctx.document || ctx.selectedLayerId.isNull())
        return nullptr;
    Layer* layer = ctx.document->findLayer(ctx.selectedLayerId);
    if (!layer || layer->type() != LayerType::Vector)
        return nullptr;
    return static_cast<VectorLayer*>(layer);
}

} // namespace

void VectorNodeTool::activate(const ToolContext& ctx)
{
    m_layer = selectedVector(ctx);
    m_drag = Drag::None;
    m_dragIndex = -1;

    // Se a camada selecionada não é um caminho, escolhemos o ÚLTIMO caminho
    // do documento: pedir ao utilizador para "seleccionar uma camada de
    // caminho" deixava a ferramenta inutilizável (não havia forma de saber
    // qual era).
    if (!m_layer && ctx.document && ctx.selectLayer) {
        const LayerId last = lastVectorLayerId(*ctx.document);
        if (!last.isNull()) {
            ctx.selectLayer(last);
            m_layer = selectedVector(ctx);
        }
    }

    const QString hint = ctx.i18n
        ? ctx.i18n->t("editor", "canvas.status.nodeEditHint")
        : QStringLiteral("Nós: arraste para mover, Alt+largar quebra a suavidade, "
                         "duplo-clique apaga, clique no caminho insere.");
    const QString noPath = ctx.i18n
        ? ctx.i18n->t("editor", "canvas.status.nodeEditNoPath")
        : QStringLiteral("Sem caminho para editar — desenhe um com a caneta (P).");
    if (m_layer && m_layer->nodes.size() >= 2) {
        rebuildPath();
        if (ctx.requestStatusMessage)
            ctx.requestStatusMessage(hint);
    } else if (ctx.requestStatusMessage) {
        ctx.requestStatusMessage(noPath);
    }
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

int VectorNodeTool::nodeAt(const QPointF& localPos, double radius) const
{
    if (!m_layer)
        return -1;
    for (size_t i = 0; i < m_layer->nodes.size(); ++i) {
        if (QLineF(m_layer->nodes[i].position, localPos).length() <= radius)
            return static_cast<int>(i);
    }
    return -1;
}

int VectorNodeTool::handleAt(const QPointF& localPos, double radius, Drag& which) const
{
    if (!m_layer)
        return -1;
    for (size_t i = 0; i < m_layer->nodes.size(); ++i) {
        const VectorNode& n = m_layer->nodes[i];
        // Handle de comprimento zero coincide com o nó: ignorá-lo, senão
        // rouba o arraste do nó (foi um bug real apanhado pelos testes).
        if (!n.handleIn.isNull()
            && QLineF(n.position + n.handleIn, localPos).length() <= radius) {
            which = Drag::HandleIn;
            return static_cast<int>(i);
        }
        if (!n.handleOut.isNull()
            && QLineF(n.position + n.handleOut, localPos).length() <= radius) {
            which = Drag::HandleOut;
            return static_cast<int>(i);
        }
    }
    which = Drag::None;
    return -1;
}

int VectorNodeTool::segmentAt(const QPointF& localPos, double radius) const
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
        qreal t = ((localPos.x() - line.p1().x()) * v.x()
                   + (localPos.y() - line.p1().y()) * v.y()) / len2;
        t = std::clamp(t, 0.0, 1.0);
        const QPointF proj = line.p1() + t * v;
        if (QLineF(proj, localPos).length() <= radius)
            return static_cast<int>(i);
    }
    return -1;
}

void VectorNodeTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    m_layer = selectedVector(ctx);
    if (!m_layer || !ctx.document || event->button() != Qt::LeftButton)
        return;

    // Zoom entra no raio de acesso: em zoom baixo os nós ficam mais perto.
    const double radius = std::max(kHitDistance, kHitDistance / std::max(0.05, ctx.zoom));
    const QPointF localPos = toLocal(m_layer, docPos);

    Drag which = Drag::None;
    // Handles e nós vivem no mesmo espaço LOCAL — testar o handle em
    // docPos (como estava) fazia-os nunca ser atingidos quando a camada
    // tem posição diferente da origem.
    const int handle = handleAt(localPos, radius, which);
    if (handle >= 0) {
        m_drag = which;
        m_dragIndex = handle;
        m_selectedNode = handle;
    } else {
        const int node = nodeAt(localPos, radius);
        if (node >= 0) {
            m_drag = Drag::Node;
            m_dragIndex = node;
            m_selectedNode = node;
        } else {
            // Clicar no caminho insere um nó no ponto mais próximo.
            const int seg = segmentAt(localPos, radius);
            if (seg >= 0 && m_layer->nodes.size() < 256) {
                auto nodes = m_layer->nodes;
                VectorNode inserted;
                inserted.position = localPos;
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

    const QPointF localPos = toLocal(m_layer, docPos);
    auto nodes = m_layer->nodes;
    VectorNode& n = nodes[static_cast<size_t>(m_dragIndex)];

    switch (m_drag) {
    case Drag::Node:
        n.position = localPos;
        break;
    case Drag::HandleIn:
        n.handleIn = localPos - n.position;
        if (n.smooth)
            n.handleOut = -n.handleIn;
        break;
    case Drag::HandleOut:
        n.handleOut = localPos - n.position;
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
    if (!ctx.document || !ctx.history)
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
    m_layer = selectedVector(ctx);
    if (!m_layer || !ctx.document || event->button() != Qt::LeftButton)
        return;
    const double radius = std::max(kHitDistance, kHitDistance / std::max(0.05, ctx.zoom));
    const int node = nodeAt(toLocal(m_layer, docPos), radius);
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


void VectorNodeTool::keyPress(QKeyEvent* event, const ToolContext& ctx)
{
    if (!event || !ctx.document || !m_layer)
        return;

    switch (event->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        // Sem posição do cursor no keyPress: actua sobre o nó seleccionado
        // (o último arrastado/clicado).
        deleteSelectedNode(ctx);
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        toggleClosed(ctx);
        event->accept();
        return;
    case Qt::Key_Escape:
        // Cancela um arraste a meio sem cometer nada.
        m_drag = Drag::None;
        m_dragIndex = -1;
        if (m_layer && m_layer->nodes.size() >= 2) {
            rebuildPath();
            if (ctx.requestUpdate)
                ctx.requestUpdate();
        }
        event->accept();
        return;
    default:
        break;
    }
}

void VectorNodeTool::deleteSelectedNode(const ToolContext& ctx)
{
    m_layer = selectedVector(ctx);
    if (!m_layer || !ctx.document || m_layer->nodes.size() <= 2)
        return;

    int node = m_selectedNode;   // o nó seleccionado (último clicado/arrastado)
    if (node < 0 || node >= static_cast<int>(m_layer->nodes.size()))
        return;

    const auto startNodes = m_layer->nodes;
    auto nodes = m_layer->nodes;
    nodes.erase(nodes.begin() + node);
    if (!ctx.document->setVectorLayerNodes(ctx.selectedLayerId, nodes))
        return;
    m_layer = static_cast<VectorLayer*>(ctx.document->findLayer(ctx.selectedLayerId));
    rebuildPath();
    m_dragIndex = -1;
    m_selectedNode = -1;
    if (ctx.history) {
        ctx.history->execute(std::make_unique<SetVectorNodesCommand>(
            *ctx.document, ctx.selectedLayerId, QStringLiteral("vector.nodes"),
            &Document::setVectorLayerNodes, startNodes, nodes));
    }
    if (ctx.requestUpdate)
        ctx.requestUpdate();
    if (ctx.requestStatusMessage)
        ctx.requestStatusMessage(QStringLiteral("Nó apagado."));
}

void VectorNodeTool::toggleClosed(const ToolContext& ctx)
{
    m_layer = selectedVector(ctx);
    if (!m_layer || !ctx.document || m_layer->nodes.size() < 2)
        return;

    const bool wasClosed = m_layer->closed;
    m_layer->closed = !wasClosed;   // campo simples; revertido pelo comando
    rebuildPath();

    if (ctx.history) {
        // Um passo de histórico que alterna o estado (o Document não tem
        // setter dedicado, por isso o comando mexe no campo directamente).
        struct ToggleClosed final : Command {
            ToggleClosed(Document& d, const LayerId& id, bool oldClosed)
                : Command(QStringLiteral("vector.closePath")), m_doc(&d), m_id(id),
                  m_old(oldClosed) {}
            void redo() override
            {
                if (Layer* l = m_doc->findLayer(m_id))
                    if (l->type() == LayerType::Vector)
                        static_cast<VectorLayer*>(l)->closed = !m_old;
            }
            void undo() override
            {
                if (Layer* l = m_doc->findLayer(m_id))
                    if (l->type() == LayerType::Vector)
                        static_cast<VectorLayer*>(l)->closed = m_old;
            }
            Document* m_doc;
            LayerId m_id;
            bool m_old;
        };
        ctx.history->execute(std::make_unique<ToggleClosed>(
            *ctx.document, ctx.selectedLayerId, wasClosed));
    }
    if (ctx.requestUpdate)
        ctx.requestUpdate();
    if (ctx.requestStatusMessage)
        ctx.requestStatusMessage(wasClosed ? QStringLiteral("Caminho aberto.")
                                           : QStringLiteral("Caminho fechado."));
}

void VectorNodeTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!painter || !ctx.document)
        return;
    m_layer = selectedVector(ctx);
    if (!m_layer || m_layer->nodes.empty())
        return;

    painter->save();

    // Caminho (mapeado para coordenadas de dispositivo pela mesma matriz
    // que o renderer aplica).
    if (!m_path.isEmpty()) {
        QPainterPath device = nodeMatrix(ctx, m_layer).map(m_path);
        painter->setPen(QPen(QColor(0x43, 0x78, 0xff), 1.5));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(device);
    }

    // Handles: linhas do nó até aos pontos de controlo.
    painter->setPen(QPen(QColor(0xf5, 0x9e, 0x0b), 1));
    for (const VectorNode& n : m_layer->nodes) {
        if (!n.handleIn.isNull()) {
            painter->drawLine(nodeMatrix(ctx, m_layer).map(n.position),
                              nodeMatrix(ctx, m_layer).map(n.position + n.handleIn));
        }
        if (!n.handleOut.isNull()) {
            painter->drawLine(nodeMatrix(ctx, m_layer).map(n.position),
                              nodeMatrix(ctx, m_layer).map(n.position + n.handleOut));
        }
    }

    // Nós: quadrados; handles: círculos pequenos.
    for (const VectorNode& n : m_layer->nodes) {
        const QPointF p = nodeMatrix(ctx, m_layer).map(n.position);
        painter->setPen(QPen(Qt::white, 1));
        painter->setBrush(n.smooth ? QColor(0x43, 0x78, 0xff) : QColor(0xec, 0x48, 0x99));
        painter->drawRect(QRectF(p.x() - 4, p.y() - 4, 8, 8));
        for (const QPointF& h : {n.position + n.handleIn, n.position + n.handleOut}) {
            if ((h - n.position).isNull())
                continue;
            const QPointF d = nodeMatrix(ctx, m_layer).map(h);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0xf5, 0x9e, 0x0b));
            painter->drawEllipse(d, 3, 3);
        }
    }

    painter->restore();
}

} // namespace cc
