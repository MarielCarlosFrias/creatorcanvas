#include "PaintExtras.h"

#include <algorithm>
#include <cmath>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include "ToolCommit.h"
#include "core/selection/SelectionOperations.h"
#include "core/image/ImageProcessing.h"
#include "localization/i18nservice.h"

namespace cc {

namespace {

QString trOr(const ToolContext& ctx, const char* key, const QString& fallback)
{
    if (!ctx.i18n)
        return fallback;
    const QString keyName = QString::fromLatin1(key);
    const QString text = ctx.i18n->t("editor", keyName);
    const QString missMarker = QStringLiteral("editor.") + keyName;
    return (text.isEmpty() || text == missMarker) ? fallback : text;
}

/// Localiza a camada de imagem ativa (somente selecionada — previsível).
ImageLayer* activeImageLayer(const ToolContext& ctx)
{
    if (!ctx.document || ctx.selectedLayerId.isNull())
        return nullptr;
    Layer* layer = ctx.document->findLayer(ctx.selectedLayerId);
    if (!layer || layer->type() != LayerType::Image)
        return nullptr;
    return static_cast<ImageLayer*>(layer);
}

QPointF toLocal(const ImageLayer* img, const QPointF& docPos)
{
    const QRectF bounds = img->contentBounds();
    if (bounds.isEmpty())
        return docPos;
    return img->transform.matrix(bounds).inverted().map(docPos);
}

} // namespace

// ============================================================================
// GradientTool
// ============================================================================

void GradientTool::activate(const ToolContext& ctx)
{
    m_dragging = false;
    if (ctx.requestStatusMessage) {
        ctx.requestStatusMessage(trOr(ctx, "canvas.status.gradientHelp",
            QStringLiteral("Gradient: drag on the canvas to set direction. Shift snaps to 45°. Works on the active image layer.")));
    }
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void GradientTool::deactivate(const ToolContext& ctx)
{
    cancel(ctx);
}

void GradientTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (event->button() != Qt::LeftButton || !ctx.document)
        return;

    // Photocraft: locks granulares — pixels trancados bloqueiam a edição.
    ImageLayer* img = activeImageLayer(ctx);
    if (!img || !SelectionOperations::canEditPixels(*ctx.document, img->id())) {
        reportPixelsLocked(ctx);
        event->accept();
        return;
    }
    if (!img) {
        if (ctx.requestStatusMessage) {
            ctx.requestStatusMessage(trOr(ctx, "canvas.status.gradientNoLayer",
                QStringLiteral("Gradient: select an image layer first (it fills the layer bitmap).")));
        }
        event->accept();
        return;
    }

    if (ctx.document->assets().decodedImage(img->assetId).isNull()) {
        if (ctx.requestStatusMessage)
            ctx.requestStatusMessage(trOr(ctx, "canvas.status.gradientNoBitmap",
                QStringLiteral("Gradient: this layer has no bitmap to fill.")));
        event->accept();
        return;
    }

    m_layerId = img->id();
    m_origAssetId = img->assetId;
    m_origWidth = img->naturalWidth;
    m_origHeight = img->naturalHeight;
    m_origTransform = img->transform;
    m_dragging = true;
    m_startDoc = docPos;
    m_endDoc = docPos;
    m_lastDoc = docPos;

    if (ctx.requestUpdate)
        ctx.requestUpdate();
    event->accept();
}

void GradientTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    m_lastDoc = docPos;
    if (!m_dragging)
        return;

    QPointF end = docPos;
    if (event && event->modifiers().testFlag(Qt::ShiftModifier)) {
        // Snap do ângulo em passos de 45°
        const QPointF delta = docPos - m_startDoc;
        const double length = std::hypot(delta.x(), delta.y());
        double angle = std::atan2(delta.y(), delta.x());
        constexpr double step = 3.14159265358979323846 / 4.0;
        angle = std::round(angle / step) * step;
        end = m_startDoc + QPointF(std::cos(angle) * length, std::sin(angle) * length);
    }
    m_endDoc = end;
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void GradientTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    if (!m_dragging || !ctx.document)
        return;
    m_dragging = false;
    m_endDoc = docPos;

