#include "PaintTool.h"
#include <QMouseEvent>
#include <QPainter>
#include "core/layers/Layer.h"
#include "core/history/DocumentCommands.h"
#include "core/history/CommandStack.h"
#include "localization/i18nservice.h"

namespace cc {

void PaintTool::activate(const ToolContext& ctx)
{
    m_isPainting = false;
    if (ctx.requestStatusMessage) {
        const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.paintHelp")
                                     : QStringLiteral("Paint: Click and drag to paint. Select brush type, color, and size above.");
        ctx.requestStatusMessage(msg);
    }
}

void PaintTool::deactivate(const ToolContext& ctx)
{
    cancel(ctx);
}

void PaintTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (event->button() != Qt::LeftButton || !ctx.document)
        return;

    m_currentDocPos = docPos;

    Layer* layer = nullptr;
    if (!ctx.selectedLayerId.isNull())
        layer = ctx.document->findLayer(ctx.selectedLayerId);

    if (!layer || layer->type() != LayerType::Image) {
        // Try hit-testing
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

    // If still no image layer, create a new transparent layer automatically
    if (!layer || layer->type() != LayerType::Image) {
        auto newImgLayer = std::make_unique<ImageLayer>();
        newImgLayer->name = ctx.i18n ? ctx.i18n->t("editor", "canvas.layer.paint") : QStringLiteral("Painting");
        newImgLayer->naturalWidth = ctx.document->width();
        newImgLayer->naturalHeight = ctx.document->height();
        newImgLayer->transform.position = QPointF(ctx.document->width() / 2.0, ctx.document->height() / 2.0);

        QImage blank(ctx.document->width(), ctx.document->height(), QImage::Format_ARGB32_Premultiplied);
        blank.fill(Qt::transparent);
        LayerId assetId = ctx.document->assets().addImage(blank);
        newImgLayer->assetId = assetId;

        const LayerId createdId = newImgLayer->id();
        ctx.document->addLayer(std::move(newImgLayer));
        if (ctx.selectLayer)
            ctx.selectLayer(createdId);
        layer = ctx.document->findLayer(createdId);
    }

    if (layer && layer->type() == LayerType::Image) {
        auto* img = static_cast<ImageLayer*>(layer);
        const QTransform matrix = layer->transform.matrix(layer->contentBounds());
        const QPointF localPos = matrix.inverted().map(docPos);

        m_activeLayerId = layer->id();
        m_origAssetId = img->assetId;
        m_origWidth = img->naturalWidth;
        m_origHeight = img->naturalHeight;
        m_origTransform = layer->transform;

        m_workingImage = ctx.document->assets().decodedImage(img->assetId);
        if (m_workingImage.isNull()) {
            m_workingImage = QImage(img->naturalWidth > 0 ? img->naturalWidth : ctx.document->width(),
                                    img->naturalHeight > 0 ? img->naturalHeight : ctx.document->height(),
                                    QImage::Format_ARGB32_Premultiplied);
            m_workingImage.fill(Qt::transparent);
        }

        m_prevPointLocal = localPos;
        m_isPainting = true;

        // Paint first dot
        m_workingImage = ImageProcessing::paintStroke(
            m_workingImage, m_prevPointLocal, localPos,
            m_brushType, m_color, m_size, m_opacity);

        LayerId tempAssetId = ctx.document->assets().addImage(m_workingImage);
        ctx.document->setImageLayerAsset(img->id(), tempAssetId, img->naturalWidth, img->naturalHeight);

        if (ctx.requestUpdate)
            ctx.requestUpdate();
        event->accept();
    }
}

void PaintTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    m_currentDocPos = docPos;

    if (m_isPainting && ctx.document && !m_activeLayerId.isNull()) {
        Layer* layer = ctx.document->findLayer(m_activeLayerId);
        if (layer && layer->type() == LayerType::Image) {
            auto* img = static_cast<ImageLayer*>(layer);
            const QTransform matrix = layer->transform.matrix(layer->contentBounds());
            const QPointF localPos = matrix.inverted().map(docPos);

            m_workingImage = ImageProcessing::paintStroke(
                m_workingImage, m_prevPointLocal, localPos,
                m_brushType, m_color, m_size, m_opacity);

            m_prevPointLocal = localPos;

            LayerId tempAssetId = ctx.document->assets().addImage(m_workingImage);
            ctx.document->setImageLayerAsset(img->id(), tempAssetId, img->naturalWidth, img->naturalHeight);

            if (ctx.requestUpdate)
                ctx.requestUpdate();
            return;
        }
    }

    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void PaintTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event); Q_UNUSED(docPos);
    if (m_isPainting && ctx.document && !m_activeLayerId.isNull() && !m_workingImage.isNull()) {
        m_isPainting = false;
        Layer* layer = ctx.document->findLayer(m_activeLayerId);
        if (layer && layer->type() == LayerType::Image) {
            auto* img = static_cast<ImageLayer*>(layer);
            LayerId finalAssetId = ctx.document->assets().addImage(m_workingImage);

            // Revert live preview temporarily so history executes cleanly
            ctx.document->setImageLayerAsset(img->id(), m_origAssetId, m_origWidth, m_origHeight);

            const QString cmdName = ctx.i18n ? ctx.i18n->t("editor", "canvas.layer.paint") : QStringLiteral("Painting");

            if (ctx.history) {
                ctx.history->execute(std::make_unique<ModifyImageLayerCommand>(
                    *ctx.document, img->id(),
                    m_origAssetId, m_origWidth, m_origHeight, m_origTransform,
                    finalAssetId, img->naturalWidth, img->naturalHeight, layer->transform,
                    cmdName
                ));
            }
        }
    }

    m_isPainting = false;
    m_workingImage = QImage();
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void PaintTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QPointF devP = ctx.docToDevice.map(m_currentDocPos);
    const double radius = std::max(2.0, m_size * ctx.zoom);

    QPen cursorPen(QColor(255, 255, 255, 200), 1.0);
    painter->setPen(cursorPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(devP, radius, radius);

    cursorPen.setColor(QColor(0, 0, 0, 120));
    cursorPen.setStyle(Qt::DotLine);
    painter->setPen(cursorPen);
    painter->drawEllipse(devP, radius + 1.0, radius + 1.0);

    painter->restore();
}

void PaintTool::cancel(const ToolContext& ctx)
{
    if (m_isPainting && ctx.document && !m_activeLayerId.isNull()) {
        Layer* layer = ctx.document->findLayer(m_activeLayerId);
        if (layer && layer->type() == LayerType::Image) {
            auto* img = static_cast<ImageLayer*>(layer);
            ctx.document->setImageLayerAsset(img->id(), m_origAssetId, m_origWidth, m_origHeight);
        }
    }
    m_isPainting = false;
    m_workingImage = QImage();
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

} // namespace cc
