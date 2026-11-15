#include "PenTool.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include "core/layers/Layer.h"
#include "core/history/DocumentCommands.h"
#include "core/history/CommandStack.h"
#include "ToolCommit.h"
#include "localization/i18nservice.h"

namespace cc {

// ============================================================================
// --- Pen Tool (curvas Bézier cúbicas) ---
// ============================================================================

void PenTool::syncAnchors()
{
    m_anchors.clear();
    m_anchors.reserve(m_nodes.size());
    for (const auto& n : m_nodes)
        m_anchors.push_back(n.pos);
}

void PenTool::rebuildCurrentPath()
{
    m_path = QPainterPath();
    if (m_nodes.empty())
        return;

    m_path.moveTo(m_nodes.front().pos);
    for (size_t i = 0; i + 1 < m_nodes.size(); ++i) {
        const auto& curr = m_nodes[i];
        const auto& next = m_nodes[i + 1];
        m_path.cubicTo(curr.pos + curr.handleOut,
                       next.pos + next.handleIn,
                       next.pos);
    }
    if (m_closed && m_nodes.size() > 2) {
        const auto& last = m_nodes.back();
        const auto& first = m_nodes.front();
        m_path.cubicTo(last.pos + last.handleOut,
                       first.pos + first.handleIn,
                       first.pos);
        m_path.closeSubpath();
    }
}

void PenTool::activate(const ToolContext& ctx)
{
    cancel(ctx);
    if (ctx.requestStatusMessage) {
        const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.penHelp")
                                     : QStringLiteral("Caneta: Clique para nós retos, clique e arraste para curvas Bézier. Enter/Duplo-clique para concluir, Esc para cancelar.");
        ctx.requestStatusMessage(msg);
    }
}

void PenTool::deactivate(const ToolContext& ctx)
{
    cancel(ctx);
}

void PenTool::finishPath(const ToolContext& ctx)
{
    if (m_nodes.empty() || !ctx.document) {
        cancel(ctx);
        return;
    }

    m_closed = true;
    rebuildCurrentPath();
    m_drawing = false;
    m_isDraggingHandle = false;

    // Cria VectorLayer com o caminho E os nós correspondentes com suas alças Bézier
    auto vecLayer = std::make_unique<VectorLayer>();
    vecLayer->path = m_path;
    vecLayer->fill = m_fillColor;
    vecLayer->stroke = m_strokeColor;
    vecLayer->strokeWidth = m_strokeWidth;
    vecLayer->fillEnabled = m_fillColor.alpha() > 0;
    vecLayer->strokeEnabled = m_strokeColor.alpha() > 0;
    vecLayer->closed = m_closed;

    const LayerId newId = vecLayer->id();
    qreal minX = m_nodes.front().pos.x(), maxX = minX;
    qreal minY = m_nodes.front().pos.y(), maxY = minY;
    for (const auto& n : m_nodes) {
        minX = std::min(minX, n.pos.x());
        maxX = std::max(maxX, n.pos.x());
        minY = std::min(minY, n.pos.y());
        maxY = std::max(maxY, n.pos.y());
    }
    const QPointF center((minX + maxX) / 2.0, (minY + maxY) / 2.0);

    for (const auto& n : m_nodes) {
        VectorNode node;
        node.position = n.pos - center; // espaço local centrado
        node.handleIn = n.handleIn;     // vetor relativo
        node.handleOut = n.handleOut;   // vetor relativo
        node.smooth = n.smooth;
        vecLayer->nodes.push_back(node);
    }
    vecLayer->transform.position = center;
    vecLayer->path.translate(-center);
    vecLayer->rebuildPath();

    if (ctx.history) {
        ctx.history->execute(std::make_unique<AddLayerCommand>(*ctx.document, std::move(vecLayer)));
    } else {
        ctx.document->addLayer(std::move(vecLayer), nullptr, -1);
    }

    // Seleção imediata para permitir edição com nós ou ferramentas de seleção
    if (ctx.selectLayer)
        ctx.selectLayer(newId);
    if (ctx.requestStatusMessage) {
        const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.penComplete")
                                     : QStringLiteral("Caminho vetorial criado com sucesso.");
        ctx.requestStatusMessage(msg);
    }

    m_path = QPainterPath();
    m_nodes.clear();
    syncAnchors();
    m_drawing = false;
    m_isDraggingHandle = false;
    m_closed = false;
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void PenTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (!ctx.document) return;

    // Botão direito, Ctrl+clique ou Alt+clique: fecha o caminho e finaliza
    if (event->button() == Qt::RightButton || (event->button() == Qt::LeftButton && (event->modifiers() & (Qt::AltModifier | Qt::ControlModifier)))) {
        if (!m_nodes.empty()) {
            finishPath(ctx);
        }
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton)
        return;

    // Se clicar próximo ao primeiro nó (fechamento automático do caminho)
    if (m_drawing && m_nodes.size() >= 3) {
        const QPointF firstScreen = ctx.docToDevice.map(m_nodes.front().pos);
        const QPointF clickScreen = ctx.docToDevice.map(docPos);
        if (QLineF(firstScreen, clickScreen).length() <= 12.0) {
            finishPath(ctx);
            event->accept();
            return;
        }
    }

    // Adiciona novo nó
    NodeData node;
    node.pos = docPos;
    node.handleIn = QPointF(0, 0);
    node.handleOut = QPointF(0, 0);
    node.smooth = false;

    m_nodes.push_back(node);
    syncAnchors();
    m_lastPoint = docPos;
    m_currentHover = docPos;
    m_drawing = true;
    m_isDraggingHandle = true; // Inicia arrasto potencial de alça Bézier

    rebuildCurrentPath();

    if (ctx.requestUpdate)
        ctx.requestUpdate();
    event->accept();
}

void PenTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    m_currentHover = docPos;

