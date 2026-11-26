#include "SvgExporter.h"

#include "core/Document.h"
#include "core/layers/Layer.h"
#include "rendering/CanvasRenderer.h"

#include <QBuffer>
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QXmlStreamWriter>

#include <cmath>

namespace cc {
namespace {

QString num(double v)
{
    return QString::number(v, 'f', 3);
}

QString colorValue(const QColor& c)
{
    return c.name(QColor::HexRgb); // #rrggbb
}

QString opacityValue(float opacity)
{
    return opacity >= 1.0f ? QString() : num(opacity);
}

QString matrixValue(const AffineTransform& t, const QRectF& bounds)
{
    const QTransform m = t.matrix(bounds);
    return QStringLiteral("matrix(%1,%2,%3,%4,%5,%6)")
        .arg(num(m.m11())).arg(num(m.m12())).arg(num(m.m21()))
        .arg(num(m.m22())).arg(num(m.dx())).arg(num(m.dy()));
}

QString blendModeValue(BlendMode mode)
{
    const char* css = nullptr;
    switch (mode) {
    case BlendMode::Normal: return {};
    case BlendMode::Multiply: css = "multiply"; break;
    case BlendMode::Screen: css = "screen"; break;
    case BlendMode::Overlay: css = "overlay"; break;
    case BlendMode::Darken: css = "darken"; break;
    case BlendMode::Lighten: css = "lighten"; break;
    case BlendMode::Add: css = "plus-lighter"; break;
    case BlendMode::ColorDodge: css = "color-dodge"; break;
    case BlendMode::ColorBurn: css = "color-burn"; break;
    case BlendMode::HardLight: css = "hard-light"; break;
    case BlendMode::SoftLight: css = "soft-light"; break;
    case BlendMode::Difference: css = "difference"; break;
    case BlendMode::Exclusion: css = "exclusion"; break;
    }
    return QStringLiteral("mix-blend-mode:%1").arg(QLatin1String(css));
}

/// Converte um QPainterPath em dados "d" de SVG (M/L/C/Z).
QString pathToSvgData(const QPainterPath& path)
{
    QString d;
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element e = path.elementAt(i);
        switch (e.type) {
        case QPainterPath::MoveToElement:
            d += QStringLiteral("M %1 %2 ").arg(num(e.x)).arg(num(e.y));
            break;
        case QPainterPath::LineToElement:
            d += QStringLiteral("L %1 %2 ").arg(num(e.x)).arg(num(e.y));
            break;
        case QPainterPath::CurveToElement: {
            if (i + 2 >= path.elementCount())
                break;
            const QPainterPath::Element e1 = path.elementAt(i + 1);
            const QPainterPath::Element e2 = path.elementAt(i + 2);
            d += QStringLiteral("C %1 %2 %3 %4 %5 %6 ")
                     .arg(num(e.x)).arg(num(e.y))
                     .arg(num(e1.x)).arg(num(e1.y))
                     .arg(num(e2.x)).arg(num(e2.y));
            i += 2;
            break;
        }
        case QPainterPath::CurveToDataElement:
            break; // consumido pelo CurveTo
        }
    }
    if (path.elementCount() >= 2) {
        const auto f = path.elementAt(0);
        const auto l = path.elementAt(path.elementCount() - 1);
        if (qFuzzyCompare(f.x + 1.0, l.x + 1.0) && qFuzzyCompare(f.y + 1.0, l.y + 1.0))
            d += QStringLiteral("Z ");
    }
    return d;
}

/// Caminho vetorial só para as formas básicas; vazio = usar fallback raster.
QPainterPath basicShapePath(const ShapeLayer& s)
{
    QPainterPath p;
    const QRectF r = s.points.boundingRect();
    switch (s.kind) {
    case ShapeKind::Rectangle:
        p.addRect(r);
        return p;
    case ShapeKind::RoundedRect:
        p.addRoundedRect(r, s.cornerRadius, s.cornerRadius);
        return p;
    case ShapeKind::Ellipse:
        p.addEllipse(r);
        return p;
    case ShapeKind::Line:
        if (s.points.size() >= 2) {
            p.moveTo(s.points.at(0));
            p.lineTo(s.points.at(1));
        }
        return p;
    case ShapeKind::Polygon:
        if (!s.points.isEmpty()) {
            p.addPolygon(s.points);
            p.closeSubpath();
        }
        return p;
    default:
        return {};
    }
}

QByteArray rasterLayerPng(const Document& doc, const LayerId& id)
{
    QImage img(doc.width(), doc.height(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter painter(&img);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        RenderOptions options;
        options.drawCheckerboard = false;
        options.canvasBorder = Qt::transparent;
        renderSiblingLayers(doc, {id}, &painter, options);
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (buffer.open(QIODevice::WriteOnly))
        img.save(&buffer, "PNG");
    buffer.close();
    return bytes;
}

/// Escreve o gradiente como <defs> inline e devolve o valor para fill=.
/// Devolve QString() se o gradiente não estiver ativo (usa cor sólida).
QString writeGradientDefs(QXmlStreamWriter& w, const Gradient& g, const QRectF& bounds,
                          int& defId)
{
    if (!g.enabled || g.stops.isEmpty())
        return {};
    if (g.kind == GradientKind::Conical) {
        // Sem equivalente SVG 1.1: usa o primeiro stop como cor sólida.
        return colorValue(g.stops.first().color);
    }
    const QString id = QStringLiteral("g%1").arg(++defId);
    w.writeStartElement(QStringLiteral("defs"));
    if (g.kind == GradientKind::Linear) {
        const double rad = qDegreesToRadians(g.angleDeg);
        const double cx = bounds.center().x();
        const double cy = bounds.center().y();
        const double len = qMax(bounds.width(), bounds.height()) / 2.0;
        w.writeStartElement(QStringLiteral("linearGradient"));
        w.writeAttribute(QStringLiteral("id"), id);
        w.writeAttribute(QStringLiteral("x1"), num(cx - std::cos(rad) * len));
        w.writeAttribute(QStringLiteral("y1"), num(cy - std::sin(rad) * len));
        w.writeAttribute(QStringLiteral("x2"), num(cx + std::cos(rad) * len));
        w.writeAttribute(QStringLiteral("y2"), num(cy + std::sin(rad) * len));
    } else { // Radial
        w.writeStartElement(QStringLiteral("radialGradient"));
        w.writeAttribute(QStringLiteral("id"), id);
        w.writeAttribute(QStringLiteral("cx"), num(bounds.left() + g.center.x() * bounds.width()));
        w.writeAttribute(QStringLiteral("cy"), num(bounds.top() + g.center.y() * bounds.height()));
        w.writeAttribute(QStringLiteral("r"),
                         num(qMax(bounds.width(), bounds.height()) * g.radius));
    }
    for (const auto& stop : g.stops) {
        w.writeStartElement(QStringLiteral("stop"));
        w.writeAttribute(QStringLiteral("offset"), num(stop.position));
        w.writeAttribute(QStringLiteral("stop-color"), colorValue(stop.color));
        if (stop.color.alpha() < 255)
            w.writeAttribute(QStringLiteral("stop-opacity"), num(stop.color.alpha() / 255.0));
        w.writeEndElement(); // stop
    }
    w.writeEndElement(); // gradient
    w.writeEndElement(); // defs
    return QStringLiteral("url(#%1)").arg(id);
}

struct CommonAttrs {
    float opacity = 1.0f;
    AffineTransform transform;
    QRectF bounds;
    BlendMode blendMode = BlendMode::Normal;
};

void writeCommon(QXmlStreamWriter& w, const CommonAttrs& c)
{
    const QString op = opacityValue(c.opacity);
    if (!op.isEmpty())
        w.writeAttribute(QStringLiteral("opacity"), op);
    const QString m = matrixValue(c.transform, c.bounds);
    if (!m.isEmpty() && m != QLatin1String("matrix(1.000,0.000,0.000,1.000,0.000,0.000)"))
        w.writeAttribute(QStringLiteral("transform"), m);
    const QString bm = blendModeValue(c.blendMode);
    if (!bm.isEmpty())
        w.writeAttribute(QStringLiteral("style"), bm);
}

void writeFillStroke(QXmlStreamWriter& w, const QColor& fill, const Gradient* grad,
                     const QRectF& bounds, const QColor& stroke, double strokeWidth,
                     int& defId)
{
    if (grad) {
        const QString gfill = writeGradientDefs(w, *grad, bounds, defId);
        if (!gfill.isEmpty()) {
            w.writeAttribute(QStringLiteral("fill"), gfill);
            return;
        }
    }
    w.writeAttribute(QStringLiteral("fill"), colorValue(fill));
    if (fill.alpha() < 255)
        w.writeAttribute(QStringLiteral("fill-opacity"), num(fill.alpha() / 255.0));
    Q_UNUSED(stroke);
    Q_UNUSED(strokeWidth);
}

void writeLayer(QXmlStreamWriter& w, const Document& doc, const Layer& layer, int& defId);

void writeRasterFallback(QXmlStreamWriter& w, const Document& doc, const Layer& layer)
{
    const QByteArray png = rasterLayerPng(doc, layer.id());
    if (png.isEmpty())
        return;
    // rasterLayerPng() já desenha a camada em coordenadas do documento,
    // incluindo transform e opacidade. Reaplicar qualquer um dos dois no
    // <image> deslocaria/redimensionaria (ou esbateria) a camada uma segunda
    // vez. O blend fica no elemento para compor com o restante SVG.
    CommonAttrs c{1.0f, AffineTransform{}, QRectF(), layer.blendMode};
    w.writeStartElement(QStringLiteral("image"));
    w.writeAttribute(QStringLiteral("x"), num(0));
    w.writeAttribute(QStringLiteral("y"), num(0));
    w.writeAttribute(QStringLiteral("width"), num(doc.width()));
    w.writeAttribute(QStringLiteral("height"), num(doc.height()));
    w.writeAttribute(QStringLiteral("preserveAspectRatio"), QStringLiteral("none"));
    w.writeAttribute(QStringLiteral("href"),
                     QStringLiteral("data:image/png;base64,")
                         + QString::fromLatin1(png.toBase64()));
    writeCommon(w, c);
    w.writeEndElement();
}

void writeLayer(QXmlStreamWriter& w, const Document& doc, const Layer& layer, int& defId)
{
    if (!layer.visible)
        return;
    const QRectF bounds = layer.contentBounds();
    CommonAttrs c{layer.opacity(), layer.transform, bounds, layer.blendMode};

    switch (layer.type()) {
    case LayerType::Group: {
        w.writeStartElement(QStringLiteral("g"));
        writeCommon(w, c);
        for (const auto& child : static_cast<const GroupLayer&>(layer).children)
            writeLayer(w, doc, *child, defId);
        w.writeEndElement();
        return;
    }
    case LayerType::Background: {
        const auto& bg = static_cast<const BackgroundLayer&>(layer);
        w.writeStartElement(QStringLiteral("rect"));
        w.writeAttribute(QStringLiteral("x"), num(0));
        w.writeAttribute(QStringLiteral("y"), num(0));
        w.writeAttribute(QStringLiteral("width"), num(doc.width()));
        w.writeAttribute(QStringLiteral("height"), num(doc.height()));
        writeFillStroke(w, bg.fill, &bg.gradient,
                        QRectF(0, 0, doc.width(), doc.height()), Qt::transparent, 0, defId);
        w.writeEndElement();
        return;
    }
    case LayerType::Fill: {
        const auto& fl = static_cast<const FillLayer&>(layer);
        w.writeStartElement(QStringLiteral("rect"));
        w.writeAttribute(QStringLiteral("x"), num(0));
        w.writeAttribute(QStringLiteral("y"), num(0));
        w.writeAttribute(QStringLiteral("width"), num(doc.width()));
        w.writeAttribute(QStringLiteral("height"), num(doc.height()));
        const QColor fill = fl.kind == FillType::Solid ? fl.color : Qt::transparent;
        writeFillStroke(w, fill, fl.kind == FillType::Gradient ? &fl.gradient : nullptr,
                        QRectF(0, 0, doc.width(), doc.height()), Qt::transparent, 0, defId);
        w.writeAttribute(QStringLiteral("opacity"), num(layer.opacity() * layer.fillOpacity()));
        w.writeEndElement();
        return;
    }
    case LayerType::Shape: {
        const auto& s = static_cast<const ShapeLayer&>(layer);
        const QPainterPath p = basicShapePath(s);
        if (p.isEmpty()) {
            writeRasterFallback(w, doc, layer); // formas complexas → raster
            return;
        }
        w.writeStartElement(p.elementCount() == 2 && s.kind == ShapeKind::Line
                                ? QStringLiteral("line") : QStringLiteral("path"));
        if (s.kind == ShapeKind::Line) {
            w.writeAttribute(QStringLiteral("x1"), num(s.points.at(0).x()));
            w.writeAttribute(QStringLiteral("y1"), num(s.points.at(0).y()));
            w.writeAttribute(QStringLiteral("x2"), num(s.points.at(1).x()));
            w.writeAttribute(QStringLiteral("y2"), num(s.points.at(1).y()));
            w.writeAttribute(QStringLiteral("fill"), QStringLiteral("none"));
        } else {
            w.writeAttribute(QStringLiteral("d"), pathToSvgData(p));
            writeFillStroke(w, s.fill, s.gradient.enabled ? &s.gradient : nullptr,
                            bounds, s.stroke, s.strokeWidth, defId);
        }
        if (s.stroke.alpha() > 0 && s.strokeWidth > 0) {
            w.writeAttribute(QStringLiteral("stroke"), colorValue(s.stroke));
            w.writeAttribute(QStringLiteral("stroke-width"), num(s.strokeWidth));
        } else if (s.kind != ShapeKind::Line) {
            w.writeAttribute(QStringLiteral("stroke"), QStringLiteral("none"));
        }
        writeCommon(w, c);
        w.writeEndElement();
        return;
    }
    case LayerType::Vector: {
        const auto& v = static_cast<const VectorLayer&>(layer);
        if (v.path.isEmpty()) {
            writeRasterFallback(w, doc, layer);
            return;
        }
        w.writeStartElement(QStringLiteral("path"));
        w.writeAttribute(QStringLiteral("d"), pathToSvgData(v.path));
        if (v.fillEnabled)
            w.writeAttribute(QStringLiteral("fill"), colorValue(v.fill));
        else
            w.writeAttribute(QStringLiteral("fill"), QStringLiteral("none"));
        if (v.strokeEnabled && v.stroke.alpha() > 0 && v.strokeWidth > 0) {
            w.writeAttribute(QStringLiteral("stroke"), colorValue(v.stroke));
            w.writeAttribute(QStringLiteral("stroke-width"), num(v.strokeWidth));
        } else {
            w.writeAttribute(QStringLiteral("stroke"), QStringLiteral("none"));
        }
        writeCommon(w, c);
        w.writeEndElement();
        return;
    }
    case LayerType::Text: {
        const auto& t = static_cast<const TextLayer&>(layer);
        w.writeStartElement(QStringLiteral("text"));
        // O texto é definido no espaço LOCAL da camada, tal como no renderer.
        // transform.position pertence apenas à matriz comum; usá-lo também em
        // x/y aplicava a translação duas vezes.
        double localX = bounds.center().x();
        if (t.align == TextAlignment::Left)
            localX = bounds.left();
        else if (t.align == TextAlignment::Right)
            localX = bounds.right();
        w.writeAttribute(QStringLiteral("x"), num(localX));
        w.writeAttribute(QStringLiteral("y"), num(bounds.center().y()));
        w.writeAttribute(QStringLiteral("font-family"), t.fontFamily);
        w.writeAttribute(QStringLiteral("font-size"), num(t.sizePt));
        if (t.bold)
            w.writeAttribute(QStringLiteral("font-weight"), QStringLiteral("bold"));
        if (t.italic)
            w.writeAttribute(QStringLiteral("font-style"), QStringLiteral("italic"));
        if (t.underline)
            w.writeAttribute(QStringLiteral("text-decoration"), QStringLiteral("underline"));
        w.writeAttribute(QStringLiteral("fill"), colorValue(t.color));
        if (t.color.alpha() < 255)
            w.writeAttribute(QStringLiteral("fill-opacity"), num(t.color.alpha() / 255.0));
        const char* anchor = t.align == TextAlignment::Left ? "start"
                           : t.align == TextAlignment::Right ? "end" : "middle";
        w.writeAttribute(QStringLiteral("text-anchor"), QLatin1String(anchor));
        w.writeAttribute(QStringLiteral("dominant-baseline"), QStringLiteral("central"));
        writeCommon(w, c);
        w.writeCharacters(t.content);
        w.writeEndElement();
        return;
    }
    case LayerType::Image:
    case LayerType::Adjustment:
    default:
        // Imagens e ajustes (e qualquer outro tipo) vão como raster embebido.
        writeRasterFallback(w, doc, layer);
        return;
    }
}

} // namespace

bool exportDocumentToSvg(const Document& doc, const QString& filePath,
                         double scale, QString* errorMessage)
{
    const double outputScale = std::isfinite(scale) && scale > 0.0 ? scale : 1.0;
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not open the SVG file for writing.");
        return false;
    }

    QXmlStreamWriter w(&file);
    w.setAutoFormatting(true);
    w.writeStartDocument(QStringLiteral("1.0"));
    w.writeStartElement(QStringLiteral("svg"));
    w.writeAttribute(QStringLiteral("xmlns"), QStringLiteral("http://www.w3.org/2000/svg"));
    // O viewBox conserva as coordenadas originais; width/height aplicam a
    // mesma escala apresentada no diálogo sem transformar os vectores em raster.
    w.writeAttribute(QStringLiteral("width"), num(doc.width() * outputScale));
    w.writeAttribute(QStringLiteral("height"), num(doc.height() * outputScale));
    w.writeAttribute(QStringLiteral("viewBox"),
                     QStringLiteral("0 0 %1 %2").arg(num(doc.width())).arg(num(doc.height())));

    int defId = 0;
    for (const auto& child : doc.rootGroup()->children)
        writeLayer(w, doc, *child, defId);

    w.writeEndElement(); // svg
    w.writeEndDocument();
    file.close();

    if (file.error() != QFile::NoError || w.hasError()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed while writing the SVG document.");
        return false;
    }
    return true;
}

bool exportDocumentToSvg(const Document& doc, const QString& filePath,
                         QString* errorMessage)
{
    return exportDocumentToSvg(doc, filePath, 1.0, errorMessage);
}

} // namespace cc
