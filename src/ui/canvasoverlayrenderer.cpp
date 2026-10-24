#include "canvasoverlayrenderer.h"
#include "designtokens.h"

#include <QFontMetrics>
#include <algorithm>

namespace cc {

void CanvasOverlayRenderer::drawSelection(QPainter* painter,
                                          const CanvasHandles& handles,
                                          bool isLocked)
{
    if (!handles.valid)
        return;

    painter->save();
    painter->resetTransform();
    const QColor accent = isLocked ? QColor(0xe7, 0x4c, 0x3c) : Design::Accent();

    QPen pen(accent, 1.0);
    if (isLocked)
        pen.setStyle(Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    QPolygonF box;
    box << handles.points[0] << handles.points[1] << handles.points[2] << handles.points[3];
    painter->drawPolygon(box);

    painter->setPen(QPen(accent, 1.0));
    for (int i = 0; i < 8; ++i) {
        const QRectF rect(handles.points[i] - QPointF(3, 3), QSizeF(6, 6));
        painter->setBrush(isLocked ? QColor(0xff, 0xdd, 0xdd) : Qt::white);
        painter->drawRoundedRect(rect, 2.0, 2.0);
    }
    if (!isLocked) {
        painter->setBrush(accent);
        painter->drawEllipse(handles.points[8], 4, 4);
    }
    painter->restore();
}

void CanvasOverlayRenderer::drawMultiSelection(QPainter* painter,
                                               const QVector<CanvasHandles>& handleSets)
{
    painter->save();
    painter->resetTransform();
    const QColor accent(0x2f, 0x6f, 0xed);
    QPen dashed(accent, 1.0, Qt::DashLine);
    painter->setPen(dashed);
    painter->setBrush(Qt::NoBrush);

    for (const auto& set : handleSets) {
        if (!set.valid)
            continue;
        QPolygonF box;
        box << set.points[0] << set.points[1] << set.points[2] << set.points[3];
        painter->drawPolygon(box);
    }
    painter->restore();
}

void CanvasOverlayRenderer::drawRubberBand(QPainter* painter,
                                           const QPointF& startDocPos,
                                           const QPointF& currentDocPos,
                                           const QTransform& toDevice)
{
    painter->save();
    const QPointF p1 = toDevice.map(startDocPos);
    const QPointF p2 = toDevice.map(currentDocPos);
    const QRectF rect = QRectF(p1, p2).normalized();

    painter->resetTransform();
    QColor selFill = Design::Accent();
    selFill.setAlpha(35);
    painter->setPen(QPen(Design::Accent(), 1.0, Qt::DashLine));
    painter->setBrush(selFill);
    painter->drawRect(rect);
    painter->restore();
}

void CanvasOverlayRenderer::drawGrid(QPainter* painter,
                                     int docW,
                                     int docH,
                                     int gridSpacing,
                                     const QTransform& toDevice)
{
    if (gridSpacing <= 0 || docW <= 0 || docH <= 0)
        return;

    painter->save();
    painter->resetTransform();

    QPen gridPen(QColor(0xff, 0xff, 0xff, 22), 0);
    painter->setPen(gridPen);
    painter->setBrush(Qt::NoBrush);

    for (int x = 0; x <= docW; x += gridSpacing) {
        const QPointF p1 = toDevice.map(QPointF(x, 0));
        const QPointF p2 = toDevice.map(QPointF(x, docH));
        painter->drawLine(p1, p2);
    }
    for (int y = 0; y <= docH; y += gridSpacing) {
        const QPointF p1 = toDevice.map(QPointF(0, y));
        const QPointF p2 = toDevice.map(QPointF(docW, y));
        painter->drawLine(p1, p2);
    }
    painter->restore();
}

void CanvasOverlayRenderer::drawSafeZones(QPainter* painter,
                                          int docW,
                                          int docH,
                                          int safeZoneMode,
                                          const QTransform& toDevice)
{
    if (safeZoneMode == 0 || docW <= 0 || docH <= 0)
        return;

    painter->save();
    painter->resetTransform();

    const double w = double(docW);
    const double h = double(docH);

    QPen zonePen(QColor(0xff, 0x44, 0x44, 160), 1.5, Qt::DashDotLine);
    painter->setPen(zonePen);
    painter->setBrush(Qt::NoBrush);
    QFont labelFont = painter->font();
    labelFont.setPointSize(9);
    labelFont.setBold(true);
    painter->setFont(labelFont);

    auto drawZoneRect = [&](double x, double y, double zw, double zh, const QString& label, const QColor& fillColor = QColor(0, 0, 0, 0)) {
        const QRectF docRect(x, y, zw, zh);
        const QPolygonF screenPoly = toDevice.map(QPolygonF(docRect));
        painter->setPen(zonePen);
        painter->setBrush(fillColor.alpha() == 0 ? Qt::NoBrush : QBrush(fillColor));
        painter->drawPolygon(screenPoly);
        if (!label.isEmpty()) {
            const QPointF labelPos = toDevice.map(QPointF(x + 6, y + 16));
            const QFontMetrics metrics(labelFont);
            const QRectF textRect = metrics.boundingRect(label);
            const QRectF background(labelPos.x() - 3.0, labelPos.y() - metrics.ascent(),
                                    textRect.width() + 6.0, metrics.height());
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(20, 20, 30, 150));
            painter->drawRoundedRect(background, 3.0, 3.0);
            painter->setPen(QColor(0xff, 0x99, 0x99));
            painter->drawText(labelPos, label);
        }
    };

    switch (safeZoneMode) {
    case 1: {
        const double margin = w * 0.05;
        drawZoneRect(margin, margin, w - 2 * margin, h - 2 * margin,
                     QStringLiteral("YouTube Safe Title Margin"));
        const double tsW = 140.0;
        const double tsH = 44.0;
        drawZoneRect(w - tsW - 16, h - tsH - 16, tsW, tsH,
                     QStringLiteral("Timestamp [0:00]"), QColor(255, 68, 68, 45));
        break;
    }
    case 2: {
        if (w != h) {
            const double side = std::min(w, h);
            const double x = (w - side) / 2.0;
            const double y = (h - side) / 2.0;
            drawZoneRect(x, y, side, side, QStringLiteral("Feed Grid (1:1)"), QColor(50, 150, 255, 20));
        }
        const double m = std::min(w, h) * 0.08;
        drawZoneRect(m, m, w - 2 * m, h - 2 * m, QStringLiteral("Instagram Safe"));
        break;
    }
    case 3: {
        const double topH = h * 0.085;
        drawZoneRect(0, 0, w, topH, QStringLiteral("Header / Status"), QColor(255, 68, 68, 40));
        const double botH = h * 0.17;
        drawZoneRect(0, h - botH, w, botH, QStringLiteral("Caption / Sound"), QColor(255, 68, 68, 40));
        const double rightW = w * 0.15;
        drawZoneRect(w - rightW, topH, rightW, h - topH - botH,
                     QStringLiteral("Buttons"), QColor(255, 68, 68, 40));
        const double safeMargin = w * 0.08;
        drawZoneRect(safeMargin, topH + 20, w - rightW - safeMargin * 1.5,
                     h - topH - botH - 40, QStringLiteral("Core Safe Zone"));
        break;
    }
    }
    painter->restore();
}

void CanvasOverlayRenderer::drawMockupPlacement(QPainter* painter,
                                                const QVector<QPointF>& mockupCorners,
                                                const QTransform& toDevice)
{
    painter->save();
    painter->resetTransform();
    QPolygonF screen;
    for (const QPointF& point : mockupCorners)
        screen << toDevice.map(point);

    QColor mkFill = Design::Accent();
    mkFill.setAlpha(32);
    painter->setPen(QPen(Design::Accent(), 2.0, Qt::DashLine));
    painter->setBrush(mkFill);
    if (screen.size() >= 3)
        painter->drawPolygon(screen);
    else if (screen.size() == 2)
        painter->drawLine(screen[0], screen[1]);
    for (int i = 0; i < screen.size(); ++i) {
        painter->setBrush(Qt::white);
        painter->drawEllipse(screen[i], 5, 5);
        painter->setBrush(QColor(0x22, 0xa6, 0xf2));
        painter->drawText(screen[i] + QPointF(8, -8), QString::number(i + 1));
    }
    const QString help = QStringLiteral("MOCKUP: %1 pontos — duplo clique/Enter confirma — botão direito cancela")
        .arg(mockupCorners.size());
    QFont font = painter->font();
    font.setBold(true);
    font.setPointSize(10);
    painter->setFont(font);
    const QRectF message(14, 14, 430, 30);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(15, 23, 42, 220));
    painter->drawRoundedRect(message, 6, 6);
    painter->setPen(Qt::white);
    painter->drawText(message, Qt::AlignCenter, help);
    painter->restore();
}

