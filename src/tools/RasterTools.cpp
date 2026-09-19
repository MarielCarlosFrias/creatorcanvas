#include "RasterTools.h"
#include <QMouseEvent>
#include <QPainter>
#include "core/layers/Layer.h"
#include "core/history/DocumentCommands.h"
#include "core/history/CommandStack.h"
#include "core/image/ImageProcessing.h"
#include "localization/i18nservice.h"

namespace cc {

// ============================================================================
// --- Magic Wand Tool ---
// ============================================================================

void MagicWandTool::activate(const ToolContext& ctx)
{
    if (ctx.requestStatusMessage) {
        const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.wandHelp")
                                     : QStringLiteral("Magic Wand: Click on an image area to remove matching color.");
        ctx.requestStatusMessage(msg);
    }
}

void MagicWandTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (!ctx.document || event->button() != Qt::LeftButton) return;

    Layer* layer = nullptr;
    if (!ctx.selectedLayerId.isNull())
        layer = ctx.document->findLayer(ctx.selectedLayerId);

    if (!layer || layer->type() != LayerType::Image) {
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
        auto* imgLayer = static_cast<ImageLayer*>(layer);
        const QImage oldImg = ctx.document->assets().decodedImage(imgLayer->assetId);
        if (!oldImg.isNull()) {
            const QTransform matrix = imgLayer->transform.matrix(imgLayer->contentBounds());
            const QPoint imgPos = matrix.inverted().map(docPos).toPoint();
            if (imgPos.x() >= 0 && imgPos.x() < oldImg.width() && imgPos.y() >= 0 && imgPos.y() < oldImg.height()) {
                QImage clearedImg = ImageProcessing::removeBackground(oldImg, imgPos, m_tolerance, m_contiguous);
                const LayerId newAssetId = ctx.document->assets().addImage(clearedImg);
                const QString cmdName = ctx.i18n ? ctx.i18n->t("editor", "canvas.command.magicWand") : QStringLiteral("Magic Wand Cut");

                if (ctx.history) {
                    ctx.history->execute(std::make_unique<ModifyImageLayerCommand>(
                        *ctx.document, imgLayer->id(),
                        imgLayer->assetId, oldImg.width(), oldImg.height(), imgLayer->transform,
                        newAssetId, clearedImg.width(), clearedImg.height(), imgLayer->transform,
                        cmdName
                    ));
                }

                if (ctx.requestStatusMessage) {
                    const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.wandSuccess").arg(m_tolerance)
                                                 : QStringLiteral("✓ Color area removed (Tolerance: %1)").arg(m_tolerance);
                    ctx.requestStatusMessage(msg);
                }

                if (ctx.requestUpdate)
                    ctx.requestUpdate();
                event->accept();
            }
        }
    }
}

// ============================================================================
// --- Clone Stamp Tool ---
// ============================================================================

void CloneStampTool::activate(const ToolContext& ctx)
{
    m_isCloning = false;
    m_hoverValid = false;
    if (ctx.requestStatusMessage) {
        const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.cloneHelp")
                                     : QStringLiteral("Clone Stamp: Right-Click or Shift+Click to set origin, then drag to paint.");
        ctx.requestStatusMessage(msg);
    }
}

void CloneStampTool::deactivate(const ToolContext& ctx)
{
    cancel(ctx);
}

void CloneStampTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (!ctx.document) return;

    // Setting origin with Right Button or Alt/Shift+LeftClick
    const bool isOriginSet = (event->button() == Qt::RightButton)
                          || (event->button() == Qt::LeftButton && (event->modifiers() & (Qt::AltModifier | Qt::ShiftModifier)));

    if (isOriginSet) {
        Layer* layer = nullptr;
        if (!ctx.selectedLayerId.isNull())
            layer = ctx.document->findLayer(ctx.selectedLayerId);

        if (!layer || layer->type() != LayerType::Image) {
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
            m_srcPointLocal = matrix.inverted().map(docPos).toPoint();
            m_srcLayerId = layer->id();
            m_hasSource = true;

            if (ctx.requestStatusMessage) {
                const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.cloneOriginSet").arg(m_srcPointLocal.x()).arg(m_srcPointLocal.y())
                                             : QStringLiteral("Clone stamp origin set at (%1, %2). Now drag to paint.")
                                                   .arg(m_srcPointLocal.x()).arg(m_srcPointLocal.y());
                ctx.requestStatusMessage(msg);
            }

            if (ctx.requestUpdate)
                ctx.requestUpdate();
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton && m_hasSource) {
        Layer* layer = ctx.document->findLayer(m_srcLayerId);
        if (!layer && !ctx.selectedLayerId.isNull())
            layer = ctx.document->findLayer(ctx.selectedLayerId);

        if (layer && layer->type() == LayerType::Image) {
            auto* img = static_cast<ImageLayer*>(layer);
            m_origAssetId = img->assetId;
            m_origWidth = img->naturalWidth;
            m_origHeight = img->naturalHeight;
            m_origTransform = layer->transform;

            m_workingImage = ctx.document->assets().decodedImage(img->assetId);
            if (!m_workingImage.isNull()) {
                m_isCloning = true;
                const QTransform matrix = layer->transform.matrix(layer->contentBounds());
                const QPoint dstLocal = matrix.inverted().map(docPos).toPoint();

                ImageProcessing::cloneStamp(m_workingImage, m_workingImage, m_srcPointLocal, dstLocal, m_radius, m_opacity, m_hardness);
                LayerId tempAssetId = ctx.document->assets().addImage(m_workingImage);
                ctx.document->setImageLayerAsset(img->id(), tempAssetId, img->naturalWidth, img->naturalHeight);

                if (ctx.requestUpdate)
                    ctx.requestUpdate();
                event->accept();
            }
        }
    }
}

void CloneStampTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    m_hoverDocPos = docPos;
    m_hoverValid = true;

    if (m_isCloning && ctx.document) {
        Layer* layer = ctx.document->findLayer(m_srcLayerId);
        if (!layer && !ctx.selectedLayerId.isNull())
            layer = ctx.document->findLayer(ctx.selectedLayerId);

        if (layer && layer->type() == LayerType::Image && !m_workingImage.isNull()) {
            auto* img = static_cast<ImageLayer*>(layer);
            const QTransform matrix = layer->transform.matrix(layer->contentBounds());
            const QPoint dstLocal = matrix.inverted().map(docPos).toPoint();

            ImageProcessing::cloneStamp(m_workingImage, m_workingImage, m_srcPointLocal, dstLocal, m_radius, m_opacity, m_hardness);
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

void CloneStampTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event); Q_UNUSED(docPos);
    if (m_isCloning && ctx.document && !m_workingImage.isNull()) {
        m_isCloning = false;
        Layer* layer = ctx.document->findLayer(m_srcLayerId);
        if (!layer && !ctx.selectedLayerId.isNull())
            layer = ctx.document->findLayer(ctx.selectedLayerId);

        if (layer && layer->type() == LayerType::Image) {
            auto* img = static_cast<ImageLayer*>(layer);
            LayerId finalAssetId = ctx.document->assets().addImage(m_workingImage);

            // Revert temporary live asset
            ctx.document->setImageLayerAsset(img->id(), m_origAssetId, m_origWidth, m_origHeight);

            const QString cmdName = ctx.i18n ? ctx.i18n->t("editor", "canvas.command.cloneStamp") : QStringLiteral("Clone Stamp");

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

    m_isCloning = false;
    m_workingImage = QImage();
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void CloneStampTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!ctx.document) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // If source origin is set, draw red crosshair
    if (m_hasSource) {
        Layer* srcLayer = ctx.document->findLayer(m_srcLayerId);
        if (!srcLayer && !ctx.selectedLayerId.isNull())
            srcLayer = ctx.document->findLayer(ctx.selectedLayerId);

        if (srcLayer && srcLayer->type() == LayerType::Image) {
            const QTransform layerToDoc = srcLayer->transform.matrix(srcLayer->contentBounds());
            const QTransform layerToDevice = layerToDoc * ctx.docToDevice;
            const QPointF srcScreen = layerToDevice.map(QPointF(m_srcPointLocal));

            QPen srcPen(QColor(255, 60, 60), 2.0);
            srcPen.setCosmetic(true);
            painter->setPen(srcPen);
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(srcScreen, 6, 6);
            painter->drawLine(srcScreen.x() - 10, srcScreen.y(), srcScreen.x() + 10, srcScreen.y());
            painter->drawLine(srcScreen.x(), srcScreen.y() - 10, srcScreen.x(), srcScreen.y() + 10);
        }
    }

    // Draw cursor circle
    if (m_hoverValid) {
        const QPointF hoverScreen = ctx.docToDevice.map(m_hoverDocPos);
        const double screenRadius = std::max(2.0, m_radius * ctx.zoom);

        QPen brushPen(QColor(255, 255, 255, 220), 1.5, Qt::DashLine);
        brushPen.setCosmetic(true);
        painter->setPen(brushPen);
        painter->setBrush(QColor(255, 255, 255, 25));
        painter->drawEllipse(hoverScreen, screenRadius, screenRadius);
    }

    painter->restore();
}

void CloneStampTool::cancel(const ToolContext& ctx)
{
    if (m_isCloning && ctx.document && !m_srcLayerId.isNull()) {
        Layer* layer = ctx.document->findLayer(m_srcLayerId);
        if (layer && layer->type() == LayerType::Image) {
            auto* img = static_cast<ImageLayer*>(layer);
            ctx.document->setImageLayerAsset(img->id(), m_origAssetId, m_origWidth, m_origHeight);
        }
    }
    m_isCloning = false;
    m_workingImage = QImage();
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

// ============================================================================
// --- Flood Fill Tool ---
// ============================================================================

void FloodFillTool::activate(const ToolContext& ctx)
{
    if (ctx.requestStatusMessage) {
        const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.fillHelp")
                                     : QStringLiteral("Flood Fill: Click inside an image region to fill with color.");
        ctx.requestStatusMessage(msg);
    }
}

void FloodFillTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (!ctx.document || event->button() != Qt::LeftButton) return;

    Layer* layer = nullptr;
    if (!ctx.selectedLayerId.isNull())
        layer = ctx.document->findLayer(ctx.selectedLayerId);

    if (!layer || layer->type() != LayerType::Image) {
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
        auto* imgLayer = static_cast<ImageLayer*>(layer);
        const QImage oldImg = ctx.document->assets().decodedImage(imgLayer->assetId);
        if (!oldImg.isNull()) {
            const QTransform matrix = imgLayer->transform.matrix(imgLayer->contentBounds());
            const QPoint imgPos = matrix.inverted().map(docPos).toPoint();
            if (imgPos.x() >= 0 && imgPos.x() < oldImg.width() && imgPos.y() >= 0 && imgPos.y() < oldImg.height()) {
                QImage filledImg = oldImg;
                ImageProcessing::floodFill(filledImg, imgPos, m_color, m_tolerance);
                const LayerId newAssetId = ctx.document->assets().addImage(filledImg);
                const QString cmdName = ctx.i18n ? ctx.i18n->t("editor", "canvas.command.floodFill") : QStringLiteral("Flood Fill");

                if (ctx.history) {
                    ctx.history->execute(std::make_unique<ModifyImageLayerCommand>(
                        *ctx.document, imgLayer->id(),
                        imgLayer->assetId, oldImg.width(), oldImg.height(), imgLayer->transform,
                        newAssetId, filledImg.width(), filledImg.height(), imgLayer->transform,
                        cmdName
                    ));
                }

                if (ctx.requestUpdate)
                    ctx.requestUpdate();
                event->accept();
            }
        }
    }
}

} // namespace cc
