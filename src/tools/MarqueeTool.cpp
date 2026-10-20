#include "MarqueeTool.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include "core/image/ImageProcessing.h"
#include "localization/i18nservice.h"

namespace cc {

void MarqueeTool::activate(const ToolContext& ctx)
{
    m_dragging = false;
    if (ctx.requestStatusMessage) {
        const QString msg = ctx.i18n
            ? ctx.i18n->t("editor", "canvas.status.marqueeHelp")
            : QStringLiteral("Marquee: drag to select. Shift adds to the selection, Alt subtracts. Esc clears.");
        ctx.requestStatusMessage(msg);
    }
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void MarqueeTool::deactivate(const ToolContext& ctx)
{
    cancel(ctx);
}

void MarqueeTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (event->button() != Qt::LeftButton || !ctx.document)
        return;

    m_dragging = true;
    m_adding = event->modifiers().testFlag(Qt::ShiftModifier);
    m_subtracting = event->modifiers().testFlag(Qt::AltModifier);
    m_startDoc = docPos;
    m_endDoc = docPos;
    if (ctx.requestUpdate)
        ctx.requestUpdate();
    event->accept();
}

void MarqueeTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    if (!m_dragging)
        return;
    m_endDoc = docPos;
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void MarqueeTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    if (!m_dragging || !ctx.document)
        return;

    m_dragging = false;
    m_endDoc = docPos;

    QRectF rect(m_startDoc, m_endDoc);
    rect = rect.normalized();
    if (rect.width() < 2.0 || rect.height() < 2.0) {
        if (ctx.requestUpdate)
            ctx.requestUpdate();
        return;
    }

    // Convenção do app: a seleção de pixel vive no espaço do bitmap da
    // camada ativa (o mesmo da varinha/tesoura); sem camada de imagem,
    // cai para o espaço do documento.
    Layer* selected = ctx.selectedLayerId.isNull()
                          ? nullptr
                          : ctx.document->findLayer(ctx.selectedLayerId);

    QImage selection;
    if (selected && selected->type() == LayerType::Image) {
        auto* img = static_cast<ImageLayer*>(selected);
        const QRectF bounds = img->contentBounds();
        const QImage bitmap = ctx.document->assets().decodedImage(img->assetId);
        if (bitmap.isNull() || bounds.isEmpty())
            return;
        const QTransform inverse =
            img->transform.matrix(bounds).inverted();
        const QRectF local = inverse.mapRect(rect);
        selection = ImageProcessing::makeShapeSelection(
            bitmap.size(), local, m_ellipse, m_feather);
    } else {
        selection = ImageProcessing::makeShapeSelection(
            QSize(ctx.document->width(), ctx.document->height()), rect,
            m_ellipse, m_feather);
    }

    if (selection.isNull()) {
        if (ctx.requestUpdate)
            ctx.requestUpdate();
        return;
    }

    const QImage current = ctx.document->pixelSelection();
    if ((m_adding || m_subtracting) && !current.isNull()
        && current.size() == selection.size()) {
        selection = ImageProcessing::combineSelections(current, selection, m_subtracting);
    }

    ctx.document->setPixelSelection(selection);

    if (ctx.requestStatusMessage) {
        const QString msg = ctx.i18n
            ? ctx.i18n->t("editor", "canvas.status.marqueeDone")
            : QStringLiteral("Selection created.");
        ctx.requestStatusMessage(msg);
    }
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void MarqueeTool::keyPress(QKeyEvent* event, const ToolContext& ctx)
{
    if (event->key() == Qt::Key_Escape) {
        // Esc limpa a seleção ativa (padrão Photoshop).
        if (ctx.document && !ctx.document->pixelSelection().isNull()) {
            ctx.document->clearPixelSelection();
            if (ctx.requestStatusMessage) {
                const QString msg = ctx.i18n
                    ? ctx.i18n->t("editor", "canvas.status.selectionCleared")
                    : QStringLiteral("Selection cleared.");
                ctx.requestStatusMessage(msg);
            }
            if (ctx.requestUpdate)
                ctx.requestUpdate();
        }
        event->accept();
    }
}

void MarqueeTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!m_dragging)
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF rect = QRectF(m_startDoc, m_endDoc);
    const QRectF normalized = rect.normalized();
    QPolygonF poly = ctx.docToDevice.map(normalized);
    const QRectF deviceRect = poly.boundingRect();

    QPen pen(QColor(255, 255, 255, 220), 1.0, Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    if (m_ellipse)
        painter->drawEllipse(deviceRect);
    else
        painter->drawRect(deviceRect);

    painter->setPen(QPen(QColor(0, 0, 0, 160), 1.0, Qt::DotLine));
    const QRectF inner = deviceRect.adjusted(1, 1, -1, -1);
    if (m_ellipse)
        painter->drawEllipse(inner);
    else
        painter->drawRect(inner);

    painter->restore();
}

void MarqueeTool::cancel(const ToolContext& ctx)
{
    m_dragging = false;
    m_startDoc = QPointF();
    m_endDoc = QPointF();
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

} // namespace cc
