#include "RasterTools.h"
#include <QMouseEvent>
#include <QPainter>
#include "core/layers/Layer.h"
#include "core/history/DocumentCommands.h"
#include "core/history/CommandStack.h"
#include "core/image/ImageProcessing.h"

namespace cc {

// --- Magic Wand Tool ---
void MagicWandTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(docPos);
    if (!ctx.document || event->button() != Qt::LeftButton) return;

    const auto& children = ctx.document->rootGroup()->children;
    if (!children.empty()) {
        Layer* active = children.back().get();
        if (active && active->type() == LayerType::Image) {
            auto* imgLayer = static_cast<ImageLayer*>(active);
            const QImage oldImg = ctx.document->assets().decodedImage(imgLayer->assetId);
            if (!oldImg.isNull()) {
                const QTransform matrix = imgLayer->transform.matrix(imgLayer->contentBounds());
                const QPoint imgPos = matrix.inverted().map(docPos).toPoint();
                if (imgPos.x() >= 0 && imgPos.x() < oldImg.width() && imgPos.y() >= 0 && imgPos.y() < oldImg.height()) {
                    QImage clearedImg = ImageProcessing::removeBackground(oldImg, imgPos, m_tolerance, m_contiguous);
                    const LayerId newAssetId = ctx.document->assets().addImage(clearedImg);
                    if (ctx.history) {
                        ctx.history->execute(std::make_unique<ModifyImageLayerCommand>(
                            *ctx.document, imgLayer->id(),
                            imgLayer->assetId, oldImg.width(), oldImg.height(), imgLayer->transform,
                            newAssetId, clearedImg.width(), clearedImg.height(), imgLayer->transform,
                            QStringLiteral("Magic Wand Cut")
                        ));
                    }
                }
            }
        }
    }
}

// --- Clone Stamp Tool ---
void CloneStampTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(ctx);
    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier)) {
        m_sourcePoint = docPos;
        m_hasSource = true;
        return;
    }

    if (event->button() == Qt::LeftButton && m_hasSource) {
        m_cloning = true;
        m_currentMousePos = docPos;
    }
}

void CloneStampTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event); Q_UNUSED(ctx);
    m_currentMousePos = docPos;
}

void CloneStampTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event); Q_UNUSED(docPos);
    if (!ctx.document || !m_cloning || !m_hasSource) return;
    m_cloning = false;

    const auto& children = ctx.document->rootGroup()->children;
    if (!children.empty()) {
        Layer* active = children.back().get();
        if (active && active->type() == LayerType::Image) {
            auto* imgLayer = static_cast<ImageLayer*>(active);
            const QImage oldImg = ctx.document->assets().decodedImage(imgLayer->assetId);
            if (!oldImg.isNull()) {
                const QTransform matrix = imgLayer->transform.matrix(imgLayer->contentBounds());
                const QPoint srcLocal = matrix.inverted().map(m_sourcePoint).toPoint();
                const QPoint dstLocal = matrix.inverted().map(docPos).toPoint();

                QImage clonedImg = oldImg;
                ImageProcessing::cloneStamp(clonedImg, oldImg, srcLocal, dstLocal, m_radius, m_opacity, m_hardness);
                const LayerId newAssetId = ctx.document->assets().addImage(clonedImg);
                if (ctx.history) {
                    ctx.history->execute(std::make_unique<ModifyImageLayerCommand>(
                        *ctx.document, imgLayer->id(),
                        imgLayer->assetId, oldImg.width(), oldImg.height(), imgLayer->transform,
                        newAssetId, clonedImg.width(), clonedImg.height(), imgLayer->transform,
                        QStringLiteral("Clone Stamp")
                    ));
                }
            }
        }
    }
}

void CloneStampTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (m_hasSource) {
        const QPointF devSrc = ctx.docToDevice.map(m_sourcePoint);
        painter->setPen(QPen(QColor(0, 255, 120), 1.5));
        painter->drawLine(devSrc.x() - 6, devSrc.y(), devSrc.x() + 6, devSrc.y());
        painter->drawLine(devSrc.x(), devSrc.y() - 6, devSrc.x(), devSrc.y() + 6);
    }

    const QPointF devDst = ctx.docToDevice.map(m_currentMousePos);
    const double radius = m_radius * ctx.zoom;
    painter->setPen(QPen(QColor(255, 255, 255, 220), 1.5, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(devDst, radius, radius);

    painter->restore();
}

// --- Flood Fill Tool ---
void FloodFillTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (!ctx.document || event->button() != Qt::LeftButton) return;

    const auto& children = ctx.document->rootGroup()->children;
    if (!children.empty()) {
        Layer* active = children.back().get();
        if (active && active->type() == LayerType::Image) {
            auto* imgLayer = static_cast<ImageLayer*>(active);
            const QImage oldImg = ctx.document->assets().decodedImage(imgLayer->assetId);
            if (!oldImg.isNull()) {
                const QTransform matrix = imgLayer->transform.matrix(imgLayer->contentBounds());
                const QPoint imgPos = matrix.inverted().map(docPos).toPoint();
                if (imgPos.x() >= 0 && imgPos.x() < oldImg.width() && imgPos.y() >= 0 && imgPos.y() < oldImg.height()) {
                    QImage filledImg = oldImg;
                    ImageProcessing::floodFill(filledImg, imgPos, m_color, m_tolerance);
                    const LayerId newAssetId = ctx.document->assets().addImage(filledImg);
                    if (ctx.history) {
                        ctx.history->execute(std::make_unique<ModifyImageLayerCommand>(
                            *ctx.document, imgLayer->id(),
                            imgLayer->assetId, oldImg.width(), oldImg.height(), imgLayer->transform,
                            newAssetId, filledImg.width(), filledImg.height(), imgLayer->transform,
                            QStringLiteral("Flood Fill")
                        ));
                    }
                }
            }
        }
    }
}

} // namespace cc
