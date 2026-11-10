#include "HealingTool.h"

#include <QMouseEvent>
#include <QPainter>
#include "core/layers/Layer.h"
#include "core/history/DocumentCommands.h"
#include "core/history/CommandStack.h"
#include "core/image/ImageProcessing.h"
#include "ToolCommit.h"
#include "localization/i18nservice.h"
#include "core/selection/SelectionOperations.h"

namespace cc {

// ============================================================================
// --- Healing Tool (Spot Healing com fonte manual) ---
// ============================================================================

void HealingTool::activate(const ToolContext& ctx)
{
    m_isHealing = false;
    m_hoverValid = false;
    if (ctx.requestStatusMessage) {
        const QString msg = (m_mode == Mode::RedEye)
            ? (ctx.i18n ? ctx.i18n->t("editor", "canvas.status.redEyeHelp")
                        : QStringLiteral("Red Eye: click the red pupil to neutralize it."))
            : (ctx.i18n ? ctx.i18n->t("editor", "canvas.status.healHelp")
                        : QStringLiteral("Spot Healing: Alt+Click to set source, then click to heal."));
        ctx.requestStatusMessage(msg);
    }
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void HealingTool::deactivate(const ToolContext& ctx)
{
    cancel(ctx);
}

void HealingTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (!ctx.document) return;

    // Photocraft: pixels trancados bloqueiam o healing.
    Layer* targetLayer = nullptr;
    if (!ctx.selectedLayerId.isNull())
        targetLayer = ctx.document->findLayer(ctx.selectedLayerId);
    if (!targetLayer || targetLayer->type() != LayerType::Image) {
        const auto& children = ctx.document->rootGroup()->children;
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            Layer* l = it->get();
            if (l && l->visible && l->type() == LayerType::Image) {
                const QTransform m = l->transform.matrix(l->contentBounds());
                const QPointF local = m.inverted().map(docPos);
                if (l->contentBounds().contains(local)) {
                    targetLayer = l;
                    break;
                }
            }
        }
    }
    if (!targetLayer || !SelectionOperations::canEditPixels(*ctx.document, targetLayer->id())) {
        reportPixelsLocked(ctx);
        event->accept();
        return;
    }

    // Modo Olho Vermelho: clique único neutraliza a pupila vermelha.
    // Não precisa de fonte — a correção é local ao pixel clicado.
    if (m_mode == Mode::RedEye && event->button() == Qt::LeftButton) {
        Layer* layer = ctx.document->findLayer(targetLayer->id());
        if (layer && layer->type() == LayerType::Image) {
            auto* img = static_cast<ImageLayer*>(layer);
            m_origAssetId = img->assetId;
            m_origWidth = img->naturalWidth;
            m_origHeight = img->naturalHeight;
            m_origTransform = layer->transform;

            const QTransform matrix = layer->transform.matrix(layer->contentBounds());
            const QPoint dstLocal = matrix.inverted().map(docPos).toPoint();

            m_workingImage = ctx.document->assets().decodedImage(img->assetId);
            if (!m_workingImage.isNull())
                m_workingImage = m_workingImage.copy(); // detached: não corrompe o cache
            if (!m_workingImage.isNull()) {
                m_isHealing = true;
                m_workingImage = ImageProcessing::removeRedEye(
                    m_workingImage, dstLocal, m_radius);

                if (ctx.setLayerPreview)
                    ctx.setLayerPreview(img->id(), m_workingImage);
                if (ctx.requestUpdate)
                    ctx.requestUpdate();
            }
            event->accept();
            return;
        }
    }