    if (m_drawing && m_isDraggingHandle && !m_nodes.empty() && (event->buttons() & Qt::LeftButton)) {
        // O usuário está arrastando a alça do nó recém-adicionado
        const QPointF delta = docPos - m_nodes.back().pos;
        if (delta.manhattanLength() > 1.5) {
            m_nodes.back().handleOut = delta;
            m_nodes.back().handleIn = -delta;
            m_nodes.back().smooth = true;
            rebuildCurrentPath();
        }
    }

    if (m_drawing && ctx.document && ctx.requestUpdate) {
        ctx.requestUpdate();
    }
}

void PenTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(docPos);
    if (event->button() == Qt::LeftButton && m_isDraggingHandle) {
        m_isDraggingHandle = false;
        if (!m_nodes.empty()) {
            // Se o arrasto foi imperceptível (< 2.0px no doc), deixa reto
            if (m_nodes.back().handleOut.manhattanLength() < 2.0) {
                m_nodes.back().handleIn = QPointF(0, 0);
                m_nodes.back().handleOut = QPointF(0, 0);
                m_nodes.back().smooth = false;
            }
            rebuildCurrentPath();
        }
        if (ctx.requestUpdate)
            ctx.requestUpdate();
    }
}

void PenTool::keyPress(QKeyEvent* event, const ToolContext& ctx)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_drawing && !m_nodes.empty()) {
            finishPath(ctx);
            event->accept();
            return;
        }
    } else if (event->key() == Qt::Key_Escape) {
        cancel(ctx);
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
        if (m_drawing && m_nodes.size() > 1) {
            m_nodes.pop_back();
            syncAnchors();
            rebuildCurrentPath();
            m_lastPoint = m_nodes.back().pos;
            if (ctx.requestUpdate)
                ctx.requestUpdate();
            event->accept();
            return;
        } else if (m_drawing && m_nodes.size() == 1) {
            cancel(ctx);
            event->accept();
            return;
        }
    }
}

void PenTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!ctx.document || !m_drawing || m_nodes.empty()) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Desenha o caminho construído até o momento em coordenadas de tela
    if (!m_path.isEmpty()) {
        const QPainterPath screenPath = ctx.docToDevice.map(m_path);
        if (m_fillColor.alpha() > 0) {
            QColor fillOverlay = m_fillColor;
            fillOverlay.setAlpha(std::min(fillOverlay.alpha(), 60));
            painter->fillPath(screenPath, fillOverlay);
        }
        QPen pen(m_strokeColor, std::max(1.5, m_strokeWidth * ctx.zoom));
        pen.setStyle(Qt::SolidLine);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(screenPath);
    }

    // Linha guia / curva elástica até o cursor do mouse se não estiver puxando alça
    if (!m_isDraggingHandle) {
        const auto& lastNode = m_nodes.back();
        const QPointF lastScreen = ctx.docToDevice.map(lastNode.pos);
        const QPointF hoverScreen = ctx.docToDevice.map(m_currentHover);

        QPainterPath rubberband;
        rubberband.moveTo(lastScreen);
        if (lastNode.handleOut != QPointF(0, 0)) {
            const QPointF c1Screen = ctx.docToDevice.map(lastNode.pos + lastNode.handleOut);
            rubberband.cubicTo(c1Screen, hoverScreen, hoverScreen);
        } else {
            rubberband.lineTo(hoverScreen);
        }

        painter->setPen(QPen(QColor(0, 140, 255, 200), 1.5, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(rubberband);
    }

    // Desenha alças de controle Bézier para todos os nós
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        const auto& n = m_nodes[i];
        const QPointF posScreen = ctx.docToDevice.map(n.pos);

        // Se tem alças Bézier ativas
        if (n.handleOut != QPointF(0, 0) || n.handleIn != QPointF(0, 0)) {
            if (n.handleIn != QPointF(0, 0)) {
                const QPointF inScreen = ctx.docToDevice.map(n.pos + n.handleIn);
                painter->setPen(QPen(QColor(100, 160, 255, 220), 1.0, Qt::SolidLine));
                painter->drawLine(posScreen, inScreen);
                painter->setPen(QPen(QColor(0, 100, 220), 1.2));
                painter->setBrush(Qt::white);
                painter->drawEllipse(inScreen, 3.5, 3.5);
            }

            if (n.handleOut != QPointF(0, 0)) {
                const QPointF outScreen = ctx.docToDevice.map(n.pos + n.handleOut);
                painter->setPen(QPen(QColor(100, 160, 255, 220), 1.0, Qt::SolidLine));
                painter->drawLine(posScreen, outScreen);
                painter->setPen(QPen(QColor(0, 100, 220), 1.2));
                painter->setBrush(Qt::white);
                painter->drawEllipse(outScreen, 3.5, 3.5);
            }
        }

        // Ponto âncora
        if (i == 0) {
            // Primeiro nó: destaque verde para fechamento
            const QPointF hoverScreen = ctx.docToDevice.map(m_currentHover);
            const bool hoverClose = m_nodes.size() >= 3 && (QLineF(posScreen, hoverScreen).length() <= 12.0);
            painter->setPen(QPen(Qt::white, 2.0));
            painter->setBrush(hoverClose ? QColor(30, 230, 120) : QColor(43, 200, 100));
            painter->drawEllipse(posScreen, hoverClose ? 7.0 : 5.0, hoverClose ? 7.0 : 5.0);
            if (hoverClose) {
                painter->setPen(QPen(QColor(30, 230, 120, 160), 2.0, Qt::DashLine));
                painter->setBrush(Qt::NoBrush);
                painter->drawEllipse(posScreen, 12.0, 12.0);
            }
        } else {
            painter->setPen(QPen(Qt::white, 1.5));
            painter->setBrush(QColor(0, 120, 255));
            painter->drawRect(QRectF(posScreen.x() - 3.5, posScreen.y() - 3.5, 7.0, 7.0));
        }
    }

    painter->restore();
}

void PenTool::cancel(const ToolContext& ctx)
{
    m_drawing = false;
    m_isDraggingHandle = false;
    m_closed = false;
    m_path = QPainterPath();
    m_nodes.clear();
    syncAnchors();
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

} // namespace cc
