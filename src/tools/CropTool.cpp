#include "CropTool.h"
#include <QMouseEvent>
#include <QPainter>
#include "core/layers/Layer.h"
#include "core/history/DocumentCommands.h"
#include "core/history/CommandStack.h"
#include "core/image/ImageProcessing.h"

namespace cc {

void CropTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(ctx);
    if (event->button() != Qt::LeftButton) return;
    m_dragging = true;
    m_startPos = docPos;
    m_cropRect = QRectF(docPos, QSizeF(0, 0));
}

void CropTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event); Q_UNUSED(ctx);
    if (!m_dragging) return;
    m_cropRect = QRectF(m_startPos, docPos).normalized();

    if (m_aspectRatio > 0.0 && m_cropRect.height() > 0.0) {
        double currentRatio = m_cropRect.width() / m_cropRect.height();
        if (currentRatio > m_aspectRatio) {
            m_cropRect.setWidth(m_cropRect.height() * m_aspectRatio);
        } else {
            m_cropRect.setHeight(m_cropRect.width() / m_aspectRatio);
        }
    }
}

void CropTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event); Q_UNUSED(docPos); Q_UNUSED(ctx);
    m_dragging = false;
}

void CropTool::applyCrop(const ToolContext& ctx)
{
    if (!ctx.document || m_cropRect.isEmpty() || m_cropRect.width() < 4.0 || m_cropRect.height() < 4.0)
        return;

    const auto& children = ctx.document->rootGroup()->children;
    if (!children.empty()) {
        Layer* active = children.back().get();
        if (active && active->type() == LayerType::Image) {
            auto* imgLayer = static_cast<ImageLayer*>(active);
            const QImage oldImg = ctx.document->assets().decodedImage(imgLayer->assetId);
            if (!oldImg.isNull()) {
                QImage cropped = ImageProcessing::cropImage(oldImg, m_cropRect.toRect());
                if (!cropped.isNull()) {
                    const LayerId newAssetId = ctx.document->assets().addImage(cropped);
                    if (ctx.history) {
                        ctx.history->execute(std::make_unique<ModifyImageLayerCommand>(
                            *ctx.document, imgLayer->id(),
                            imgLayer->assetId, oldImg.width(), oldImg.height(), imgLayer->transform,
                            newAssetId, cropped.width(), cropped.height(), imgLayer->transform,
                            QStringLiteral("Crop Image")
                        ));
                    }
                }
            }
        }
    }
    m_cropRect = QRectF();
}

void CropTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (m_cropRect.isEmpty()) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF devRect = ctx.docToDevice.mapRect(m_cropRect);
    painter->setPen(QPen(QColor(255, 200, 0), 2, Qt::SolidLine));
    painter->setBrush(QColor(255, 200, 0, 30));
    painter->drawRect(devRect);

    // Rule of thirds lines inside crop
    const double w = devRect.width();
    const double h = devRect.height();
    painter->setPen(QPen(QColor(255, 200, 0, 150), 1, Qt::DashLine));
    painter->drawLine(QPointF(devRect.left() + w / 3.0, devRect.top()), QPointF(devRect.left() + w / 3.0, devRect.bottom()));
    painter->drawLine(QPointF(devRect.left() + (2.0 * w) / 3.0, devRect.top()), QPointF(devRect.left() + (2.0 * w) / 3.0, devRect.bottom()));
    painter->drawLine(QPointF(devRect.left(), devRect.top() + h / 3.0), QPointF(devRect.right(), devRect.top() + h / 3.0));
    painter->drawLine(QPointF(devRect.left(), devRect.top() + (2.0 * h) / 3.0), QPointF(devRect.right(), devRect.top() + (2.0 * h) / 3.0));

    painter->restore();
}

void CropTool::cancel(const ToolContext& ctx)
{
    Q_UNUSED(ctx);
    m_dragging = false;
    m_cropRect = QRectF();
}

} // namespace cc
