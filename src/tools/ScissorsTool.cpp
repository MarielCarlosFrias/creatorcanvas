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

    QPointF pt;
    if (layer && layer->type() == LayerType::Image) {
        const QTransform matrix = layer->transform.matrix(layer->contentBounds());
        pt = matrix.inverted().map(docPos);
    } else {
        pt = docPos;
    }

    // Se clicar próximo ao primeiro nó, fecha o polígono automaticamente
    if (m_isDrawing && m_polygon.size() >= 3) {
        QPointF firstScreen;
        if (layer && layer->type() == LayerType::Image) {
            const QTransform layerToDevice = layer->transform.matrix(layer->contentBounds()) * ctx.docToDevice;
            firstScreen = layerToDevice.map(m_polygon.first());
        } else {
            firstScreen = ctx.docToDevice.map(m_polygon.first());
        }
        const QPointF clickScreen = ctx.docToDevice.map(docPos);
        if (QLineF(firstScreen, clickScreen).length() <= 10.0) {
            applyCut(ctx);
            event->accept();
            return;
        }
    }

    m_polygon << pt;
    m_currentHover = pt;
    m_isDrawing = true;
    if (ctx.requestUpdate)
        ctx.requestUpdate();
    event->accept();
}

void ScissorsTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    if (!m_isDrawing || !ctx.document)
        return;

    Layer* layer = !ctx.selectedLayerId.isNull() ? ctx.document->findLayer(ctx.selectedLayerId) : nullptr;
    if (layer && layer->type() == LayerType::Image) {
        const QTransform matrix = layer->transform.matrix(layer->contentBounds());
        m_currentHover = matrix.inverted().map(docPos);
    } else {
        m_currentHover = docPos;
    }
    if (ctx.requestUpdate)
        ctx.requestUpdate();
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
    } else if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
        if (m_isDrawing && m_polygon.size() > 1) {
            m_polygon.removeLast();
            m_currentHover = m_polygon.last();
            if (ctx.requestUpdate)
                ctx.requestUpdate();
            event->accept();
        } else if (m_isDrawing && m_polygon.size() <= 1) {
            cancel(ctx);
            event->accept();
        }
    }
}

void ScissorsTool::applyCut(const ToolContext& ctx)
{
    if (!ctx.document || m_polygon.size() < 3) {
        cancel(ctx);
        return;
    }

    Layer* layer = !ctx.selectedLayerId.isNull() ? ctx.document->findLayer(ctx.selectedLayerId) : nullptr;
    QSize targetSize(ctx.document->width(), ctx.document->height());
    if (layer && layer->type() == LayerType::Image) {
        auto* imgLayer = static_cast<ImageLayer*>(layer);
        const QImage oldImg = ctx.document->assets().decodedImage(imgLayer->assetId);
        if (!oldImg.isNull())
            targetSize = oldImg.size();
    }

    QImage selection(targetSize, QImage::Format_Alpha8);
    selection.fill(0);
    QPainter maskPainter(&selection);
    maskPainter.setPen(Qt::NoPen);
    maskPainter.setBrush(Qt::white);
    maskPainter.drawPolygon(QPolygonF(m_polygon));
    maskPainter.end();
    if (!m_keepInside) {
        for (int y = 0; y < selection.height(); ++y) {
            uchar* row = selection.scanLine(y);
            for (int x = 0; x < selection.width(); ++x) row[x] = 255 - row[x];
        }
    }

    // Scissors creates an active selection. The user chooses the operation
    // afterwards (mask, delete, copy, cut, invert or cancel).
    executeSetPixelSelection(*ctx.document, ctx.history, selection,
                             QStringLiteral("selection.scissors"));
    m_polygon.clear();
    m_isDrawing = false;
    if (ctx.requestStatusMessage)
        ctx.requestStatusMessage(QStringLiteral("Selection created. Choose an operation from the layer menu."));
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void ScissorsTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!ctx.document || m_polygon.isEmpty())
        return;

    Layer* layer = !ctx.selectedLayerId.isNull() ? ctx.document->findLayer(ctx.selectedLayerId) : nullptr;
    QTransform pointToDevice;
    if (layer && layer->type() == LayerType::Image) {
        const QTransform layerToDoc = layer->transform.matrix(layer->contentBounds());
        pointToDevice = layerToDoc * ctx.docToDevice;
    } else {
        pointToDevice = ctx.docToDevice;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPolygonF screenPoly;
    for (const QPointF& pt : m_polygon)
        screenPoly << pointToDevice.map(pt);

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
        QPointF hoverPt = pointToDevice.map(m_currentHover);
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