    Layer* layer = ctx.document->findLayer(m_layerId);
    if (!layer || layer->type() != LayerType::Image) {
        cancel(ctx);
        return;
    }
    auto* img = static_cast<ImageLayer*>(layer);

    const QImage bitmap = ctx.document->assets().decodedImage(m_origAssetId);
    if (bitmap.isNull()) {
        cancel(ctx);
        return;
    }

    QImage mask = ctx.document->pixelSelection();
    if (!mask.isNull() && mask.size() != bitmap.size())
        mask = QImage(); // seleção em espaço divergente: ignora em vez de errar
    if (!mask.isNull())
        mask = mask.convertToFormat(QImage::Format_Alpha8);

    const QPointF localStart = toLocal(img, m_startDoc);
    const QPointF localEnd = toLocal(img, m_endDoc);

    const QImage result = ImageProcessing::paintGradient(
        bitmap, localStart, localEnd, m_startColor, m_endColor, m_radial,
        m_opacity, mask);
    if (!result.isNull() && result != bitmap) {
        commitImageBitmap(ctx, m_layerId, m_origAssetId, m_origWidth, m_origHeight,
                          m_origTransform, result,
                          trOr(ctx, "canvas.command.gradient", QStringLiteral("Gradient")));
    }

    if (ctx.requestStatusMessage) {
        ctx.requestStatusMessage(trOr(ctx, "canvas.status.gradientDone",
            QStringLiteral("Gradient applied.")));
    }
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void GradientTool::keyPress(QKeyEvent* event, const ToolContext& ctx)
{
    if (event->key() == Qt::Key_Escape && m_dragging) {
        cancel(ctx);
        event->accept();
    }
}

void GradientTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!m_dragging)
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QPointF start = ctx.docToDevice.map(m_startDoc);
    const QPointF end = ctx.docToDevice.map(m_endDoc);

    QPen pen(QColor(255, 255, 255, 230), 1.5, Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    if (m_radial) {
        const double r = QLineF(start, end).length();
        painter->drawEllipse(start, r, r);
    } else {
        painter->drawLine(start, end);
    }

    painter->setPen(QPen(QColor(0, 0, 0, 140), 1.0, Qt::DotLine));
    if (m_radial) {
        const double r = QLineF(start, end).length();
        painter->drawEllipse(start, r + 1.0, r + 1.0);
    } else {
        painter->drawLine(start + QPointF(0, 1), end + QPointF(0, 1));
    }

    // Handles
    painter->setPen(QPen(QColor(20, 20, 20), 1.0));
    painter->setBrush(QColor(255, 255, 255));
    painter->drawEllipse(start, 4.0, 4.0);
    painter->setBrush(QColor(61, 126, 255));
    painter->drawEllipse(end, 4.0, 4.0);

    painter->restore();
}