void CanvasOverlayRenderer::drawSnapGuides(QPainter* painter,
                                           int docW,
                                           int docH,
                                           const QList<GuideLine>& guides,
                                           double zoom,
                                           const QTransform& toDevice)
{
    if (guides.isEmpty() || docW <= 0 || docH <= 0)
        return;

    painter->save();
    painter->resetTransform();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const double marginDoc = 40.0 / std::max(0.01, zoom);

    QPen shadowPen(QColor(0, 0, 0, 140));
    shadowPen.setWidthF(3.0);
    shadowPen.setCosmetic(true);

    QPen guidePen(QColor(0xff, 0x00, 0x7f));
    guidePen.setWidthF(1.5);
    guidePen.setStyle(Qt::DashLine);
    guidePen.setCosmetic(true);

    for (const GuideLine& guide : guides) {
        QPointF p1;
        QPointF p2;

        if (guide.isVertical()) {
            p1 = toDevice.map(QPointF(guide.axisPosition, -marginDoc));
            p2 = toDevice.map(QPointF(guide.axisPosition, double(docH) + marginDoc));
        } else {
            p1 = toDevice.map(QPointF(-marginDoc, guide.axisPosition));
            p2 = toDevice.map(QPointF(double(docW) + marginDoc, guide.axisPosition));
        }

        painter->setPen(shadowPen);
        painter->drawLine(p1, p2);

        painter->setPen(guidePen);
        painter->drawLine(p1, p2);
    }

    painter->restore();
}

