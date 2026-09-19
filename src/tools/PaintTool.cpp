#include "PaintTool.h"
#include <QMouseEvent>
#include <QPainter>
#include "core/layers/Layer.h"
#include "core/history/DocumentCommands.h"
#include "core/history/CommandStack.h"

namespace cc {

void PaintTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (!ctx.document || event->button() != Qt::LeftButton)
        return;

    m_painting = true;
    m_strokePoints.clear();
    m_strokePoints.append(docPos);
    m_currentMousePos = docPos;
}

void PaintTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event); Q_UNUSED(ctx);
    m_currentMousePos = docPos;
    if (m_painting)
        m_strokePoints.append(docPos);
}

void PaintTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    if (!ctx.document || !m_painting)
        return;

    m_strokePoints.append(docPos);
    m_painting = false;

    // Apply stroke to active image layer
    const auto& children = ctx.document->rootGroup()->children;
    if (!children.empty() && m_strokePoints.size() >= 1) {
        Layer* active = children.back().get(); // Default to top/active layer
        if (active && active->type() == LayerType::Image) {
            auto* imgLayer = static_cast<ImageLayer*>(active);
            const QImage oldImg = ctx.document->assets().decodedImage(imgLayer->assetId);
            if (!oldImg.isNull()) {
                QImage modified = oldImg;
                for (int i = 1; i < m_strokePoints.size(); ++i) {
                    modified = ImageProcessing::paintStroke(modified, m_strokePoints[i - 1], m_strokePoints[i], m_brushType, m_color, m_size, m_opacity);
                }

                const LayerId newAssetId = ctx.document->assets().addImage(modified);
                if (ctx.history) {
                    ctx.history->execute(std::make_unique<ModifyImageLayerCommand>(
                        *ctx.document, imgLayer->id(),
                        imgLayer->assetId, oldImg.width(), oldImg.height(), imgLayer->transform,
                        newAssetId, modified.width(), modified.height(), imgLayer->transform,
                        QStringLiteral("Paint Stroke")
                    ));
                }
            }
        }
    }
    m_strokePoints.clear();
}

void PaintTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QPointF devP = ctx.docToDevice.map(m_currentMousePos);
    const double radius = (m_size * 0.5) * ctx.zoom;

    painter->setPen(QPen(QColor(255, 255, 255, 220), 1.5, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(devP, radius, radius);

    painter->setPen(QPen(QColor(0, 0, 0, 180), 1.0));
    painter->drawEllipse(devP, radius + 1.0, radius + 1.0);

    painter->restore();
}

void PaintTool::cancel(const ToolContext& ctx)
{
    Q_UNUSED(ctx);
    m_painting = false;
    m_strokePoints.clear();
}

} // namespace cc
