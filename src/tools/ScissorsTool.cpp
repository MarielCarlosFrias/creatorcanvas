#include "ScissorsTool.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include "core/layers/Layer.h"
#include "core/history/DocumentCommands.h"
#include "core/history/CommandStack.h"
#include "core/image/ImageProcessing.h"
#include "localization/i18nservice.h"

namespace cc {

void ScissorsTool::activate(const ToolContext& ctx)
{
    m_polygon.clear();
    m_isDrawing = false;
    if (ctx.requestStatusMessage) {
        const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.scissorsHelp")
                                     : QStringLiteral("Scissors: Click to add polygon points. Press Enter or double-click to cut, Esc to cancel.");
        ctx.requestStatusMessage(msg);
    }
}

void ScissorsTool::deactivate(const ToolContext& ctx)
{
    m_polygon.clear();
    m_isDrawing = false;
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void ScissorsTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (event->button() != Qt::LeftButton || !ctx.document)
        return;

    Layer* layer = nullptr;
    if (!ctx.selectedLayerId.isNull())
        layer = ctx.document->findLayer(ctx.selectedLayerId);

    if (!layer || layer->type() != LayerType::Image) {
        // Try hit-testing an image layer
        const auto& children = ctx.document->rootGroup()->children;
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            Layer* l = it->get();
            if (l && l->visible && !l->locked && l->type() == LayerType::Image) {
                const QTransform m = l->transform.matrix(l->contentBounds());
                const QPointF local = m.inverted().map(docPos);
                if (l->contentBounds().contains(local)) {
                    if (ctx.selectLayer)
                        ctx.selectLayer(l->id());
                    layer = l;
                    break;
                }
            }
        }
    }

    if (layer && layer->type() == LayerType::Image) {
        const QTransform matrix = layer->transform.matrix(layer->contentBounds());
        const QPointF localPos = matrix.inverted().map(docPos);
        m_polygon << localPos;
        m_currentHover = localPos;
        m_isDrawing = true;
        if (ctx.requestUpdate)
            ctx.requestUpdate();
        event->accept();
    }
}

void ScissorsTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    if (!m_isDrawing || !ctx.document || ctx.selectedLayerId.isNull())
        return;

    Layer* layer = ctx.document->findLayer(ctx.selectedLayerId);
    if (layer && layer->type() == LayerType::Image) {
        const QTransform matrix = layer->transform.matrix(layer->contentBounds());
        m_currentHover = matrix.inverted().map(docPos);
        if (ctx.requestUpdate)
            ctx.requestUpdate();
    }
}

void ScissorsTool::mouseDoubleClick(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event); Q_UNUSED(docPos);
    applyCut(ctx);
    if (event) event->accept();
}

void ScissorsTool::keyPress(QKeyEvent* event, const ToolContext& ctx)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        applyCut(ctx);
        event->accept();
    } else if (event->key() == Qt::Key_Escape) {
        cancel(ctx);
        event->accept();
    }
}

void ScissorsTool::applyCut(const ToolContext& ctx)
{
    if (!ctx.document || ctx.selectedLayerId.isNull() || m_polygon.size() < 3) {
        cancel(ctx);
        return;
    }

    Layer* layer = ctx.document->findLayer(ctx.selectedLayerId);
    if (!layer || layer->type() != LayerType::Image) {
        cancel(ctx);
        return;
    }

    auto* imgLayer = static_cast<ImageLayer*>(layer);
    const QImage oldImg = ctx.document->assets().decodedImage(imgLayer->assetId);
    if (oldImg.isNull()) {
        cancel(ctx);
        return;
    }

    const ScissorsCutResult res = ImageProcessing::scissorsCut(oldImg, m_polygon, m_keepInside, m_autoCrop);
    if (!res.image.isNull()) {
        const LayerId newAssetId = ctx.document->assets().addImage(res.image);

        AffineTransform newTransform = layer->transform;
        if (res.offset != QPoint(0, 0)) {
            QTransform rotScale;
            rotScale.rotate(layer->transform.rotationDeg);
            rotScale.scale(layer->transform.scaleX, layer->transform.scaleY);
            const QPointF worldOffset = rotScale.map(QPointF(res.offset));
            newTransform.position = layer->transform.position + worldOffset;
        }

        const QString cmdName = ctx.i18n ? ctx.i18n->t("editor", "canvas.command.scissorsCut") : QStringLiteral("Scissors Cut");

        if (ctx.history) {
            ctx.history->execute(std::make_unique<ModifyImageLayerCommand>(
                *ctx.document, imgLayer->id(),
                imgLayer->assetId, oldImg.width(), oldImg.height(), imgLayer->transform,
                newAssetId, res.image.width(), res.image.height(), newTransform,
                cmdName
            ));
        }
    }

    cancel(ctx);
}

void ScissorsTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!ctx.document || ctx.selectedLayerId.isNull() || m_polygon.isEmpty())
        return;

    Layer* layer = ctx.document->findLayer(ctx.selectedLayerId);
    if (!layer || layer->type() != LayerType::Image)
        return;

    const QTransform layerToDoc = layer->transform.matrix(layer->contentBounds());
    const QTransform layerToDevice = layerToDoc * ctx.docToDevice;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPolygonF screenPoly;
    for (const QPointF& pt : m_polygon)
        screenPoly << layerToDevice.map(pt);

    if (screenPoly.size() >= 3) {
        painter->setBrush(QColor(0, 220, 255, 45));
        painter->setPen(Qt::NoPen);
        painter->drawPolygon(screenPoly);
    }

    QPen shadowPen(QColor(0, 0, 0, 180), 3.0);
    shadowPen.setCosmetic(true);
    painter->setPen(shadowPen);
    painter->drawPolyline(screenPoly);

    QPen linePen(QColor(0, 220, 255), 1.5, Qt::DashLine);
    linePen.setCosmetic(true);
    painter->setPen(linePen);
    painter->drawPolyline(screenPoly);

    if (m_isDrawing) {
        QPointF lastPt = screenPoly.last();
        QPointF hoverPt = layerToDevice.map(m_currentHover);
        painter->setPen(QPen(QColor(255, 255, 255, 200), 1.0, Qt::DotLine));
        painter->drawLine(lastPt, hoverPt);
    }

    painter->setBrush(Qt::white);
    painter->setPen(QPen(QColor(0, 180, 220), 1.5));
    for (const QPointF& pt : screenPoly) {
        painter->drawEllipse(pt, 3.5, 3.5);
    }

    painter->restore();
}

void ScissorsTool::cancel(const ToolContext& ctx)
{
    m_polygon.clear();
    m_isDrawing = false;
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

} // namespace cc