    // Alt+clique: definir fonte (origem)
    const bool isSourceSet = (event->button() == Qt::RightButton)
                          || (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier));

    if (isSourceSet) {
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

            // Captura snapshot da origem para referência
            auto* img = static_cast<ImageLayer*>(layer);
            m_srcSnapshot = ctx.document->assets().decodedImage(img->assetId);

            if (ctx.requestStatusMessage) {
                const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.healSourceSet").arg(m_srcPointLocal.x()).arg(m_srcPointLocal.y())
                                             : QStringLiteral("Healing source set at (%1, %2). Click to heal.")
                                                   .arg(m_srcPointLocal.x()).arg(m_srcPointLocal.y());
                ctx.requestStatusMessage(msg);
            }

            if (ctx.requestUpdate)
                ctx.requestUpdate();
            event->accept();
            return;
        }
    }

    // Clique esquerdo: aplicar healing se há fonte definida
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
            if (!m_workingImage.isNull())
                m_workingImage = m_workingImage.copy(); // detached: não corrompe o cache
            if (!m_workingImage.isNull()) {
                m_isHealing = true;
                const QTransform matrix = layer->transform.matrix(layer->contentBounds());
                const QPoint dstLocal = matrix.inverted().map(docPos).toPoint();

                ImageProcessing::spotHeal(m_workingImage, dstLocal, m_srcPointLocal, m_radius, m_strength);

                if (ctx.setLayerPreview)
                    ctx.setLayerPreview(img->id(), m_workingImage);

                if (ctx.requestUpdate)
                    ctx.requestUpdate();
                event->accept();
            }
        }
    }
}

void HealingTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    m_hoverDocPos = docPos;
    m_hoverValid = true;

    if (m_isHealing && ctx.document && !m_workingImage.isNull()) {
        Layer* layer = ctx.document->findLayer(m_srcLayerId);
        if (!layer && !ctx.selectedLayerId.isNull())
            layer = ctx.document->findLayer(ctx.selectedLayerId);
        if (!layer) {
            // RedEye não usa m_srcLayerId — recupera pela seleção.
            layer = ctx.document->findLayer(ctx.selectedLayerId);
        }

        if (layer && layer->type() == LayerType::Image) {
            auto* img = static_cast<ImageLayer*>(layer);
            const QTransform matrix = layer->transform.matrix(layer->contentBounds());
            const QPoint dstLocal = matrix.inverted().map(docPos).toPoint();

            if (m_mode == Mode::RedEye) {
                // RedEye: um clique, uma correção — o arrasto não repete.
                m_isHealing = false;
            } else {
                // Re-trabalha a partir da cópia original para evitar acúmulo de artefatos
                m_workingImage = ctx.document->assets().decodedImage(img->assetId);
                ImageProcessing::spotHeal(m_workingImage, dstLocal, m_srcPointLocal, m_radius, m_strength);
            }

            if (ctx.setLayerPreview)
                ctx.setLayerPreview(img->id(), m_workingImage);

            if (ctx.requestUpdate)
                ctx.requestUpdate();
            return;
        }
    }

    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void HealingTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event); Q_UNUSED(docPos);
    if (m_isHealing && ctx.document && !m_workingImage.isNull()) {
        m_isHealing = false;
        Layer* layer = ctx.document->findLayer(m_srcLayerId);
        if (!layer && !ctx.selectedLayerId.isNull())
            layer = ctx.document->findLayer(ctx.selectedLayerId);
        if (!layer)
            layer = ctx.document->findLayer(ctx.selectedLayerId);

        if (layer && layer->type() == LayerType::Image) {
            auto* img = static_cast<ImageLayer*>(layer);
            const LayerId finalAssetId = ctx.document->assets().addImage(m_workingImage);

            if (ctx.clearLayerPreview)
                ctx.clearLayerPreview();

            const QString cmdName = (m_mode == Mode::RedEye)
                ? (ctx.i18n ? ctx.i18n->t("editor", "canvas.command.redEye")
                            : QStringLiteral("Red Eye"))
                : (ctx.i18n ? ctx.i18n->t("editor", "canvas.command.healing")
                            : QStringLiteral("Spot Healing"));

            if (ctx.history) {
                ctx.history->execute(std::make_unique<ModifyImageLayerCommand>(
                    *ctx.document, img->id(),
                    m_origAssetId, m_origWidth, m_origHeight, m_origTransform,
                    finalAssetId, img->naturalWidth, img->naturalHeight, layer->transform,
                    cmdName
                ));
            } else {
                ctx.document->setImageLayerAsset(img->id(), finalAssetId,
                                                 img->naturalWidth, img->naturalHeight);
            }
        }
    }

    m_isHealing = false;
    m_workingImage = QImage();
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void HealingTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!ctx.document) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Se a fonte está definida, desenha crosshair vermelho na origem
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

    // Desenha círculo do cursor
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

void HealingTool::cancel(const ToolContext& ctx)
{
    if (m_isHealing && ctx.clearLayerPreview && !m_srcLayerId.isNull())
        ctx.clearLayerPreview();
    m_isHealing = false;
    m_workingImage = QImage();
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

} // namespace cc
