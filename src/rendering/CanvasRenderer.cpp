#include "CanvasRenderer.h"
#include <QRectF>

#include "core/Document.h"
#include "core/layers/Layer.h"

#include <QBrush>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <algorithm>
#include <cmath>
#include "core/image/ImageProcessing.h"

namespace cc {

namespace {

/// Converte o Gradient do documento em QBrush sobre a area dada.
QBrush gradientBrush(const Gradient& g, const QRectF& rect)
{
    if (g.stops.isEmpty())
        return QBrush(Qt::black);
    QVector<GradientStop> stops = g.stops;
    std::sort(stops.begin(), stops.end(),
              [](const GradientStop& a, const GradientStop& b) {
                  return a.position < b.position;
              });
    if (g.kind == GradientKind::Radial) {
        const QPointF c(rect.left() + rect.width() * g.center.x(),
                        rect.top() + rect.height() * g.center.y());
        const double r = qMax(1.0, qMax(rect.width(), rect.height()) * g.radius);
        QRadialGradient qg(c, r);
        for (const auto& s : stops)
            qg.setColorAt(qBound(0.0, double(s.position), 1.0), s.color);
        return QBrush(qg);
    }
    const QPointF c = rect.center();
    const double rad = qDegreesToRadians(g.angleDeg);
    const QPointF dir(std::cos(rad), std::sin(rad));
    const double half = (rect.width() * std::abs(std::cos(rad))
                         + rect.height() * std::abs(std::sin(rad))) / 2.0;
    QLinearGradient qg(c - dir * half, c + dir * half);
    for (const auto& s : stops)
        qg.setColorAt(qBound(0.0, double(s.position), 1.0), s.color);
    return QBrush(qg);
}

} // namespace

namespace {

QPainter::CompositionMode compositionMode(BlendMode mode)
{
    switch (mode) {
    case BlendMode::Normal:     return QPainter::CompositionMode_SourceOver;
    case BlendMode::Multiply:   return QPainter::CompositionMode_Multiply;
    case BlendMode::Screen:     return QPainter::CompositionMode_Screen;
    case BlendMode::Overlay:    return QPainter::CompositionMode_Overlay;
    case BlendMode::Darken:     return QPainter::CompositionMode_Darken;
    case BlendMode::Lighten:    return QPainter::CompositionMode_Lighten;
    case BlendMode::Add:        return QPainter::CompositionMode_Plus;
    case BlendMode::ColorDodge: return QPainter::CompositionMode_ColorDodge;
    case BlendMode::ColorBurn:  return QPainter::CompositionMode_ColorBurn;
    case BlendMode::HardLight:  return QPainter::CompositionMode_HardLight;
    case BlendMode::SoftLight:  return QPainter::CompositionMode_SoftLight;
    case BlendMode::Difference: return QPainter::CompositionMode_Difference;
    case BlendMode::Exclusion:  return QPainter::CompositionMode_Exclusion;
    }
    return QPainter::CompositionMode_SourceOver;
}

QImage applyBlendIf(const QImage& source, const BlendIf& blendIf)
{
    if (source.isNull() || blendIf.isDefault())
        return source;

    const BlendRange grayRange = blendIf.get(0)[0]; // This layer Gray
    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const int w = result.width();
    const int h = result.height();

    for (int y = 0; y < h; ++y) {
        QRgb* scan = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb pixel = scan[x];
            const int a = qAlpha(pixel);
            if (a == 0)
                continue;

            // Perceived luminance (Rec. 601)
            const float lum = 0.299f * qRed(pixel) + 0.587f * qGreen(pixel) + 0.114f * qBlue(pixel);
            const float wFactor = grayRange.weight(lum);

            if (wFactor <= 0.0f) {
                scan[x] = 0;
            } else if (wFactor < 1.0f) {
                const int newAlpha = static_cast<int>(std::round(a * wFactor));
                scan[x] = qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), newAlpha);
            }
        }
    }
    return result;
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

    // Preenchimento: gradiente quando ativo, cor solida caso contrario.
    const QBrush fillBrush = shape.gradient.isValid()
        ? gradientBrush(shape.gradient, shape.points.boundingRect())
        : QBrush(shape.fill);

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
        painter->setBrush(fillBrush);
        painter->drawPolygon(shape.points);
        break;
    }
    case ShapeKind::Rectangle:
    case ShapeKind::RoundedRect:
    case ShapeKind::Ellipse: {
        const QRectF rect = shape.points.boundingRect();
        painter->setBrush(fillBrush);
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

        painter->setBrush(fillBrush);
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

        painter->setBrush(fillBrush);
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

        painter->setBrush(fillBrush);
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

        painter->setBrush(fillBrush);
        painter->drawPath(path);
        break;
    }
    case ShapeKind::SpeechBubble: {
        const QRectF rect = shape.points.boundingRect();
        const double x = rect.left();
        const double y = rect.top();
        const double w = rect.width();
        const double h = rect.height();

        const double bodyH = h * 0.8;
        const double cr = (shape.cornerRadius > 0)
            ? std::min({shape.cornerRadius, w / 2.0, bodyH / 2.0})
            : std::min(w, bodyH) * 0.18;

        QPainterPath path;
        path.moveTo(x + cr, y);
        path.lineTo(x + w - cr, y);
        path.arcTo(x + w - 2 * cr, y, 2 * cr, 2 * cr, 90, -90);
        path.lineTo(x + w, y + bodyH - cr);
        path.arcTo(x + w - 2 * cr, y + bodyH - 2 * cr, 2 * cr, 2 * cr, 0, -90);
        path.lineTo(x + w * 0.45, y + bodyH);
        path.lineTo(x + w * 0.20, y + h);
        path.lineTo(x + w * 0.25, y + bodyH);
        path.lineTo(x + cr, y + bodyH);
        path.arcTo(x, y + bodyH - 2 * cr, 2 * cr, 2 * cr, 270, -90);
        path.lineTo(x, y + cr);
        path.arcTo(x, y, 2 * cr, 2 * cr, 180, -90);
        path.closeSubpath();

        painter->setBrush(fillBrush);
        painter->drawPath(path);
        break;
    }
    case ShapeKind::Heart: {
        const QRectF rect = shape.points.boundingRect();
        const double x = rect.left();
        const double y = rect.top();
        const double w = rect.width();
        const double h = rect.height();

        const double midX = x + w * 0.5;
        const double topY = y + h * 0.25;
        const double botY = y + h;

        QPainterPath path;
        path.moveTo(midX, topY);
        path.cubicTo(midX - w * 0.05, y,
                     x, y,
                     x, y + h * 0.35);
        path.cubicTo(x, y + h * 0.65,
                     midX - w * 0.2, y + h * 0.85,
                     midX, botY);
        path.cubicTo(midX + w * 0.2, y + h * 0.85,
                     x + w, y + h * 0.65,
                     x + w, y + h * 0.35);
        path.cubicTo(x + w, y,
                     midX + w * 0.05, y,
                     midX, topY);
        path.closeSubpath();

        painter->setBrush(fillBrush);
        painter->drawPath(path);
        break;
    }
    }
}

