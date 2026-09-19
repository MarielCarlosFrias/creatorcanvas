#include "ScissorsTool.h"
#include <QMouseEvent>
#include <QPainter>
#include "core/layers/Layer.h"
#include "core/history/DocumentCommands.h"
#include "core/history/CommandStack.h"
#include "core/image/ImageProcessing.h"

namespace cc {

void ScissorsTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(ctx);
    if (event->button() == Qt::LeftButton) {
        m_polygon.append(docPos);
        m_currentCursor = docPos;
    }
}

void ScissorsTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event); Q_UNUSED(ctx);
    m_currentCursor = docPos;
}

void ScissorsTool::mouseDoubleClick(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event); Q_UNUSED(docPos);
    applyCut(ctx);
}

void ScissorsTool::applyCut(const ToolContext& ctx)
{
    if (!ctx.document || m_polygon.size() < 3) return;

    const auto& children = ctx.document->rootGroup()->children;
    if (!children.empty()) {
        Layer* active = children.back().get();
        if (active && active->type() == LayerType::Image) {
            auto* imgLayer = static_cast<ImageLayer*>(active);
            const QImage oldImg = ctx.document->assets().decodedImage(imgLayer->assetId);
            if (!oldImg.isNull()) {
                ScissorsCutResult result = ImageProcessing::scissorsCut(oldImg, m_polygon, m_keepInside, m_autoCrop);
                if (!result.image.isNull()) {
                    const LayerId newAssetId = ctx.document->assets().addImage(result.image);
                    if (ctx.history) {
                        AffineTransform newTransform = imgLayer->transform;
                        newTransform.position += QPointF(result.offset);
                        ctx.history->execute(std::make_unique<ModifyImageLayerCommand>(
                            *ctx.document, imgLayer->id(),
                            imgLayer->assetId, oldImg.width(), oldImg.height(), imgLayer->transform,
                            newAssetId, result.image.width(), result.image.height(), newTransform,
                            QStringLiteral("Scissors Cut")
                        ));
                    }
                }
            }
        }
    }
    m_polygon.clear();
}

void ScissorsTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (m_polygon.isEmpty()) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPolygonF devPoly;
    for (const QPointF& p : m_polygon)
        devPoly.append(ctx.docToDevice.map(p));

    painter->setPen(QPen(QColor(0, 240, 255), 1.5, Qt::DashLine));
    painter->setBrush(QColor(0, 240, 255, 35));
    painter->drawPolygon(devPoly);

    if (!devPoly.isEmpty()) {
        const QPointF devCursor = ctx.docToDevice.map(m_currentCursor);
        painter->drawLine(devPoly.last(), devCursor);
    }

    painter->restore();
}

void ScissorsTool::cancel(const ToolContext& ctx)
{
    Q_UNUSED(ctx);
    m_polygon.clear();
}

} // namespace cc
