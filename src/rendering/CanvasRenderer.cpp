#include "CanvasRenderer.h"
#include <QRectF>

#include "core/Document.h"
#include "core/layers/Layer.h"

#include <QBrush>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <algorithm>
#include <cmath>
#include "core/image/ImageProcessing.h"

namespace cc {
namespace {

QPainter::CompositionMode compositionMode(BlendMode mode)
{
    switch (mode) {
    case BlendMode::Normal:   return QPainter::CompositionMode_SourceOver;
    case BlendMode::Multiply: return QPainter::CompositionMode_Multiply;
    case BlendMode::Screen:   return QPainter::CompositionMode_Screen;
    case BlendMode::Overlay:  return QPainter::CompositionMode_Overlay;
    case BlendMode::Darken:   return QPainter::CompositionMode_Darken;
    case BlendMode::Lighten:  return QPainter::CompositionMode_Lighten;
    case BlendMode::Add:      return QPainter::CompositionMode_Plus;
    }
    return QPainter::CompositionMode_SourceOver;
}

void drawCheckerboard(QPainter* painter, const QRectF& canvasOnDevice,
                      const RenderOptions& options)
{
    painter->save();
    painter->resetTransform();
    painter->setClipRect(canvasOnDevice);
    painter->fillRect(canvasOnDevice, options.checkerLight);

    // QImage (not QPixmap): the renderer must work headless - the export
    // path renders on worker threads without a GUI application.
    QImage tile(options.checkerSize * 2, options.checkerSize * 2,
                QImage::Format_ARGB32_Premultiplied);
    tile.fill(options.checkerLight);
    {
        QPainter tilePainter(&tile);
        tilePainter.fillRect(0, 0, options.checkerSize, options.checkerSize,
                             options.checkerDark);
        tilePainter.fillRect(options.checkerSize, options.checkerSize,
                             options.checkerSize, options.checkerSize,
                             options.checkerDark);
    }
    painter->fillRect(canvasOnDevice, QBrush(tile));
    painter->restore();
}

void drawShape(QPainter* painter, const ShapeLayer& shape)
{
    if (shape.points.size() < 2)
        return; // no geometry yet (the shape tool arrives in M9)

    QPen pen(shape.stroke, shape.strokeWidth);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);

