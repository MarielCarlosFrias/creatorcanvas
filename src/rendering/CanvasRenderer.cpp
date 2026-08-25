#include "CanvasRenderer.h"

#include "core/Document.h"
#include "core/layers/Layer.h"

#include <QBrush>
#include <QFont>        // <-- adicionei
#include <QPainter>     // <-- adicionei
#include <QPen>   
#include <QPen>
#include <QImage>

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
    }
}

void drawLayer(QPainter* painter, const Layer& layer, const QRectF& canvasRect,
               float parentOpacity)
{
    if (!layer.visible)
        return;

    const float effectiveOpacity = parentOpacity * layer.opacity();
    if (effectiveOpacity <= 0.0f)
        return;

    painter->save();
    painter->setOpacity(effectiveOpacity);
    painter->setCompositionMode(compositionMode(layer.blendMode));

    switch (layer.type()) {
    case LayerType::Background: {
        const auto& background = static_cast<const BackgroundLayer&>(layer);
        painter->fillRect(canvasRect, background.fill);
        break;
    }
    case LayerType::Group: {
        const auto& group = static_cast<const GroupLayer&>(layer);
        for (const auto& child : group.children)
            drawLayer(painter, *child, canvasRect, effectiveOpacity);
        break;
    }
    case LayerType::Shape:
        drawShape(painter, static_cast<const ShapeLayer&>(layer));
        break;
    case LayerType::Text: {
        const auto& text = static_cast<const TextLayer&>(layer);
        if (!text.content.isEmpty()) {
            QFont font(text.fontFamily);
            font.setBold(text.bold);
            font.setItalic(text.italic);
            font.setUnderline(text.underline);
            painter->setFont(font);
            painter->setPen(text.color);
            // Transforms arrive in M6; until then text sits at the doc origin.
            painter->drawText(QPointF(0, 0), text.content);
        }
        break;
    }
    case LayerType::Image: {
        const auto& image = static_cast<const ImageLayer&>(layer);
        // Neutral placeholder until the pixel store exists (M6 import).
        painter->fillRect(QRectF(0, 0, image.naturalWidth, image.naturalHeight),
                          QColor(0x3a, 0x3b, 0x40));
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

    const GroupLayer* root = doc.rootGroup();
    for (const auto& child : root->children)
        drawLayer(painter, *child, canvasRect, 1.0f);
    painter->restore();

    painter->save();
    painter->resetTransform();
    painter->setPen(QPen(options.canvasBorder, 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(canvasOnDevice);
    painter->restore();
}

} // namespace cc
