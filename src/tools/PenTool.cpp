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

void PenTool::activate(const ToolContext& ctx)
{
    m_drawing = false;
    m_path = QPainterPath();
    m_anchors.clear();
    if (ctx.requestStatusMessage) {
        const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.penHelp")
                                     : QStringLiteral("Pen Tool: Click to create anchor points. Press Enter, double-click or Alt+click to complete, Esc to cancel.");
        ctx.requestStatusMessage(msg);
    }
}

void PenTool::deactivate(const ToolContext& ctx)
{
    cancel(ctx);
}

void PenTool::finishPath(const ToolContext& ctx)
{
    if (m_path.isEmpty() || m_anchors.empty() || !ctx.document) {
        cancel(ctx);
        return;
    }

    m_path.closeSubpath();
    m_drawing = false;

    // Cria VectorLayer com o caminho E os nós correspondentes
    auto vecLayer = std::make_unique<VectorLayer>();
    vecLayer->path = m_path;
    vecLayer->fill = m_fillColor;
    vecLayer->stroke = m_strokeColor;
    vecLayer->strokeWidth = m_strokeWidth;
    vecLayer->fillEnabled = m_fillColor.alpha() > 0;
    vecLayer->strokeEnabled = m_strokeColor.alpha() > 0;
    vecLayer->closed = true;

    const LayerId newId = vecLayer->id();
    qreal minX = m_anchors.front().x(), maxX = minX;
    qreal minY = m_anchors.front().y(), maxY = minY;
    for (const QPointF& anchor : m_anchors) {
        minX = std::min(minX, anchor.x());
        maxX = std::max(maxX, anchor.x());
        minY = std::min(minY, anchor.y());
        maxY = std::max(maxY, anchor.y());
    }
    const QPointF center((minX + maxX) / 2.0, (minY + maxY) / 2.0);
    for (const QPointF& anchor : m_anchors) {
        VectorNode node;
        node.position = anchor - center;   // centrado na origem
        node.smooth = false;   // segmentos rectos até o utilizador os suavizar
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

    // Selecção imediata: sem isto a camada fica criada mas nenhuma ferramenta a vê
    if (ctx.selectLayer)
        ctx.selectLayer(newId);
    if (ctx.requestStatusMessage) {
        const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.penComplete")
                                     : QStringLiteral("Vector path created.");
        ctx.requestStatusMessage(msg);
    }

    m_path = QPainterPath();
    m_anchors.clear();
    m_drawing = false;
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void PenTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (!ctx.document) return;

    // Botão direito, Ctrl+clique ou Alt+clique: fecha o caminho e finaliza
    if (event->button() == Qt::RightButton || (event->button() == Qt::LeftButton && (event->modifiers() & (Qt::AltModifier | Qt::ControlModifier)))) {
        if (!m_path.isEmpty()) {
            finishPath(ctx);
        }
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton)
        return;

    // Se clicar próximo ao primeiro nó (fechamento automático do caminho)
    if (m_drawing && m_anchors.size() >= 3) {
        const QPointF firstScreen = ctx.docToDevice.map(m_anchors.front());
        const QPointF clickScreen = ctx.docToDevice.map(docPos);
        if (QLineF(firstScreen, clickScreen).length() <= 10.0) {
            finishPath(ctx);
            event->accept();
            return;
        }
    }

    // Primeiro ponto: começa o caminho
    if (!m_drawing) {
        m_anchors.clear();
        m_path.moveTo(docPos);
        m_anchors.push_back(docPos);
        m_lastPoint = docPos;
        m_currentHover = docPos;
        m_drawing = true;
    } else {
        m_path.lineTo(docPos);
        m_anchors.push_back(docPos);
        m_lastPoint = docPos;
        m_currentHover = docPos;
    }

    if (ctx.requestUpdate)
        ctx.requestUpdate();
    event->accept();
}

void PenTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    m_currentHover = docPos;
    if (m_drawing && ctx.document) {
        if (ctx.requestUpdate)
            ctx.requestUpdate();
    }
}

void PenTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) {
    Q_UNUSED(event); Q_UNUSED(docPos); Q_UNUSED(ctx);
}

void PenTool::keyPress(QKeyEvent* event, const ToolContext& ctx)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_drawing && !m_path.isEmpty()) {
            finishPath(ctx);
            event->accept();
            return;
        }
    } else if (event->key() == Qt::Key_Escape) {
        cancel(ctx);
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
        if (m_drawing && m_anchors.size() > 1) {
            m_anchors.pop_back();
            m_path = QPainterPath();
            m_path.moveTo(m_anchors.front());
            for (size_t i = 1; i < m_anchors.size(); ++i) {
                m_path.lineTo(m_anchors[i]);
            }
            m_lastPoint = m_anchors.back();
            if (ctx.requestUpdate)
                ctx.requestUpdate();
            event->accept();
            return;
        } else if (m_drawing && m_anchors.size() == 1) {
            cancel(ctx);
            event->accept();
            return;
        }
    }
}

void PenTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!ctx.document || !m_drawing || m_anchors.empty()) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Desenha o path em construção mapeado para coordenadas de tela
    if (!m_path.isEmpty()) {
        const QPainterPath screenPath = ctx.docToDevice.map(m_path);
        QPen pen(m_strokeColor, std::max(1.5, m_strokeWidth * ctx.zoom));
        pen.setStyle(Qt::SolidLine);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(screenPath);
    }

    // Linha guia elástica até o cursor
    const QPointF lastScreen = ctx.docToDevice.map(m_lastPoint);
    const QPointF hoverScreen = ctx.docToDevice.map(m_currentHover);
    painter->setPen(QPen(QColor(0, 120, 255, 180), 1.5, Qt::DashLine));
    painter->drawLine(lastScreen, hoverScreen);

    // Desenha todos os nós âncora
    for (size_t i = 0; i < m_anchors.size(); ++i) {
        const QPointF sc = ctx.docToDevice.map(m_anchors[i]);
        if (i == 0) {
            // Primeiro nó tem destaque para fechamento do laço
            painter->setPen(QPen(Qt::white, 1.5));
            painter->setBrush(QColor(43, 200, 100)); // verde
            painter->drawEllipse(sc, 5.0, 5.0);
        } else {
            painter->setPen(QPen(Qt::white, 1.5));
            painter->setBrush(QColor(0, 120, 255));
            painter->drawRect(QRectF(sc.x() - 3.5, sc.y() - 3.5, 7.0, 7.0));
        }
    }

    painter->restore();
}

void PenTool::cancel(const ToolContext& ctx)
{
    m_drawing = false;
    m_path = QPainterPath();
    m_anchors.clear();
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

} // namespace cc