    switch (shape.kind) {
    case ShapeKind::Line: {
        painter->setBrush(Qt::NoBrush);
        if (shape.points.size() == 2)
            painter->drawLine(shape.points[0], shape.points[1]);
        else
            painter->drawPolyline(shape.points);
        break;
    }
    case ShapeKind::Polygon: {
        painter->setBrush(shape.fill);
        painter->drawPolygon(shape.points);
        break;
    }
    case ShapeKind::Rectangle:
    case ShapeKind::RoundedRect:
    case ShapeKind::Ellipse: {
        const QRectF rect = shape.points.boundingRect();
        painter->setBrush(shape.fill);
        if (shape.kind == ShapeKind::Ellipse)
            painter->drawEllipse(rect);
        else if (shape.kind == ShapeKind::RoundedRect)
            painter->drawRoundedRect(rect, shape.cornerRadius,
                                     shape.cornerRadius);
        else
            painter->drawRect(rect);
        break;
    }
    case ShapeKind::ArrowRight: {
        const QRectF rect = shape.points.boundingRect();
        const double x = rect.left();
        const double y = rect.top();
        const double w = rect.width();
        const double h = rect.height();
        // Seta para a direita: haste retangular + ponta triangular
        const double shaftTop = y + h * 0.28;
        const double shaftBottom = y + h * 0.72;
        const double headStart = x + w * 0.58;

        QPainterPath path;
        path.moveTo(x, shaftTop);
        path.lineTo(headStart, shaftTop);
        path.lineTo(headStart, y);
        path.lineTo(x + w, y + h * 0.5);
        path.lineTo(headStart, y + h);
        path.lineTo(headStart, shaftBottom);
        path.lineTo(x, shaftBottom);
        path.closeSubpath();

        painter->setBrush(shape.fill);
        painter->drawPath(path);
        break;
    }
    case ShapeKind::ArrowCurved: {
        const QRectF rect = shape.points.boundingRect();
        const double x = rect.left();
        const double y = rect.top();
        const double w = rect.width();
        const double h = rect.height();

        // Seta curva dinâmica (swoop arrow) muito usada em thumbnails
        QPainterPath path;
        path.moveTo(x, y + h * 0.9);
        path.cubicTo(x + w * 0.1, y + h * 0.35,
                    x + w * 0.45, y + h * 0.15,
                    x + w * 0.75, y + h * 0.25);
        path.lineTo(x + w * 0.70, y);
        path.lineTo(x + w, y + h * 0.35);
        path.lineTo(x + w * 0.65, y + h * 0.65);
        path.lineTo(x + w * 0.70, y + h * 0.42);
        path.cubicTo(x + w * 0.45, y + h * 0.35,
                    x + w * 0.25, y + h * 0.55,
                    x + w * 0.15, y + h);
        path.closeSubpath();

        painter->setBrush(shape.fill);
        painter->drawPath(path);
        break;
    }
    case ShapeKind::Star: {
        const QRectF rect = shape.points.boundingRect();
        const double cx = rect.center().x();
        const double cy = rect.center().y();
        const double rOuter = std::min(rect.width(), rect.height()) / 2.0;
        const double rInner = rOuter * 0.42;

        QPainterPath path;
        constexpr int numPoints = 5;
        constexpr double angleStep = 3.14159265358979323846 / numPoints;
        double currentAngle = -3.14159265358979323846 / 2.0; // Começa no topo

        for (int i = 0; i < numPoints * 2; ++i) {
            const double r = (i % 2 == 0) ? rOuter : rInner;
            const double px = cx + r * std::cos(currentAngle);
            const double py = cy + r * std::sin(currentAngle);
            if (i == 0)
                path.moveTo(px, py);
            else
                path.lineTo(px, py);
            currentAngle += angleStep;
        }
        path.closeSubpath();

        painter->setBrush(shape.fill);
        painter->drawPath(path);
        break;
    }
    case ShapeKind::Badge: {
        const QRectF rect = shape.points.boundingRect();
        const double cx = rect.center().x();
        const double cy = rect.center().y();
        const double rx = rect.width() / 2.0;
        const double ry = rect.height() / 2.0;

        // Selo / Badge com 12 pontas arredondadas / recortadas estilo burst promocional
        QPainterPath path;
        constexpr int points = 12;
        constexpr double angleStep = 3.14159265358979323846 / points;
        double currentAngle = 0.0;

        for (int i = 0; i < points * 2; ++i) {
            const double rFactor = (i % 2 == 0) ? 1.0 : 0.82;
            const double px = cx + (rx * rFactor) * std::cos(currentAngle);
            const double py = cy + (ry * rFactor) * std::sin(currentAngle);
            if (i == 0)
                path.moveTo(px, py);
            else
                path.lineTo(px, py);
            currentAngle += angleStep;
        }
        path.closeSubpath();

        painter->setBrush(shape.fill);
        painter->drawPath(path);
        break;
    }
    }
}

void drawLayer(QPainter* painter, const Layer& layer, const Document& doc,
               const QRectF& canvasRect, float parentOpacity)
{
    if (!layer.visible)
        return;

    const float effectiveOpacity = parentOpacity * layer.opacity();
    if (effectiveOpacity <= 0.0f)
        return;

    painter->save();
    painter->setOpacity(effectiveOpacity);
    painter->setCompositionMode(compositionMode(layer.blendMode));
    painter->setTransform(layer.transform.matrix(layer.contentBounds()),
                          /*combine=*/true);
    switch (layer.type()) {
    case LayerType::Background: {
        const auto& background = static_cast<const BackgroundLayer&>(layer);
        painter->fillRect(canvasRect, background.fill);
        break;
    }
    case LayerType::Group: {
        const auto& group = static_cast<const GroupLayer&>(layer);
        for (const auto& child : group.children)
            drawLayer(painter, *child, doc, canvasRect, effectiveOpacity);
        break;
    }
    case LayerType::Shape:
        drawShape(painter, static_cast<const ShapeLayer&>(layer));
        break;
    case LayerType::Text: {
        const auto& text = static_cast<const TextLayer&>(layer);
        if (text.content.isEmpty())
            break;

        QFont font(text.fontFamily);
        font.setBold(text.bold);
        font.setItalic(text.italic);
        font.setUnderline(text.underline);
        font.setPointSizeF(text.sizePt > 0 ? text.sizePt : 1.0);

        const bool hasOutline = text.effects.outline.enabled
                                && text.effects.outline.width > 0.0;
        const bool hasShadow = text.effects.shadow.enabled;
        const bool hasGradient = text.effects.gradient.enabled;

        Qt::Alignment alignFlag = Qt::AlignLeft;
        if (text.align == TextAlignment::Center)
            alignFlag = Qt::AlignHCenter;
        else if (text.align == TextAlignment::Right)
            alignFlag = Qt::AlignRight;

        const QRectF drawRect = text.box.isEmpty()
            ? text.contentBounds()
            : QRectF(QPointF(0, 0), text.box);

        if (!hasOutline && !hasShadow && !hasGradient) {
            painter->setFont(font);
            painter->setPen(text.color);
            painter->drawText(drawRect,
                              alignFlag | Qt::AlignTop | Qt::TextWordWrap,
                              text.content);
            break;
        }

        // Effects path: build the glyph outline(s), then draw
        // shadow (blurred silhouette) -> outline stroke -> fill/gradient.
        const QFontMetrics metrics(font);
        const QStringList lines = text.content.split(QLatin1Char('\n'));
        QPainterPath path;
        double lineY = metrics.ascent();
        for (const QString& lineText : lines) {
            double lineX = 0.0;
            const double lw = metrics.horizontalAdvance(lineText);
            if (alignFlag == Qt::AlignHCenter)
                lineX = qMax(0.0, (drawRect.width() - lw) / 2.0);
            else if (alignFlag == Qt::AlignRight)
                lineX = qMax(0.0, drawRect.width() - lw);

            path.addText(QPointF(lineX, lineY), font, lineText);
            lineY += metrics.lineSpacing();
        }

        if (hasShadow) {
            const auto& shadow = text.effects.shadow;
            const QRectF pb = path.boundingRect();
            const int margin = static_cast<int>(shadow.blur * 3) + 8;
            QImage silhouette(static_cast<int>(pb.width()) + 2 * margin + 2,
                              static_cast<int>(pb.height()) + 2 * margin + 2,
                              QImage::Format_ARGB32_Premultiplied);
            silhouette.fill(Qt::transparent);
            {
                QPainter sp(&silhouette);
                sp.setRenderHint(QPainter::Antialiasing);
                sp.translate(margin - pb.left(), margin - pb.top());
                sp.fillPath(path, shadow.color);
            }
            if (shadow.blur > 0.5) {
                const int k = qMax(2, static_cast<int>(shadow.blur));
                QImage small = silhouette.scaled(
                    qMax(1, silhouette.width() / k),
                    qMax(1, silhouette.height() / k),
                    Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                silhouette = small.scaled(silhouette.size(),
                                          Qt::IgnoreAspectRatio,
                                          Qt::SmoothTransformation);
            }
            painter->drawImage(
                pb.topLeft() - QPointF(margin, margin)
                    + QPointF(shadow.offsetX, shadow.offsetY),
                silhouette);
        }

        if (hasOutline) {
            const QPen outlinePen(text.effects.outline.color,
                                  text.effects.outline.width,
                                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter->strokePath(path, outlinePen);
        }

        if (hasGradient) {
            const auto& grad = text.effects.gradient;
            const QRectF b = path.boundingRect();
            if (grad.type == 1) { // Radial
                QRadialGradient rg(b.center(), qMax(b.width(), b.height()) / 2.0);
                rg.setColorAt(0.0, grad.startColor);
                rg.setColorAt(1.0, grad.endColor);
                painter->fillPath(path, rg);
            } else { // Linear
                const double rad = qDegreesToRadians(grad.angleDeg);
                const QPointF c = b.center();
                const double halfDiag = std::hypot(b.width(), b.height()) / 2.0;
                const QPointF p1 = c - QPointF(std::cos(rad) * halfDiag, std::sin(rad) * halfDiag);
                const QPointF p2 = c + QPointF(std::cos(rad) * halfDiag, std::sin(rad) * halfDiag);
                QLinearGradient lg(p1, p2);
                lg.setColorAt(0.0, grad.startColor);
                lg.setColorAt(1.0, grad.endColor);
                painter->fillPath(path, lg);
            }
        } else {
            painter->fillPath(path, text.color);
        }
        break;
    }
    case LayerType::Image: {
        const auto& image = static_cast<const ImageLayer&>(layer);
        const QImage pixels = doc.assets().decodedImage(image.assetId);
        if (!pixels.isNull()) {
            // Efeito Contorno / Glow ("Sticker Effect")
            if (image.effects.outline.enabled && image.effects.outline.width > 0.0) {
                const auto& outline = image.effects.outline;
                const int w = pixels.width();
                const int h = pixels.height();
                const int pad = static_cast<int>(std::ceil(outline.width + outline.blur * 2.0));
                const int effectW = w + pad * 2;
                const int effectH = h + pad * 2;

                // 1. Gera silhueta na cor do efeito baseada no canal alfa da imagem original
                QImage silhouette(effectW, effectH, QImage::Format_ARGB32_Premultiplied);
                silhouette.fill(Qt::transparent);

                // Preenche onde há opacidade na imagem
                QImage mask(effectW, effectH, QImage::Format_ARGB32_Premultiplied);
                mask.fill(Qt::transparent);
                {
                    QPainter mp(&mask);
                    mp.drawImage(QPoint(pad, pad), pixels);
                }

                // Dilatação por amostragem circular para criar borda sólida precisa
                const int radius = static_cast<int>(std::round(outline.width));
                QPainter sp(&silhouette);
                const int steps = std::clamp(radius * 3, 12, 48);
                const double angleDelta = (2.0 * 3.14159265358979323846) / steps;

                // Desenha a máscara com deslocamentos circulares
                for (int r = 1; r <= radius; ++r) {
                    for (int i = 0; i < steps; ++i) {
                        const double ang = i * angleDelta;
                        const double dx = r * std::cos(ang);
                        const double dy = r * std::sin(ang);
                        sp.drawImage(QPointF(dx, dy), mask);
                    }
                }

                // Tingir a silhueta com a cor desejada
                sp.setCompositionMode(QPainter::CompositionMode_SourceIn);
                QColor glowColor = outline.color;
                glowColor.setAlphaF(std::clamp(outline.opacity, 0.0, 1.0) * glowColor.alphaF());
                sp.fillRect(silhouette.rect(), glowColor);
                sp.end();

                // Se houver blur (efeito glow / neon), aplica o desfoque
                if (outline.blur > 0.1) {
                    silhouette = ImageProcessing::applyBlur(silhouette, outline.blur);
                }

                // Desenha o contorno/brilho atrás da imagem original
                painter->drawImage(QPointF(-pad, -pad), silhouette);
            }

            painter->drawImage(QPointF(0, 0), pixels);
        } else {
            // Placeholder while the asset has no decoded pixels.
            painter->fillRect(
                QRectF(0, 0, image.naturalWidth, image.naturalHeight),
                QColor(0x3a, 0x3b, 0x40));
        }
        break;
    }
    }

    painter->restore();
}

} // namespace

void renderDocument(const Document& doc, QPainter* painter,
                    const QTransform& docToDevice, const RenderOptions& options)
{
    const QRectF canvasRect(0, 0, doc.width(), doc.height());
    const QRectF canvasOnDevice = docToDevice.mapRect(canvasRect);

    if (options.drawCheckerboard)
        drawCheckerboard(painter, canvasOnDevice, options);

    painter->save();
    painter->setTransform(docToDevice);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->setClipRect(canvasRect); // layers never draw outside the canvas

    const GroupLayer* root = doc.rootGroup();
    for (const auto& child : root->children)
        drawLayer(painter, *child, doc, canvasRect, 1.0f);
    painter->restore();

    painter->save();
    painter->resetTransform();
    painter->setPen(QPen(options.canvasBorder, 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(canvasOnDevice);
    painter->restore();
}

} // namespace cc
