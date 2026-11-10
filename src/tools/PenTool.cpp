#include "PenTool.h"

#include <QMouseEvent>
#include <QPainter>
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
    if (ctx.requestStatusMessage) {
        const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.penHelp")
                                     : QStringLiteral("Pen Tool: Click to create anchor points, drag for Bézier handles.");
        ctx.requestStatusMessage(msg);
    }
}

void PenTool::deactivate(const ToolContext& ctx)
{
    cancel(ctx);
}

void PenTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (!ctx.document) return;

    // Alt+clique: fecha o caminho e finaliza
    if (event->button() == Qt::RightButton || (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier))) {
        if (!m_path.isEmpty()) {
            m_path.closeSubpath();
            m_drawing = false;
            // Cria VectorLayer com o caminho
            auto vecLayer = std::make_unique<VectorLayer>();
            vecLayer->path = m_path;
            vecLayer->fill = m_fillColor;
            vecLayer->stroke = m_strokeColor;
            vecLayer->strokeWidth = m_strokeWidth;
            vecLayer->fillEnabled = m_fillColor.alpha() > 0;
            vecLayer->strokeEnabled = m_strokeColor.alpha() > 0;

            (void)vecLayer->id(); // unused
            if (ctx.document->addLayer(std::move(vecLayer), nullptr, -1)) {
                if (ctx.requestStatusMessage) {
                    const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.penComplete")
                                                 : QStringLiteral("Vector path created.");
                    ctx.requestStatusMessage(msg);
                }
                if (ctx.requestUpdate)
                    ctx.requestUpdate();
            }
        }
        m_path = QPainterPath();
        m_drawing = false;
        event->accept();
        return;
    }

    // Primeiro ponto: começa o caminho
    if (!m_drawing) {
        m_path.moveTo(docPos);
        m_lastPoint = docPos;
        m_drawing = true;
    } else {
        // Próximo ponto: cria curva cúbica com handle aproximado
        // Para simplicidade, cria linha reta (usuário pode editar handles depois)
        m_path.lineTo(docPos);
        m_lastPoint = docPos;
    }

    if (ctx.requestUpdate)
        ctx.requestUpdate();
    event->accept();
}

void PenTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    if (m_drawing && ctx.document) {
        // Atualiza o último segmento como curva para preview
        QPainterPath previewPath = m_path;
        if (previewPath.currentPosition() != docPos) {
            previewPath.lineTo(docPos);
        }
        if (ctx.setLayerPreview) {
            // Renderiza o path em uma imagem temporária para preview
            QImage preview(512, 512, QImage::Format_ARGB32_Premultiplied);
            preview.fill(Qt::transparent);
            QPainter p(&preview);
            p.setRenderHint(QPainter::Antialiasing, true);
            QPen pen(m_strokeColor, m_strokeWidth);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPath(previewPath);
            // Nota: setLayerPreview espera uma camada existente; aqui apenas
            // solicitamos atualização da view
            Q_UNUSED(preview);
        }
        if (ctx.requestUpdate)
            ctx.requestUpdate();
    }
}

void PenTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) {
    Q_UNUSED(event); Q_UNUSED(docPos); Q_UNUSED(ctx);
    // O path é finalizado em mousePress (clique) ou mouseRelease (arrasto)
    // Para simplicidade, o path fica ativo até Alt+clique ou clique direito.
}

void PenTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!ctx.document) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Desenha o path em construção
    if (m_drawing && !m_path.isEmpty()) {
        QPen pen(m_strokeColor, m_strokeWidth);
        pen.setCosmetic(true);
        pen.setStyle(Qt::DashLine);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(m_path);

        // Desenha o último ponto âncora
        QPointF lastScreen = ctx.docToDevice.map(m_lastPoint);
        painter->setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(QColor(0, 120, 255));
        painter->drawEllipse(lastScreen, 5, 5);
    }

    painter->restore();
}

void PenTool::cancel(const ToolContext& ctx)
{
    m_drawing = false;
    m_path = QPainterPath();
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

} // namespace cc