void GradientTool::cancel(const ToolContext& ctx)
{
    m_dragging = false;
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

// ============================================================================
// RetouchTool
// ============================================================================

void RetouchTool::activate(const ToolContext& ctx)
{
    m_active = false;
    if (ctx.requestStatusMessage) {
        ctx.requestStatusMessage(trOr(ctx, "canvas.status.retouchHelp",
            QStringLiteral("Retouch: drag over the image. Choose the mode, size and strength in the options bar.")));
    }
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void RetouchTool::deactivate(const ToolContext& ctx)
{
    cancel(ctx);
}

void RetouchTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (event->button() != Qt::LeftButton || !ctx.document)
        return;
    // Photocraft: locks granulares — pixels trancados bloqueiam a edição.
    ImageLayer* img = activeImageLayer(ctx);
    if (!img || !SelectionOperations::canEditPixels(*ctx.document, img->id())) {
        reportPixelsLocked(ctx);
        event->accept();
        return;
    }
    if (!img) {
        if (ctx.requestStatusMessage) {
            ctx.requestStatusMessage(trOr(ctx, "canvas.status.retouchNoLayer",
                QStringLiteral("Retouch: select an image layer first.")));
        }
        event->accept();
        return;
    }
    const QImage bitmap = ctx.document->assets().decodedImage(img->assetId);
    if (bitmap.isNull())
        return;

    m_layerId = img->id();
    m_origAssetId = img->assetId;
    m_origWidth = img->naturalWidth;
    m_origHeight = img->naturalHeight;
    m_origTransform = img->transform;
    m_workingImage = bitmap;
    m_active = true;
    m_currentDoc = docPos;

    const QPointF local = toLocal(img, docPos);
    m_prevLocal = local;
    applyAt(local, local, ctx);
    event->accept();
}

void RetouchTool::applyAt(const QPointF& fromLocal, const QPointF& toLocal, const ToolContext& ctx)
{
    if (!m_active || m_workingImage.isNull())
        return;
    m_workingImage = ImageProcessing::retouchStroke(
        m_workingImage, fromLocal, toLocal, m_mode, m_size, m_strength);
    if (ctx.setLayerPreview)
        ctx.setLayerPreview(m_layerId, m_workingImage);
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void RetouchTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    m_currentDoc = docPos;
    m_hoverValid = true;
    if (!m_active || !ctx.document)
        return;
    Layer* layer = ctx.document->findLayer(m_layerId);
    if (!layer || layer->type() != LayerType::Image)
        return;
    const QPointF local = toLocal(static_cast<ImageLayer*>(layer), docPos);
    applyAt(m_prevLocal, local, ctx);
    m_prevLocal = local;
}

void RetouchTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    Q_UNUSED(docPos);
    if (!m_active || m_workingImage.isNull()) {
        m_active = false;
        return;
    }
    m_active = false;
    commitImageBitmap(ctx, m_layerId, m_origAssetId, m_origWidth, m_origHeight,
                      m_origTransform, m_workingImage,
                      trOr(ctx, "canvas.command.retouch", QStringLiteral("Retouch")));
    m_workingImage = QImage();
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void RetouchTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!m_hoverValid)
        return; // sem cursor fantasma em (0,0)
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QPointF p = ctx.docToDevice.map(m_currentDoc);
    const double radius = std::max(2.0, (m_size / 2.0) * ctx.zoom);
    painter->setPen(QPen(QColor(255, 255, 255, 210), 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(p, radius, radius);
    painter->setPen(QPen(QColor(0, 0, 0, 120), 1.0, Qt::DotLine));
    painter->drawEllipse(p, radius + 1.0, radius + 1.0);
    painter->restore();
}

void RetouchTool::cancel(const ToolContext& ctx)
{
    if (ctx.clearLayerPreview)
        ctx.clearLayerPreview();
    m_active = false;
    m_workingImage = QImage();
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

// ============================================================================
// HealingTool
// ============================================================================

void HealingTool::activate(const ToolContext& ctx)
{
    m_active = false;
    if (ctx.requestStatusMessage) {
        ctx.requestStatusMessage(
            m_mode == Mode::RedEye
                ? trOr(ctx, "canvas.status.redEyeHelp",
                       QStringLiteral("Red Eye: click the red pupil to neutralize it."))
                : trOr(ctx, "canvas.status.healingHelp",
                       QStringLiteral("Spot Healing: paint over the blemish — the repair is sampled from nearby pixels.")));
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
    if (event->button() != Qt::LeftButton || !ctx.document)
        return;
    // Photocraft: locks granulares — pixels trancados bloqueiam a edição.
    ImageLayer* img = activeImageLayer(ctx);
    if (!img || !SelectionOperations::canEditPixels(*ctx.document, img->id())) {
        reportPixelsLocked(ctx);
        event->accept();
        return;
    }
    if (!img) {
        if (ctx.requestStatusMessage) {
            ctx.requestStatusMessage(trOr(ctx, "canvas.status.retouchNoLayer",
                QStringLiteral("Healing: select an image layer first.")));
        }
        event->accept();
        return;
    }
    const QImage bitmap = ctx.document->assets().decodedImage(img->assetId);
    if (bitmap.isNull())
        return;

    m_layerId = img->id();
    m_origAssetId = img->assetId;
    m_origWidth = img->naturalWidth;
    m_origHeight = img->naturalHeight;
    m_origTransform = img->transform;
    m_workingImage = bitmap;
    m_active = true;
    m_currentDoc = docPos;

    // Origem automática do reparo: à direita, dentro dos limites do bitmap.
    m_sourceOffset = QPoint(m_radius * 2 + 2, 0);
    const QPoint local = toLocal(img, docPos).toPoint();
    const QPoint src = local + m_sourceOffset;
    if (src.x() >= bitmap.width() || src.x() < 0)
        m_sourceOffset = QPoint(-m_sourceOffset.x(), m_sourceOffset.y());

    applyAt(toLocal(img, docPos), true, ctx);
    event->accept();
}

void HealingTool::applyAt(const QPointF& local, bool firstSample, const ToolContext& ctx)
{
    Q_UNUSED(firstSample);
    if (!m_active || m_workingImage.isNull())
        return;

    if (m_mode == Mode::RedEye) {
        m_workingImage = ImageProcessing::redEyeRemove(
            m_workingImage, local.toPoint(), m_radius);
    } else {
        const int w = m_workingImage.width();
        const int h = m_workingImage.height();
        QPoint src = local.toPoint() + m_sourceOffset;
        src.setX(std::clamp(src.x(), 0, std::max(0, w - 1)));
        src.setY(std::clamp(src.y(), 0, std::max(0, h - 1)));
        m_workingImage = ImageProcessing::spotHeal(
            m_workingImage, local.toPoint(), src, m_radius, m_strength);
    }
    if (ctx.setLayerPreview)
        ctx.setLayerPreview(m_layerId, m_workingImage);
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void HealingTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    m_currentDoc = docPos;
    m_hoverValid = true;
    if (!m_active || !ctx.document)
        return;
    Layer* layer = ctx.document->findLayer(m_layerId);
    if (!layer || layer->type() != LayerType::Image)
        return;
    applyAt(toLocal(static_cast<ImageLayer*>(layer), docPos), false, ctx);
}

void HealingTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    Q_UNUSED(docPos);
    if (!m_active || m_workingImage.isNull()) {
        m_active = false;
        return;
    }
    m_active = false;
    commitImageBitmap(ctx, m_layerId, m_origAssetId, m_origWidth, m_origHeight,
                      m_origTransform, m_workingImage,
                      m_mode == Mode::RedEye
                          ? trOr(ctx, "canvas.command.redEye", QStringLiteral("Red Eye"))
                          : trOr(ctx, "canvas.command.healing", QStringLiteral("Spot Healing")));
    m_workingImage = QImage();
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

void HealingTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!m_hoverValid)
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QPointF p = ctx.docToDevice.map(m_currentDoc);
    const double radius = std::max(2.0, m_radius * ctx.zoom);
    painter->setPen(QPen(QColor(255, 255, 255, 210), 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(p, radius, radius);
    painter->setPen(QPen(QColor(0, 0, 0, 120), 1.0, Qt::DotLine));
    painter->drawEllipse(p, radius + 1.0, radius + 1.0);
    painter->restore();
}

void HealingTool::cancel(const ToolContext& ctx)
{
    if (ctx.clearLayerPreview)
        ctx.clearLayerPreview();
    m_active = false;
    m_workingImage = QImage();
    if (ctx.requestUpdate)
        ctx.requestUpdate();
}

} // namespace cc