struct OutlineCacheKey {
    LayerId assetId;
    QRgb color = 0;
    int width10 = 0;
    int blur10 = 0;
    int opacity100 = 0;
    int naturalW = 0;
    int naturalH = 0;

    bool operator==(const OutlineCacheKey& o) const {
        return assetId == o.assetId
            && color == o.color
            && width10 == o.width10
            && blur10 == o.blur10
            && opacity100 == o.opacity100
            && naturalW == o.naturalW
            && naturalH == o.naturalH;
    }
};

inline size_t qHash(const OutlineCacheKey& k, size_t seed = 0) {
    return ::qHash(k.assetId.toString(), seed)
        ^ ::qHash(k.color)
        ^ ::qHash(k.width10)
        ^ ::qHash(k.blur10)
        ^ ::qHash(k.opacity100)
        ^ ::qHash(k.naturalW)
        ^ ::qHash(k.naturalH);
}

struct CachedOutline {
    QImage silhouette;
    double pad = 0.0;
};

static QHash<OutlineCacheKey, CachedOutline> s_outlineCache;

void renderImageOutline(QPainter* painter, const ImageLayer& image, const Document& doc)
{
    const auto& outline = image.effects.outline;
    OutlineCacheKey key;
    key.assetId = image.assetId;
    key.color = outline.color.rgba();
    key.width10 = static_cast<int>(std::round(outline.width * 10.0));
    key.blur10 = static_cast<int>(std::round(outline.blur * 10.0));
    key.opacity100 = static_cast<int>(std::round(outline.opacity * 100.0));
    key.naturalW = image.naturalWidth;
    key.naturalH = image.naturalHeight;

    auto it = s_outlineCache.find(key);
    if (it != s_outlineCache.end()) {
        const CachedOutline& cached = it.value();
        painter->drawImage(QRectF(-cached.pad, -cached.pad,
                                  image.naturalWidth + cached.pad * 2.0,
                                  image.naturalHeight + cached.pad * 2.0),
                           cached.silhouette);
        return;
    }

    const QImage fullImg = doc.assets().decodedImage(image.assetId);
    if (fullImg.isNull()) return;

    // Geração acelerada: downscale para no máximo 1280px se a textura for gigante
    const int maxDim = 1280;
    double scaleDown = 1.0;
    QImage baseImg = fullImg;
    if (fullImg.width() > maxDim || fullImg.height() > maxDim) {
        baseImg = fullImg.scaled(maxDim, maxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaleDown = static_cast<double>(baseImg.width()) / static_cast<double>(fullImg.width());
    }

    const double scaledWidth = std::max(1.0, outline.width * scaleDown);
    const double scaledBlur = outline.blur * scaleDown;
    const int pad = static_cast<int>(std::ceil(scaledWidth + scaledBlur * 2.0));
    const int effectW = baseImg.width() + pad * 2;
    const int effectH = baseImg.height() + pad * 2;

    QImage silhouette(effectW, effectH, QImage::Format_ARGB32_Premultiplied);
    silhouette.fill(Qt::transparent);

    QImage mask(effectW, effectH, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    {
        QPainter mp(&mask);
        mp.drawImage(QPoint(pad, pad), baseImg);
    }

    const int radius = static_cast<int>(std::round(scaledWidth));
    QPainter sp(&silhouette);
    const int steps = std::clamp(radius * 3, 12, 36);
    const double angleDelta = (2.0 * 3.14159265358979323846) / steps;

    for (int r = 1; r <= radius; ++r) {
        for (int i = 0; i < steps; ++i) {
            const double ang = i * angleDelta;
            const double dx = r * std::cos(ang);
            const double dy = r * std::sin(ang);
            sp.drawImage(QPointF(dx, dy), mask);
        }
    }

    sp.setCompositionMode(QPainter::CompositionMode_SourceIn);
    QColor glowColor = outline.color;
    glowColor.setAlphaF(std::clamp(outline.opacity, 0.0, 1.0) * glowColor.alphaF());
    sp.fillRect(silhouette.rect(), glowColor);
    sp.end();

    if (scaledBlur > 0.1) {
        silhouette = ImageProcessing::applyBlur(silhouette, scaledBlur);
    }

    const double unscaledPad = pad / scaleDown;

    if (s_outlineCache.size() > 32)
        s_outlineCache.clear();

    CachedOutline entry;
    entry.silhouette = silhouette;
    entry.pad = unscaledPad;
    s_outlineCache.insert(key, entry);

    painter->drawImage(QRectF(-unscaledPad, -unscaledPad,
                              image.naturalWidth + unscaledPad * 2.0,
                              image.naturalHeight + unscaledPad * 2.0),
                       silhouette);
}

void renderImageDropShadow(QPainter* painter, const ImageLayer& image, const Document& doc)
{
    const auto& shadow = image.effects.dropShadow;
    const QImage fullImg = doc.assets().decodedImage(image.assetId);
    if (fullImg.isNull()) return;

    const double rad = qDegreesToRadians(shadow.angleDeg);
    const double offsetX = shadow.distance * std::cos(rad);
    const double offsetY = shadow.distance * std::sin(rad);

    const int maxDim = 1000;
    double scaleDown = 1.0;
    QImage baseImg = fullImg;
    if (fullImg.width() > maxDim || fullImg.height() > maxDim) {
        baseImg = fullImg.scaled(maxDim, maxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaleDown = static_cast<double>(baseImg.width()) / static_cast<double>(fullImg.width());
    }

    const double blurScaled = shadow.blur * scaleDown;
    const int margin = static_cast<int>(std::ceil(blurScaled * 2.0 + 4.0));
    QImage silhouette(baseImg.width() + margin * 2, baseImg.height() + margin * 2,
                      QImage::Format_ARGB32_Premultiplied);
    silhouette.fill(Qt::transparent);

    {
        QPainter sp(&silhouette);
        sp.drawImage(QPoint(margin, margin), baseImg);
        sp.setCompositionMode(QPainter::CompositionMode_SourceIn);
        QColor shadowColor = shadow.color;
        shadowColor.setAlphaF(std::clamp(shadow.opacity, 0.0, 1.0) * shadowColor.alphaF());
        sp.fillRect(silhouette.rect(), shadowColor);
    }

    if (blurScaled > 0.5) {
        silhouette = ImageProcessing::applyBlur(silhouette, blurScaled);
    }

    const double unscaledMargin = margin / scaleDown;
    const QRectF destRect(-unscaledMargin + offsetX,
                          -unscaledMargin + offsetY,
                          image.naturalWidth + unscaledMargin * 2.0,
                          image.naturalHeight + unscaledMargin * 2.0);
    painter->drawImage(destRect, silhouette);
}

void applyImageInnerShadow(QImage& target, const ImageInnerShadow& shadow)
{
    if (target.isNull() || !shadow.enabled || shadow.opacity <= 0.0)
        return;

    const double rad = qDegreesToRadians(shadow.angleDeg);
    const double offsetX = shadow.distance * std::cos(rad);
    const double offsetY = shadow.distance * std::sin(rad);

    // Passo 1: Obter a máscara alfa invertida (tudo que está FORA da forma é opaco)
    // Conforme PhotoCraft `crates/compose/src/effects.rs` shadow_map (inner: shape.map(|a| 1.0 - a)):
    const int w = target.width();
    const int h = target.height();

    const double blurRadius = shadow.blur;
    const int margin = static_cast<int>(std::ceil(blurRadius * 2.0 + 8.0));

    // Canvas estendido com a silhueta externa invertida deslocada
    QImage outsideMask(w + margin * 2, h + margin * 2, QImage::Format_ARGB32_Premultiplied);
    outsideMask.fill(QColor(0, 0, 0, 255)); // Tudo fora é 255

    {
        QPainter mp(&outsideMask);
        mp.setCompositionMode(QPainter::CompositionMode_Clear);
        mp.drawImage(QPoint(margin, margin), target);
    }

    // Deslocar a máscara conforme o ângulo/distância da luz
    QImage shifted(outsideMask.size(), outsideMask.format());
    shifted.fill(Qt::transparent);
    {
        QPainter sp(&shifted);
        sp.drawImage(QPointF(offsetX, offsetY), outsideMask);
    }

    if (blurRadius > 0.5) {
        shifted = ImageProcessing::applyBlur(shifted, blurRadius);
    }

    // Colorir a sombra com a cor e opacidade do efeito
    {
        QPainter cp(&shifted);
        cp.setCompositionMode(QPainter::CompositionMode_SourceIn);
        QColor shadowColor = shadow.color;
        shadowColor.setAlphaF(std::clamp(shadow.opacity, 0.0, 1.0) * shadowColor.alphaF());
        cp.fillRect(shifted.rect(), shadowColor);
    }

    // Recortar a sombra interna para dentro da forma da imagem e pintar por cima (Multiply / Normal)
    QPainter p(&target);
    p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    p.drawImage(QPoint(-margin, -margin), shifted);
}


void drawLayer(QPainter* painter, const Layer& layer, const Document& doc,
               const QRectF& canvasRect, float parentOpacity,
               const RenderOptions& options)
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
        if (background.gradient.isValid())
            painter->fillRect(canvasRect,
                              gradientBrush(background.gradient, canvasRect));
        else
            painter->fillRect(canvasRect, background.fill);
        break;
    }
    case LayerType::Group: {
        const auto& group = static_cast<const GroupLayer&>(layer);
        for (const auto& child : group.children)
            drawLayer(painter, *child, doc, canvasRect, effectiveOpacity, options);
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
        if (text.letterSpacingPx != 0.0) {
            font.setLetterSpacing(QFont::AbsoluteSpacing, text.letterSpacingPx);
        }

        const bool hasOutline = text.effects.outline.enabled
                                && text.effects.outline.width > 0.0;
        const bool hasShadow = text.effects.shadow.enabled;
        const bool hasGradient = text.effects.gradient.enabled;

        const bool hasTexture = text.effects.texture.enabled
                                && !text.effects.texture.assetId.isNull();

        Qt::Alignment alignFlag = Qt::AlignLeft;
        if (text.align == TextAlignment::Center)
            alignFlag = Qt::AlignHCenter;
        else if (text.align == TextAlignment::Right)
            alignFlag = Qt::AlignRight;

        const QRectF drawRect = text.box.isEmpty()
            ? text.contentBounds()
            : QRectF(QPointF(0, 0), text.box);

        const bool isHollow = text.effects.hollow;
        const bool isKnockout = text.effects.knockout;
        const bool customLineHeight = !qFuzzyCompare(text.lineHeightMult, 1.0) && text.lineHeightMult > 0.0;

        if (!hasOutline && !hasShadow && !hasGradient && !hasTexture && !isHollow && !isKnockout && !customLineHeight) {
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
        QStringList lines = text.content.split(QLatin1Char('\n'));
        // Envolver palavras quando houver caixa fixa — sem isso o texto com
        // efeitos ignorava a caixa e transbordava (o caminho simples usa
        // TextWordWrap via drawText). Palavra mais larga que a caixa
        // transborda, igual ao comportamento do drawText.
        if (!text.box.isEmpty() && drawRect.width() > 0.0) {
            QStringList wrapped;
            wrapped.reserve(lines.size());
            for (const QString& paragraph : lines) {
                const QStringList words =
                    paragraph.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (words.isEmpty()) {
                    wrapped.append(QString());
                    continue;
                }
                QString current;
                for (const QString& word : words) {
                    const QString candidate =
                        current.isEmpty() ? word
                                          : current + QLatin1Char(' ') + word;
                    if (current.isEmpty()
                        || metrics.horizontalAdvance(candidate) <= drawRect.width()) {
                        current = candidate;
                    } else {
                        wrapped.append(current);
                        current = word;
                    }
                }
                wrapped.append(current);
            }
            lines = std::move(wrapped);
        }
        const double lineSpacing = metrics.lineSpacing() * (text.lineHeightMult > 0.0 ? text.lineHeightMult : 1.0);
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
            lineY += lineSpacing;
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

        // Knockout / Chroma Cut mode: remove as camadas de trás no formato do texto
        if (isKnockout) {
            painter->setCompositionMode(QPainter::CompositionMode_Clear);
            painter->fillPath(path, Qt::transparent);
            painter->setCompositionMode(compositionMode(layer.blendMode));
        }

        if (hasGradient && !isHollow) {
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
        } else if (hasTexture && !isHollow) {
            const QImage texImg = options.interactive
                ? doc.assets().previewImage(text.effects.texture.assetId)
                : doc.assets().decodedImage(text.effects.texture.assetId);
            if (!texImg.isNull()) {
                const QRectF b = path.boundingRect();
                painter->save();
                painter->setClipPath(path, Qt::ReplaceClip);
                const double texOpacity = std::clamp(text.effects.texture.opacity, 0.0, 1.0);
                if (texOpacity < 1.0) {
                    painter->setOpacity(painter->opacity() * texOpacity);
                }
                painter->drawImage(b, texImg);
                painter->restore();
            } else {
                painter->fillPath(path, text.color);
            }
        } else if (!isHollow) {
            if (text.color.alpha() > 0) {
                painter->fillPath(path, text.color);
            }
        }

        // Desenha o contorno por cima da cor/transparência para acabamento perfeito
        if (hasOutline) {
            const QPen outlinePen(text.effects.outline.color,
                                  text.effects.outline.width,
                                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter->strokePath(path, outlinePen);
        }
        break;
    }
    case LayerType::Image: {
        const auto& image = static_cast<const ImageLayer&>(layer);
        const QImage pixels = (options.previewLayerId == image.id() && !options.previewImage.isNull())
            ? options.previewImage
            : options.interactive
            ? doc.assets().previewImage(image.assetId)
            : doc.assets().decodedImage(image.assetId);
        if (!pixels.isNull()) {
            QImage maskedPixels = pixels;
            if (!image.alphaMask.isNull() && image.alphaMask.size() == pixels.size()) {
                const qint64 sourceKey = static_cast<qint64>(pixels.cacheKey());
                const qint64 maskKey = static_cast<qint64>(image.alphaMask.cacheKey());
                if (image.maskedRenderCache.isNull()
                    || image.maskedRenderSourceKey != sourceKey
                    || image.maskedRenderMaskKey != maskKey
                    || image.maskedRenderCache.size() != pixels.size()) {
                    // We write straight RGBA values below. Using a premultiplied
                    // buffer here corrupts partially masked pixels (white/black
                    // halos and blocks), so keep the non-premultiplied format.
                    QImage composed = pixels.convertToFormat(QImage::Format_ARGB32);
                    const QImage mask = image.alphaMask.convertToFormat(QImage::Format_Alpha8);
                    for (int y = 0; y < composed.height(); ++y) {
                        QRgb* dst = reinterpret_cast<QRgb*>(composed.scanLine(y));
                        const uchar* src = mask.constScanLine(y);
                        for (int x = 0; x < composed.width(); ++x)
                            dst[x] = qRgba(qRed(dst[x]), qGreen(dst[x]), qBlue(dst[x]),
                                           (qAlpha(dst[x]) * src[x]) / 255);
                    }
                    image.maskedRenderCache = std::move(composed);
                    image.maskedRenderSourceKey = sourceKey;
                    image.maskedRenderMaskKey = maskKey;
                }
                maskedPixels = image.maskedRenderCache;
            }
            if (!image.blendIf.isDefault()) {
                maskedPixels = applyBlendIf(maskedPixels, image.blendIf);
            }

            const QRectF targetRect(0, 0, image.naturalWidth, image.naturalHeight);

            // Efeito Sombra Projetada (Drop Shadow)
            if (image.effects.dropShadow.enabled) {
                renderImageDropShadow(painter, image, doc);
            }

            // Efeito Contorno / Glow ("Sticker Effect")
            if (image.effects.outline.enabled && image.effects.outline.width > 0.0) {
                renderImageOutline(painter, image, doc);
            }

            // Efeito Sobreposição de Cor (Color Overlay - Photoshop Style)
            if (image.effects.colorOverlay.enabled && image.effects.colorOverlay.opacity > 0.0) {
                QImage tinted = maskedPixels.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                QPainter tip(&tinted);
                tip.setCompositionMode(QPainter::CompositionMode_SourceIn);
                QColor overlayCol = image.effects.colorOverlay.color;
                overlayCol.setAlphaF(std::clamp(image.effects.colorOverlay.opacity, 0.0, 1.0) * overlayCol.alphaF());
                tip.fillRect(tinted.rect(), overlayCol);
                tip.end();

                QPainter blendP(&maskedPixels);
                blendP.drawImage(0, 0, tinted);
            }

            // Efeito Sombra Interna (Inner Shadow - Photoshop / PhotoCraft Style)
            if (image.effects.innerShadow.enabled && image.effects.innerShadow.opacity > 0.0) {
                applyImageInnerShadow(maskedPixels, image.effects.innerShadow);
            }

            painter->drawImage(targetRect, maskedPixels);
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
    if (!options.interactive) {
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    }
    painter->setClipRect(canvasRect); // layers never draw outside the canvas

    const GroupLayer* root = doc.rootGroup();
    for (const auto& child : root->children)
        drawLayer(painter, *child, doc, canvasRect, 1.0f, options);
    painter->restore();

    painter->save();
    painter->resetTransform();
    painter->setPen(QPen(options.canvasBorder, 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(canvasOnDevice);
    painter->restore();
}

void renderSiblingLayers(const Document& doc, const QList<QUuid>& layerIds,
                         QPainter* painter, const RenderOptions& options)
{
    if (!painter || layerIds.isEmpty())
        return;

    const QRectF canvasRect(0, 0, doc.width(), doc.height());
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setClipRect(canvasRect);

    // Keep document paint order, rather than the row-click order.
    GroupLayer* parent = doc.parentOf(layerIds.first());
    if (parent == doc.rootGroup()) {
        for (const auto& child : parent->children) {
            if (layerIds.contains(child->id()))
                drawLayer(painter, *child, doc, canvasRect, 1.0f, options);
        }
    } else if (parent) {
        for (const auto& child : parent->children) {
            if (layerIds.contains(child->id()))
                drawLayer(painter, *child, doc, canvasRect, 1.0f, options);
        }
    }
    painter->restore();
}

} // namespace cc
