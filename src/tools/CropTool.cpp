#include "CropTool.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include "core/layers/Layer.h"
#include "core/history/DocumentCommands.h"
#include "core/history/CommandStack.h"
#include "core/image/ImageProcessing.h"
#include "localization/i18nservice.h"

namespace cc {

void CropTool::activate(const ToolContext& ctx)
{
    initCropRect(ctx);
    if (ctx.requestStatusMessage) {
        const QString msg = ctx.i18n ? ctx.i18n->t("editor", "canvas.status.cropHelp")
                                     : QStringLiteral("Crop: Drag handles to adjust crop area. Press Enter or double-click to apply, Esc to cancel.");
        ctx.requestStatusMessage(msg);
    }
}

void CropTool::deactivate(const ToolContext& ctx)
{
    m_cropRect = QRectF();
    m_activeCropHandle = -1;
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void CropTool::initCropRect(const ToolContext& ctx)
{
    if (!ctx.document || ctx.selectedLayerId.isNull())
        return;

    Layer* layer = ctx.document->findLayer(ctx.selectedLayerId);
    if (layer && layer->type() == LayerType::Image) {
        auto* img = static_cast<ImageLayer*>(layer);
        if (m_aspectRatio > 0.0) {
            m_cropRect = ImageProcessing::calculateAspectCropRect(
                QSize(img->naturalWidth, img->naturalHeight), m_aspectRatio);
        } else {
            m_cropRect = QRectF(0, 0, img->naturalWidth, img->naturalHeight);
        }
        if (ctx.requestUpdate)
            ctx.requestUpdate();
    }
}

void CropTool::setAspectRatio(double ratio, const ToolContext& ctx)
{
    m_aspectRatio = ratio;
    initCropRect(ctx);
}

int CropTool::cropHandleAt(const QPointF& widgetPos, const ToolContext& ctx) const
{
    if (!ctx.document || ctx.selectedLayerId.isNull() || m_cropRect.isEmpty())
        return -1;

    Layer* layer = ctx.document->findLayer(ctx.selectedLayerId);
    if (!layer || layer->type() != LayerType::Image)
        return -1;

    const QTransform layerToDoc = layer->transform.matrix(layer->contentBounds());
    const QTransform layerToDevice = layerToDoc * ctx.docToDevice;

    const double w = m_cropRect.width();
    const double h = m_cropRect.height();
    const double x = m_cropRect.x();
    const double y = m_cropRect.y();

    const QPointF pts[8] = {
        layerToDevice.map(m_cropRect.topLeft()),
        layerToDevice.map(m_cropRect.topRight()),
        layerToDevice.map(m_cropRect.bottomRight()),
        layerToDevice.map(m_cropRect.bottomLeft()),
        layerToDevice.map(QPointF(x + w / 2.0, y)),
        layerToDevice.map(QPointF(x + w, y + h / 2.0)),
        layerToDevice.map(QPointF(x + w / 2.0, y + h)),
        layerToDevice.map(QPointF(x, y + h / 2.0))
    };

    for (int i = 0; i < 8; ++i) {
        if (QLineF(widgetPos, pts[i]).length() <= 10.0)
            return i;
    }

    const QPointF localPos = layerToDevice.inverted().map(widgetPos);
    if (m_cropRect.contains(localPos))
        return 8; // Central move handle

    return -1;
}

void CropTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (event->button() != Qt::LeftButton || !ctx.document)
        return;

    if (!ctx.selectedLayerId.isNull()) {
        Layer* layer = ctx.document->findLayer(ctx.selectedLayerId);
        if (layer && layer->type() == LayerType::Image) {
            const int cropH = cropHandleAt(event->pos(), ctx);
            if (cropH >= 0) {
                m_activeCropHandle = cropH;
                m_cropStartRect = m_cropRect;
                const QTransform matrix = layer->transform.matrix(layer->contentBounds());
                m_cropDragStartLocal = matrix.inverted().map(docPos);
                event->accept();
                return;
            }
        }
    }

    // Try hit-testing an image layer under the cursor
    if (ctx.selectLayer) {
        // Document search for hit
        const auto& children = ctx.document->rootGroup()->children;
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            Layer* l = it->get();
            if (l && l->visible && !l->locked && l->type() == LayerType::Image) {
                const QTransform m = l->transform.matrix(l->contentBounds());
                const QPointF local = m.inverted().map(docPos);
                if (l->contentBounds().contains(local)) {
                    ctx.selectLayer(l->id());
                    initCropRect(ctx);
                    event->accept();
                    return;
                }
            }
        }
    }
}

void CropTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (m_activeCropHandle < 0 || !ctx.document || ctx.selectedLayerId.isNull())
        return;

    Layer* layer = ctx.document->findLayer(ctx.selectedLayerId);
    if (!layer || layer->type() != LayerType::Image)
        return;

    auto* img = static_cast<ImageLayer*>(layer);
    const QTransform matrix = layer->transform.matrix(layer->contentBounds());
    const QPointF localNow = matrix.inverted().map(docPos);
    const double dx = localNow.x() - m_cropDragStartLocal.x();
    const double dy = localNow.y() - m_cropDragStartLocal.y();

    QRectF r = m_cropStartRect;
    const double maxW = img->naturalWidth;
    const double maxH = img->naturalHeight;

    if (m_activeCropHandle == 8) {
        r.translate(dx, dy);
        if (r.left() < 0) r.moveLeft(0);
        if (r.top() < 0) r.moveTop(0);
        if (r.right() > maxW) r.moveRight(maxW);
        if (r.bottom() > maxH) r.moveBottom(maxH);
    } else {
        double left = r.left();
        double top = r.top();
        double right = r.right();
        double bottom = r.bottom();

        switch (m_activeCropHandle) {
        case 0: left += dx; top += dy; break; // TL
        case 1: right += dx; top += dy; break; // TR
        case 2: right += dx; bottom += dy; break; // BR
        case 3: left += dx; bottom += dy; break; // BL
        case 4: top += dy; break; // T
        case 5: right += dx; break; // R
        case 6: bottom += dy; break; // B
        case 7: left += dx; break; // L
        default: break;
        }

        left = std::clamp(left, 0.0, std::max(0.0, right - 10.0));
        top = std::clamp(top, 0.0, std::max(0.0, bottom - 10.0));
        right = std::clamp(right, left + 10.0, maxW);
        bottom = std::clamp(bottom, top + 10.0, maxH);

        if (m_aspectRatio > 0.0) {
            const double curW = right - left;
            const double curH = bottom - top;
            if (m_activeCropHandle == 4 || m_activeCropHandle == 6) {
                // Vertical drag adjusts width centered
                const double targetW = curH * m_aspectRatio;
                const double diff = (targetW - curW) / 2.0;
                left = std::max(0.0, left - diff);
                right = std::min(maxW, right + diff);
            } else {
                // Adjust height to match aspect
                const double targetH = curW / m_aspectRatio;
                bottom = std::min(maxH, top + targetH);
            }
        }

        r.setCoords(left, top, right, bottom);
    }

    m_cropRect = r;
    if (ctx.requestUpdate)
        ctx.requestUpdate();
    event->accept();
}

void CropTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(docPos); Q_UNUSED(ctx);
    if (event->button() == Qt::LeftButton) {
        m_activeCropHandle = -1;
        event->accept();
    }
}

void CropTool::mouseDoubleClick(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(docPos);
    if (event->button() == Qt::LeftButton) {
        applyCrop(ctx);
        event->accept();
    }
}

void CropTool::keyPress(QKeyEvent* event, const ToolContext& ctx)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        applyCrop(ctx);
        event->accept();
    } else if (event->key() == Qt::Key_Escape) {
        cancel(ctx);
        event->accept();
    }
}

void CropTool::applyCrop(const ToolContext& ctx)
{
    if (!ctx.document || ctx.selectedLayerId.isNull() || m_cropRect.isEmpty()) {
        cancel(ctx);
        return;
    }

    Layer* layer = ctx.document->findLayer(ctx.selectedLayerId);
    if (!layer || layer->type() != LayerType::Image) {
        cancel(ctx);
        return;
    }

    auto* img = static_cast<ImageLayer*>(layer);
    const QImage orig = ctx.document->assets().decodedImage(img->assetId);
    if (orig.isNull()) {
        cancel(ctx);
        return;
    }

    const QRect cropR = m_cropRect.toRect().intersected(orig.rect());
    if (cropR.isEmpty()) {
        cancel(ctx);
        return;
    }

    const QImage cropped = ImageProcessing::cropImage(orig, cropR);
    if (cropped.isNull()) {
        cancel(ctx);
        return;
    }

    const LayerId newAssetId = ctx.document->assets().addImage(cropped);

    // Adjust layer position to preserve document-space center
    const QPointF oldCenter(img->naturalWidth / 2.0, img->naturalHeight / 2.0);
    const QPointF cropCenter = cropR.center();
    const QPointF deltaCenter = cropCenter - oldCenter;

    QTransform rotScale;
    rotScale.rotate(layer->transform.rotationDeg);
    rotScale.scale(layer->transform.scaleX, layer->transform.scaleY);
    const QPointF worldDelta = rotScale.map(deltaCenter);

    AffineTransform newTransform = layer->transform;
    newTransform.position = layer->transform.position + worldDelta;

    const QString cmdName = ctx.i18n ? ctx.i18n->t("editor", "canvas.command.cropImage") : QStringLiteral("Crop Image");

    if (ctx.history) {
        ctx.history->execute(std::make_unique<ModifyImageLayerCommand>(
            *ctx.document, img->id(),
            img->assetId, img->naturalWidth, img->naturalHeight, layer->transform,
            newAssetId, cropped.width(), cropped.height(), newTransform,
            cmdName
        ));
    }

    cancel(ctx);
}

void CropTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!ctx.document || ctx.selectedLayerId.isNull() || m_cropRect.isEmpty())
        return;

    Layer* layer = ctx.document->findLayer(ctx.selectedLayerId);
    if (!layer || layer->type() != LayerType::Image)
        return;

    auto* img = static_cast<ImageLayer*>(layer);
    const QTransform layerToDoc = layer->transform.matrix(layer->contentBounds());
    const QTransform layerToDevice = layerToDoc * ctx.docToDevice;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF fullRect(0, 0, img->naturalWidth, img->naturalHeight);

    QPainterPath darkPath;
    darkPath.setFillRule(Qt::OddEvenFill);
    darkPath.addPolygon(layerToDevice.map(QPolygonF(fullRect)));
    darkPath.addPolygon(layerToDevice.map(QPolygonF(m_cropRect)));
    painter->fillPath(darkPath, QColor(0, 0, 0, 160));

    const QRectF screenCrop = layerToDevice.mapRect(m_cropRect);
    QPen borderPen(Qt::white, 2.0);
    borderPen.setCosmetic(true);
    painter->setPen(borderPen);
    painter->drawRect(screenCrop);

    QPen gridPen(QColor(255, 255, 255, 120), 1.0);
    gridPen.setStyle(Qt::DashLine);
    gridPen.setCosmetic(true);
    painter->setPen(gridPen);

    const double sw = screenCrop.width();
    const double sh = screenCrop.height();
    const double sx = screenCrop.x();
    const double sy = screenCrop.y();

    painter->drawLine(QPointF(sx + sw / 3.0, sy), QPointF(sx + sw / 3.0, sy + sh));
    painter->drawLine(QPointF(sx + 2.0 * sw / 3.0, sy), QPointF(sx + 2.0 * sw / 3.0, sy + sh));
    painter->drawLine(QPointF(sx, sy + sh / 3.0), QPointF(sx + sw, sy + sh / 3.0));
    painter->drawLine(QPointF(sx, sy + 2.0 * sh / 3.0), QPointF(sx + sw, sy + 2.0 * sh / 3.0));

    const double w = m_cropRect.width();
    const double h = m_cropRect.height();
    const double x = m_cropRect.x();
    const double y = m_cropRect.y();

    const QPointF pts[8] = {
        layerToDevice.map(m_cropRect.topLeft()),
        layerToDevice.map(m_cropRect.topRight()),
        layerToDevice.map(m_cropRect.bottomRight()),
        layerToDevice.map(m_cropRect.bottomLeft()),
        layerToDevice.map(QPointF(x + w / 2.0, y)),
        layerToDevice.map(QPointF(x + w, y + h / 2.0)),
        layerToDevice.map(QPointF(x + w / 2.0, y + h)),
        layerToDevice.map(QPointF(x, y + h / 2.0))
    };

    painter->setPen(QPen(Qt::black, 1));
    painter->setBrush(Qt::white);
    for (int i = 0; i < 8; ++i) {
        painter->drawRect(QRectF(pts[i].x() - 5, pts[i].y() - 5, 10, 10));
    }

    painter->restore();
}

void CropTool::cancel(const ToolContext& ctx)
{
    m_cropRect = QRectF();
    m_activeCropHandle = -1;
    if (ctx.switchTool) {
        ctx.switchTool(CanvasTool::Select);
    } else if (ctx.requestUpdate) {
        ctx.requestUpdate();
    }
}

} // namespace cc