void CanvasOverlayRenderer::drawPixelSelectionOutline(QPainter* painter,
                                                      const ImageLayer* layer,
                                                      const QImage& pixelSelection,
                                                      const QImage& sourceImage,
                                                      const QTransform& toDevice)
{
    if (!layer || pixelSelection.isNull() || sourceImage.isNull()
        || pixelSelection.size() != sourceImage.size())
        return;

    QImage selectionOverlay(pixelSelection.size(), QImage::Format_ARGB32);
    selectionOverlay.fill(Qt::transparent);

    for (int y = 0; y < pixelSelection.height(); ++y) {
        QRgb* dst = reinterpret_cast<QRgb*>(selectionOverlay.scanLine(y));
        const uchar* src = pixelSelection.constScanLine(y);
        for (int x = 0; x < pixelSelection.width(); ++x) {
            const uchar here = src[x];
            const uchar left = x > 0 ? src[x - 1] : here;
            const uchar right = x + 1 < pixelSelection.width() ? src[x + 1] : here;
            const uchar up = y > 0 ? pixelSelection.constScanLine(y - 1)[x] : here;
            const uchar down = y + 1 < pixelSelection.height() ? pixelSelection.constScanLine(y + 1)[x] : here;
            const bool edge = qAbs(int(here) - int(left)) > 20
                           || qAbs(int(here) - int(right)) > 20
                           || qAbs(int(here) - int(up)) > 20
                           || qAbs(int(here) - int(down)) > 20;

            if (edge) {
                dst[x] = ((x + y) / 3) % 2
                    ? qRgba(255, 255, 255, 255)
                    : qRgba(0, 0, 0, 255);
            }
        }
    }

    painter->save();
    painter->setTransform(layer->transform.matrix(layer->contentBounds()) * toDevice);
    painter->drawImage(QPointF(0, 0), selectionOverlay);
    painter->restore();
}

} // namespace cc
