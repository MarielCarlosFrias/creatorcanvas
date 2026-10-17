#include "ProjectFile.h"

#include "core/Document.h"
#include "core/layers/Layer.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QBuffer>
#include <QImage>
#include <QLoggingCategory>
#include <QCryptographicHash>

#include <miniz.h>

#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace cc {

namespace {

Q_LOGGING_CATEGORY(lcSerialization, "cc.serialization")

constexpr auto kEntryName   = "project.json";
constexpr auto kFormatKey   = "formatVersion";
constexpr auto kAppKey      = "app";
constexpr auto kDocumentKey = "document";
constexpr auto kLayersKey   = "layers";
constexpr auto kAssetsKey   = "assets";

constexpr mz_uint kMaxProjectEntries           = 1000;
constexpr mz_uint64 kMaxSingleAssetSize        = 64ULL * 1024ULL * 1024ULL;  // 64 MB
constexpr mz_uint64 kMaxTotalUncompressedBytes = 256ULL * 1024ULL * 1024ULL; // 256 MB

QString blendModeToString(BlendMode mode)
{
    switch (mode) {
    case BlendMode::Normal:     return QStringLiteral("normal");
    case BlendMode::Multiply:   return QStringLiteral("multiply");
    case BlendMode::Screen:     return QStringLiteral("screen");
    case BlendMode::Overlay:    return QStringLiteral("overlay");
    case BlendMode::Darken:     return QStringLiteral("darken");
    case BlendMode::Lighten:    return QStringLiteral("lighten");
    case BlendMode::Add:        return QStringLiteral("add");
    case BlendMode::ColorDodge: return QStringLiteral("colorDodge");
    case BlendMode::ColorBurn:  return QStringLiteral("colorBurn");
    case BlendMode::HardLight:  return QStringLiteral("hardLight");
    case BlendMode::SoftLight:  return QStringLiteral("softLight");
    case BlendMode::Difference: return QStringLiteral("difference");
    case BlendMode::Exclusion:  return QStringLiteral("exclusion");
    }
    return QStringLiteral("normal");
}

std::optional<BlendMode> blendModeFromString(const QString& value)
{
    if (value == QLatin1String("normal"))     return BlendMode::Normal;
    if (value == QLatin1String("multiply"))   return BlendMode::Multiply;
    if (value == QLatin1String("screen"))     return BlendMode::Screen;
    if (value == QLatin1String("overlay"))    return BlendMode::Overlay;
    if (value == QLatin1String("darken"))     return BlendMode::Darken;
    if (value == QLatin1String("lighten"))    return BlendMode::Lighten;
    if (value == QLatin1String("add"))        return BlendMode::Add;
    if (value == QLatin1String("colorDodge")) return BlendMode::ColorDodge;
    if (value == QLatin1String("colorBurn"))  return BlendMode::ColorBurn;
    if (value == QLatin1String("hardLight"))  return BlendMode::HardLight;
    if (value == QLatin1String("softLight"))  return BlendMode::SoftLight;
    if (value == QLatin1String("difference")) return BlendMode::Difference;
    if (value == QLatin1String("exclusion"))  return BlendMode::Exclusion;
    return std::nullopt;
}

QString shapeKindToString(ShapeKind kind)
{
    switch (kind) {
    case ShapeKind::Rectangle:   return QStringLiteral("rectangle");
    case ShapeKind::RoundedRect: return QStringLiteral("roundedRect");
    case ShapeKind::Ellipse:     return QStringLiteral("ellipse");
    case ShapeKind::Line:        return QStringLiteral("line");
    case ShapeKind::Polygon:     return QStringLiteral("polygon");
    case ShapeKind::ArrowRight:  return QStringLiteral("arrowRight");
    case ShapeKind::ArrowCurved: return QStringLiteral("arrowCurved");
    case ShapeKind::Star:        return QStringLiteral("star");
    case ShapeKind::Badge:       return QStringLiteral("badge");
    case ShapeKind::SpeechBubble: return QStringLiteral("speechBubble");
    case ShapeKind::Heart:       return QStringLiteral("heart");
    case ShapeKind::Triangle:    return QStringLiteral("triangle");
    case ShapeKind::Diamond:     return QStringLiteral("diamond");
    case ShapeKind::Pentagon:    return QStringLiteral("pentagon");
    case ShapeKind::Hexagon:     return QStringLiteral("hexagon");
    case ShapeKind::Cross:       return QStringLiteral("cross");
    case ShapeKind::Checkmark:   return QStringLiteral("checkmark");
    case ShapeKind::Cloud:       return QStringLiteral("cloud");
    case ShapeKind::Explosion:   return QStringLiteral("explosion");
    }
    return QStringLiteral("rectangle");
}

std::optional<ShapeKind> shapeKindFromString(const QString& value)
{
    if (value == QLatin1String("rectangle"))    return ShapeKind::Rectangle;
    if (value == QLatin1String("roundedRect"))  return ShapeKind::RoundedRect;
    if (value == QLatin1String("ellipse"))      return ShapeKind::Ellipse;
    if (value == QLatin1String("line"))         return ShapeKind::Line;
    if (value == QLatin1String("polygon"))      return ShapeKind::Polygon;
    if (value == QLatin1String("arrowRight"))   return ShapeKind::ArrowRight;
    if (value == QLatin1String("arrowCurved"))  return ShapeKind::ArrowCurved;
    if (value == QLatin1String("star"))         return ShapeKind::Star;
    if (value == QLatin1String("badge"))        return ShapeKind::Badge;
    if (value == QLatin1String("speechBubble")) return ShapeKind::SpeechBubble;
    if (value == QLatin1String("heart"))        return ShapeKind::Heart;
    if (value == QLatin1String("triangle"))     return ShapeKind::Triangle;
    if (value == QLatin1String("diamond"))      return ShapeKind::Diamond;
    if (value == QLatin1String("pentagon"))     return ShapeKind::Pentagon;
    if (value == QLatin1String("hexagon"))      return ShapeKind::Hexagon;
    if (value == QLatin1String("cross"))        return ShapeKind::Cross;
    if (value == QLatin1String("checkmark"))    return ShapeKind::Checkmark;
    if (value == QLatin1String("cloud"))        return ShapeKind::Cloud;
    if (value == QLatin1String("explosion"))    return ShapeKind::Explosion;
    return std::nullopt;
}

QString textAlignToString(TextAlignment align)
{
    switch (align) {
    case TextAlignment::Left:   return QStringLiteral("left");
    case TextAlignment::Center: return QStringLiteral("center");
    case TextAlignment::Right:  return QStringLiteral("right");
    }
    return QStringLiteral("center");
}

QString layerColorTagToString(LayerColorTag tag)
{
    switch (tag) {
    case LayerColorTag::None:   return QStringLiteral("none");
    case LayerColorTag::Red:    return QStringLiteral("red");
    case LayerColorTag::Orange: return QStringLiteral("orange");
    case LayerColorTag::Yellow: return QStringLiteral("yellow");
    case LayerColorTag::Green:  return QStringLiteral("green");
    case LayerColorTag::Blue:   return QStringLiteral("blue");
    case LayerColorTag::Purple: return QStringLiteral("purple");
    case LayerColorTag::Gray:   return QStringLiteral("gray");
    }
    return QStringLiteral("none");
}

std::optional<LayerColorTag> layerColorTagFromString(const QString& value)
{
    if (value == QLatin1String("red"))    return LayerColorTag::Red;
    if (value == QLatin1String("orange")) return LayerColorTag::Orange;
    if (value == QLatin1String("yellow")) return LayerColorTag::Yellow;
    if (value == QLatin1String("green"))  return LayerColorTag::Green;
    if (value == QLatin1String("blue"))   return LayerColorTag::Blue;
    if (value == QLatin1String("purple")) return LayerColorTag::Purple;
    if (value == QLatin1String("gray"))   return LayerColorTag::Gray;
    return LayerColorTag::None;
}

std::optional<TextAlignment> textAlignFromString(const QString& value)
{
    if (value == QLatin1String("left"))   return TextAlignment::Left;
    if (value == QLatin1String("center")) return TextAlignment::Center;
    if (value == QLatin1String("right"))  return TextAlignment::Right;
    return std::nullopt;
}

QString colorToString(const QColor& color)
{
    return color.alpha() == 255 ? color.name(QColor::HexRgb)
                                : color.name(QColor::HexArgb);
}

QColor colorFromString(const QString& value)
{
    const QColor color(value);
    return color.isValid() ? color : QColor(0, 0, 0, 0);
}

QJsonObject gradientToJson(const Gradient& g)
{
    QJsonObject o;
    o.insert(QStringLiteral("enabled"), g.enabled);
    o.insert(QStringLiteral("kind"),
             g.kind == GradientKind::Radial ? QStringLiteral("radial")
                                            : QStringLiteral("linear"));
    o.insert(QStringLiteral("angle"), g.angleDeg);
    o.insert(QStringLiteral("centerX"), g.center.x());
    o.insert(QStringLiteral("centerY"), g.center.y());
    o.insert(QStringLiteral("radius"), g.radius);
    QJsonArray stops;
    for (const GradientStop& s : g.stops) {
        QJsonArray pair;
        pair.append(double(s.position));
        pair.append(colorToString(s.color));
        stops.append(pair);
    }
    o.insert(QStringLiteral("stops"), stops);
    return o;
}

Gradient gradientFromJson(const QJsonObject& o)
{
    Gradient g;
    g.enabled = o.value(QStringLiteral("enabled")).toBool(false);
    g.kind = o.value(QStringLiteral("kind")).toString() == QLatin1String("radial")
                 ? GradientKind::Radial
                 : GradientKind::Linear;
    g.angleDeg = o.value(QStringLiteral("angle")).toDouble(90.0);
    g.center = QPointF(o.value(QStringLiteral("centerX")).toDouble(0.5),
                       o.value(QStringLiteral("centerY")).toDouble(0.5));
    g.radius = o.value(QStringLiteral("radius")).toDouble(0.75);
    const QJsonArray stops = o.value(QStringLiteral("stops")).toArray();
    for (const QJsonValue& stop : stops) {
        const QJsonArray pair = stop.toArray();
        if (pair.size() == 2) {
            GradientStop stop;
            stop.position = static_cast<float>(pair.at(0).toDouble());
            stop.color = colorFromString(pair.at(1).toString());
            g.stops.append(stop);
        }
    }
    return g;
}


QJsonObject writeLayer(const Layer& layer)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), layer.id().toString(QUuid::WithoutBraces));
    o.insert(QStringLiteral("name"), layer.name);
    o.insert(QStringLiteral("visible"), layer.visible);
    o.insert(QStringLiteral("locked"), layer.locked);
    o.insert(QStringLiteral("colorTag"), layerColorTagToString(layer.colorTag));
    o.insert(QStringLiteral("opacity"), double(layer.opacity()));
    o.insert(QStringLiteral("blendMode"), blendModeToString(layer.blendMode));
    o.insert(QStringLiteral("transformX"), layer.transform.position.x());
    o.insert(QStringLiteral("transformY"), layer.transform.position.y());
    o.insert(QStringLiteral("rotation"), layer.transform.rotationDeg);
    o.insert(QStringLiteral("scaleX"), layer.transform.scaleX);
    o.insert(QStringLiteral("scaleY"), layer.transform.scaleY);
    o.insert(QStringLiteral("shearX"), layer.transform.shearX);
    o.insert(QStringLiteral("shearY"), layer.transform.shearY);

    if (!layer.blendIf.isDefault()) {
        QJsonArray rangesArr;
        for (const auto& pair : layer.blendIf.ranges) {
            QJsonObject rangeObj;
            rangeObj.insert(QStringLiteral("thisBlackLo"), pair[0].black[0]);
            rangeObj.insert(QStringLiteral("thisBlackHi"), pair[0].black[1]);
            rangeObj.insert(QStringLiteral("thisWhiteLo"), pair[0].white[0]);
            rangeObj.insert(QStringLiteral("thisWhiteHi"), pair[0].white[1]);
            rangeObj.insert(QStringLiteral("underBlackLo"), pair[1].black[0]);
            rangeObj.insert(QStringLiteral("underBlackHi"), pair[1].black[1]);
            rangeObj.insert(QStringLiteral("underWhiteLo"), pair[1].white[0]);
            rangeObj.insert(QStringLiteral("underWhiteHi"), pair[1].white[1]);
            rangesArr.append(rangeObj);
        }
        o.insert(QStringLiteral("blendIf"), rangesArr);
    }

    switch (layer.type()) {
    case LayerType::Group: {
        o.insert(QStringLiteral("type"), QStringLiteral("group"));
        const auto& group = static_cast<const GroupLayer&>(layer);
        o.insert(QStringLiteral("isExpanded"), group.isExpanded);
        QJsonArray children;
        for (const auto& child : group.children)
            children.append(writeLayer(*child));
        o.insert(QStringLiteral("children"), children);
        break;
    }
    case LayerType::Image: {
        o.insert(QStringLiteral("type"), QStringLiteral("image"));
        const auto& image = static_cast<const ImageLayer&>(layer);
        o.insert(QStringLiteral("assetId"),
                 image.assetId.toString(QUuid::WithoutBraces));
        o.insert(QStringLiteral("naturalWidth"), image.naturalWidth);
        o.insert(QStringLiteral("naturalHeight"), image.naturalHeight);
        if (!image.alphaMask.isNull()) {
            o.insert(QStringLiteral("alphaMask"),
                     QStringLiteral("masks/%1.png")
                         .arg(image.id().toString(QUuid::WithoutBraces)));
        }

        QJsonObject effects;
        QJsonObject outline;
        outline.insert(QStringLiteral("enabled"), image.effects.outline.enabled);
        outline.insert(QStringLiteral("color"), colorToString(image.effects.outline.color));
        outline.insert(QStringLiteral("width"), image.effects.outline.width);
        outline.insert(QStringLiteral("blur"), image.effects.outline.blur);
        outline.insert(QStringLiteral("opacity"), image.effects.outline.opacity);
        effects.insert(QStringLiteral("outline"), outline);

        QJsonObject dropShadow;
        dropShadow.insert(QStringLiteral("enabled"), image.effects.dropShadow.enabled);
        dropShadow.insert(QStringLiteral("color"), colorToString(image.effects.dropShadow.color));
        dropShadow.insert(QStringLiteral("distance"), image.effects.dropShadow.distance);
        dropShadow.insert(QStringLiteral("angleDeg"), image.effects.dropShadow.angleDeg);
        dropShadow.insert(QStringLiteral("blur"), image.effects.dropShadow.blur);
        dropShadow.insert(QStringLiteral("opacity"), image.effects.dropShadow.opacity);
        effects.insert(QStringLiteral("dropShadow"), dropShadow);

        QJsonObject innerShadow;
        innerShadow.insert(QStringLiteral("enabled"), image.effects.innerShadow.enabled);
        innerShadow.insert(QStringLiteral("color"), colorToString(image.effects.innerShadow.color));
        innerShadow.insert(QStringLiteral("distance"), image.effects.innerShadow.distance);
        innerShadow.insert(QStringLiteral("angleDeg"), image.effects.innerShadow.angleDeg);
        innerShadow.insert(QStringLiteral("blur"), image.effects.innerShadow.blur);
        innerShadow.insert(QStringLiteral("opacity"), image.effects.innerShadow.opacity);
        effects.insert(QStringLiteral("innerShadow"), innerShadow);

        QJsonObject colorOverlay;
        colorOverlay.insert(QStringLiteral("enabled"), image.effects.colorOverlay.enabled);
        colorOverlay.insert(QStringLiteral("color"), colorToString(image.effects.colorOverlay.color));
        colorOverlay.insert(QStringLiteral("opacity"), image.effects.colorOverlay.opacity);
        effects.insert(QStringLiteral("colorOverlay"), colorOverlay);

        o.insert(QStringLiteral("effects"), effects);
        break;
    }
    case LayerType::Text: {
        o.insert(QStringLiteral("type"), QStringLiteral("text"));
        const auto& text = static_cast<const TextLayer&>(layer);
        o.insert(QStringLiteral("content"), text.content);
        o.insert(QStringLiteral("fontFamily"), text.fontFamily);
        o.insert(QStringLiteral("sizePt"), text.sizePt);
        o.insert(QStringLiteral("bold"), text.bold);
        o.insert(QStringLiteral("italic"), text.italic);
        o.insert(QStringLiteral("underline"), text.underline);
        o.insert(QStringLiteral("color"), colorToString(text.color));
        o.insert(QStringLiteral("letterSpacingPx"), text.letterSpacingPx);
        o.insert(QStringLiteral("lineHeightMult"), text.lineHeightMult);
        o.insert(QStringLiteral("align"), textAlignToString(text.align));
        o.insert(QStringLiteral("boxW"), text.box.width());
        o.insert(QStringLiteral("boxH"), text.box.height());

        QJsonObject effects;
        QJsonObject outline;
        outline.insert(QStringLiteral("enabled"), text.effects.outline.enabled);
        outline.insert(QStringLiteral("color"), colorToString(text.effects.outline.color));
        outline.insert(QStringLiteral("width"), text.effects.outline.width);
        effects.insert(QStringLiteral("outline"), outline);

        QJsonObject shadow;
        shadow.insert(QStringLiteral("enabled"), text.effects.shadow.enabled);
        shadow.insert(QStringLiteral("color"), colorToString(text.effects.shadow.color));
        shadow.insert(QStringLiteral("offsetX"), text.effects.shadow.offsetX);
        shadow.insert(QStringLiteral("offsetY"), text.effects.shadow.offsetY);
        shadow.insert(QStringLiteral("blur"), text.effects.shadow.blur);
        effects.insert(QStringLiteral("shadow"), shadow);

        QJsonObject gradient;
        gradient.insert(QStringLiteral("enabled"), text.effects.gradient.enabled);
        gradient.insert(QStringLiteral("type"), text.effects.gradient.type);
        gradient.insert(QStringLiteral("startColor"), colorToString(text.effects.gradient.startColor));
        gradient.insert(QStringLiteral("endColor"), colorToString(text.effects.gradient.endColor));
        gradient.insert(QStringLiteral("angleDeg"), text.effects.gradient.angleDeg);
        effects.insert(QStringLiteral("gradient"), gradient);

        QJsonObject texture;
        texture.insert(QStringLiteral("enabled"), text.effects.texture.enabled);
        texture.insert(QStringLiteral("assetId"), text.effects.texture.assetId.toString(QUuid::WithoutBraces));
        texture.insert(QStringLiteral("opacity"), text.effects.texture.opacity);
        effects.insert(QStringLiteral("texture"), texture);

        effects.insert(QStringLiteral("hollow"), text.effects.hollow);
        effects.insert(QStringLiteral("knockout"), text.effects.knockout);

        o.insert(QStringLiteral("effects"), effects);
        break;
    }
    case LayerType::Shape: {
        o.insert(QStringLiteral("type"), QStringLiteral("shape"));
        const auto& shape = static_cast<const ShapeLayer&>(layer);
        o.insert(QStringLiteral("kind"), shapeKindToString(shape.kind));
        o.insert(QStringLiteral("fill"), colorToString(shape.fill));
        o.insert(QStringLiteral("stroke"), colorToString(shape.stroke));
        o.insert(QStringLiteral("strokeWidth"), shape.strokeWidth);
        o.insert(QStringLiteral("cornerRadius"), shape.cornerRadius);
        if (shape.gradient.enabled)
            o.insert(QStringLiteral("gradient"), gradientToJson(shape.gradient));
        o.insert(QStringLiteral("starPoints"), shape.starPoints);
        o.insert(QStringLiteral("starInnerRatio"), shape.starInnerRatio);
        o.insert(QStringLiteral("strokeDashRatio"), shape.strokeDashRatio);
        if (shape.shadow.enabled) {
            QJsonObject sh;
            sh.insert(QStringLiteral("color"), colorToString(shape.shadow.color));
            sh.insert(QStringLiteral("offsetX"), shape.shadow.offsetX);
            sh.insert(QStringLiteral("offsetY"), shape.shadow.offsetY);
            sh.insert(QStringLiteral("blur"), shape.shadow.blur);
            o.insert(QStringLiteral("shadow"), sh);
        }
        QJsonArray points;
        for (const QPointF& point : shape.points)
            points.append(QJsonArray{point.x(), point.y()});
        o.insert(QStringLiteral("points"), points);
        break;
    }
    case LayerType::Background: {
        o.insert(QStringLiteral("type"), QStringLiteral("background"));
        const auto& background = static_cast<const BackgroundLayer&>(layer);
        o.insert(QStringLiteral("fill"), colorToString(background.fill));
        if (background.gradient.enabled)
            o.insert(QStringLiteral("gradient"),
                      gradientToJson(background.gradient));
        break;
    }
    }
    return o;
}

QString alphaMaskEntry(const ImageLayer& image)
{
    return QStringLiteral("masks/%1.png")
        .arg(image.id().toString(QUuid::WithoutBraces));
}

bool writeLayerMasks(mz_zip_archive* zip, const Layer& layer)
{
    if (layer.type() == LayerType::Group) {
        const auto& group = static_cast<const GroupLayer&>(layer);
        for (const auto& child : group.children) {
            if (!writeLayerMasks(zip, *child))
                return false;
        }
        return true;
    }

    if (layer.type() != LayerType::Image)
        return true;

    const auto& image = static_cast<const ImageLayer&>(layer);
    if (image.alphaMask.isNull())
        return true;

    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    if (!buffer.open(QIODevice::WriteOnly)
        || !image.alphaMask.convertToFormat(QImage::Format_Alpha8).save(&buffer, "PNG")) {
        return false;
    }

    const QByteArray entry = alphaMaskEntry(image).toUtf8();
    return mz_zip_writer_add_mem(zip, entry.constData(), pngBytes.constData(),
                                 static_cast<size_t>(pngBytes.size()),
                                 MZ_DEFAULT_LEVEL);
}

QJsonObject buildProjectJson(const Document& doc)
{
    QJsonObject root;
    root.insert(QLatin1String(kFormatKey), kProjectFormatVersion);

    QJsonObject app;
    app.insert(QStringLiteral("name"), QStringLiteral("CreatorCanvas"));
#ifdef CC_PROJECT_VERSION
    app.insert(QStringLiteral("version"), QStringLiteral(CC_PROJECT_VERSION));
#endif
    root.insert(QLatin1String(kAppKey), app);

    QJsonObject document;
    document.insert(QStringLiteral("width"), doc.width());
    document.insert(QStringLiteral("height"), doc.height());
    document.insert(QStringLiteral("dpi"), doc.dpi());
    root.insert(QLatin1String(kDocumentKey), document);

    QJsonArray assets;
    for (const Asset& asset : doc.assets().assets()) {
        QJsonObject o;
        o.insert(QStringLiteral("id"),
                 asset.id.toString(QUuid::WithoutBraces));
        o.insert(QStringLiteral("file"),
                 QStringLiteral("media/%1.%2")
                     .arg(asset.id.toString(QUuid::WithoutBraces),
                          asset.format));
        o.insert(QStringLiteral("format"), asset.format);
        o.insert(QStringLiteral("width"), asset.width);
        o.insert(QStringLiteral("height"), asset.height);
        o.insert(QStringLiteral("sha256"), asset.sha256);
        assets.append(o);
    }
    root.insert(QStringLiteral("assets"), assets);

    QJsonArray layers;
    for (const auto& child : doc.rootGroup()->children)
        layers.append(writeLayer(*child));
    root.insert(QLatin1String(kLayersKey), layers);

    // Keep the active pixel selection in the native project as well. This is
    // intentionally separate from layer alpha masks: a selection is editor
    // state and must be restored without changing the image pixels.
    if (!doc.pixelSelection().isNull())
        root.insert(QStringLiteral("pixelSelection"), QStringLiteral("selection.png"));

    if (!doc.layerComps().empty()) {
        QJsonArray compsArr;
        for (const auto& comp : doc.layerComps()) {
            QJsonObject compObj;
            compObj.insert(QStringLiteral("id"), comp.id);
            compObj.insert(QStringLiteral("name"), comp.name);
            compObj.insert(QStringLiteral("comment"), comp.comment);
            compObj.insert(QStringLiteral("applyVisibility"), comp.applyVisibility);
            compObj.insert(QStringLiteral("applyPosition"), comp.applyPosition);
            compObj.insert(QStringLiteral("applyAppearance"), comp.applyAppearance);
            QJsonArray statesArr;
            for (const auto& s : comp.states) {
                QJsonObject stateObj;
                stateObj.insert(QStringLiteral("layerId"), s.layerId.toString(QUuid::WithoutBraces));
                if (s.visible.has_value())
                    stateObj.insert(QStringLiteral("visible"), *s.visible);
                if (s.position.has_value()) {
                    stateObj.insert(QStringLiteral("posX"), s.position->x());
                    stateObj.insert(QStringLiteral("posY"), s.position->y());
                }
                if (s.opacity.has_value())
                    stateObj.insert(QStringLiteral("opacity"), static_cast<double>(*s.opacity));
                if (s.blendMode.has_value())
                    stateObj.insert(QStringLiteral("blendMode"), blendModeToString(*s.blendMode));
                statesArr.append(stateObj);
            }
            compObj.insert(QStringLiteral("states"), statesArr);
            compsArr.append(compObj);
        }
        root.insert(QStringLiteral("layerComps"), compsArr);
    }

    if (!doc.webSlices().empty()) {
        QJsonArray slicesArr;
        for (const auto& slice : doc.webSlices()) {
            QJsonObject sliceObj;
            sliceObj.insert(QStringLiteral("id"), slice.id);
            sliceObj.insert(QStringLiteral("name"), slice.name);
            sliceObj.insert(QStringLiteral("x"), slice.rect.x());
            sliceObj.insert(QStringLiteral("y"), slice.rect.y());
            sliceObj.insert(QStringLiteral("w"), slice.rect.width());
            sliceObj.insert(QStringLiteral("h"), slice.rect.height());
            sliceObj.insert(QStringLiteral("kind"), slice.kind == SliceKind::Image ? QStringLiteral("image") : QStringLiteral("noImage"));
            sliceObj.insert(QStringLiteral("format"), slice.exportFormat);
            slicesArr.append(sliceObj);
        }
        root.insert(QStringLiteral("webSlices"), slicesArr);
    }

    return root;
}

bool writeArchive(const QString& filePath, const QByteArray& jsonBytes,
                  const Document& doc, QString* error,
                  const QImage* thumbnail = nullptr)
{
    const QFileInfo info(filePath);
    const QDir targetDir = info.dir();
    if (!targetDir.mkpath(QStringLiteral("."))) {
        *error = QStringLiteral("Could not create the destination folder.");
        return false;
    }

    // Gravação atômica: cria um arquivo temporário no mesmo diretório
    const QString tempFileName = QStringLiteral(".%1.tmp.%2")
                                     .arg(info.fileName(),
                                          QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString tempFilePath = targetDir.filePath(tempFileName);

    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));

    if (!mz_zip_writer_init_file(&zip, QFile::encodeName(tempFilePath).constData(), 0)) {
        *error = QStringLiteral("Could not create the temporary project file.");
        return false;
    }

    const bool jsonOk =
        mz_zip_writer_add_mem(&zip, kEntryName, jsonBytes.constData(),
                              static_cast<size_t>(jsonBytes.size()),
                              MZ_DEFAULT_LEVEL);

    bool assetsOk = jsonOk;
    if (jsonOk) {
        for (const Asset& asset : doc.assets().assets()) {
            const QString entry = QStringLiteral("media/%1.%2")
                                      .arg(asset.id.toString(QUuid::WithoutBraces),
                                           asset.format);
            if (!mz_zip_writer_add_mem(&zip, entry.toUtf8().constData(),
                                       asset.encoded.constData(),
                                       static_cast<size_t>(asset.encoded.size()),
                                       MZ_DEFAULT_LEVEL)) {
                assetsOk = false;
                break;
            }
        }
    }

    if (assetsOk) {
        for (const auto& child : doc.rootGroup()->children) {
            if (!writeLayerMasks(&zip, *child)) {
                assetsOk = false;
                break;
            }
        }
    }

    if (assetsOk && !doc.pixelSelection().isNull()) {
        QByteArray selectionBytes;
        QBuffer buffer(&selectionBytes);
        buffer.open(QIODevice::WriteOnly);
        // Encode the alpha values in a regular RGBA PNG. Some PNG plugins
        // treat Qt's Alpha8 format as an indexed image and lose its values.
        const QImage source = doc.pixelSelection().convertToFormat(QImage::Format_Alpha8);
        QImage encodedSelection(source.size(), QImage::Format_ARGB32);
        for (int y = 0; y < source.height(); ++y) {
            const uchar* src = source.constScanLine(y);
            QRgb* dst = reinterpret_cast<QRgb*>(encodedSelection.scanLine(y));
            for (int x = 0; x < source.width(); ++x)
                dst[x] = qRgba(255, 255, 255, src[x]);
        }
        const bool encoded = encodedSelection.save(&buffer, "PNG");
        buffer.close();
        if (!encoded || selectionBytes.isEmpty()
            || !mz_zip_writer_add_mem(&zip, "selection.png",
                                      selectionBytes.constData(),
                                      static_cast<size_t>(selectionBytes.size()),
                                      MZ_DEFAULT_LEVEL)) {
            assetsOk = false;
        }
    }

    if (assetsOk && thumbnail && !thumbnail->isNull()) {
        QByteArray thumbBytes;
        QBuffer buffer(&thumbBytes);
        buffer.open(QIODevice::WriteOnly);
        thumbnail->save(&buffer, "PNG");
        buffer.close();

        if (!thumbBytes.isEmpty()) {
            mz_zip_writer_add_mem(&zip, "thumbnail.png", thumbBytes.constData(),
                                  static_cast<size_t>(thumbBytes.size()),
                                  MZ_DEFAULT_LEVEL);
        }
    }

    if (!assetsOk || !mz_zip_writer_finalize_archive(&zip)) {
        mz_zip_writer_end(&zip);
        QFile::remove(tempFilePath);
        *error = QStringLiteral("Could not write the project contents.");
        return false;
    }

    mz_zip_writer_end(&zip);

    // Substituição segura: NUNCA deixar o usuário sem nenhuma cópia do
    // projeto. Em POSIX, QFile::rename sobre arquivo existente substitui
    // atomicamente (remove+rename deixava uma janela em que um crash ou
    // falha de rename apagava TODAS as cópias).
    if (!QFile::rename(tempFilePath, filePath)) {
        // Rename pode falhar se o destino estiver aberto/travado (Windows).
        // Estratégia de backup: move o antigo para .bak, promove o temp e
        // só então remove o .bak — em qualquer falha, restaura o .bak.
        const QString bakPath = filePath + QStringLiteral(".bak");
        const bool hadExisting = QFile::exists(filePath);
        if (!hadExisting) {
            QFile::remove(tempFilePath);
            *error = QStringLiteral("Could not finalize the project file rename.");
            return false;
        }
        QFile::remove(bakPath);
        if (!QFile::rename(filePath, bakPath)) {
            QFile::remove(tempFilePath);
            *error = QStringLiteral("Could not overwrite the existing project file.");
            return false;
        }
        if (!QFile::rename(tempFilePath, filePath)) {
            // Fim do mundo: temp não virou destino. Restaura o .bak.
            QFile::rename(bakPath, filePath);
            QFile::remove(tempFilePath);
            *error = QStringLiteral("Could not finalize the project file rename.");
            return false;
        }
        QFile::remove(bakPath);
    }

    return true;
}

std::unique_ptr<Document> loadFail(QString* out, const QString& message,
                                   const QString& detail = {})
{
    qCWarning(lcSerialization) << "loadDocument failed:" << message << detail;
    if (out)
        *out = message;
    return nullptr;
}

} // namespace

// Defined at cc scope (NOT the anonymous namespace): Layer declares this
// class as a friend so ids can be restored exactly as saved.
class ProjectReader
{
public:
    ProjectReader(const QJsonObject& root, const QHash<QString, QByteArray>& media)
        : m_root(root)
        , m_media(media)
    {
    }

    std::unique_ptr<Document> run(QString* error)
    {
        const QJsonValue format = m_root.value(QLatin1String(kFormatKey));
        if (!format.isDouble()) {
            *error = QStringLiteral(
                "This file is not a CreatorCanvas project "
                "(missing format version).");
            return nullptr;
        }

        const int version = format.toInt();
        if (version > kProjectFormatVersion) {
            *error = QStringLiteral(
                "This project was saved by a newer version of CreatorCanvas "
                "and cannot be opened with this version.");
            return nullptr;
        }
        if (version < kProjectFormatVersion) {
            *error = QStringLiteral(
                "This project uses an older file format that this version "
                "cannot read.");
            return nullptr;
        }

        const QJsonObject document =
            m_root.value(QLatin1String(kDocumentKey)).toObject();
        const int width = document.value(QStringLiteral("width")).toInt(-1);
        const int height = document.value(QStringLiteral("height")).toInt(-1);
        const int dpi = document.value(QStringLiteral("dpi")).toInt(96);
        if (width < 1 || height < 1) {
            *error = QStringLiteral(
                "The project file contains invalid canvas dimensions.");
            return nullptr;
        }

        auto doc = std::make_unique<Document>(width, height, dpi);

        const QJsonArray assets = m_root.value(QLatin1String(kAssetsKey)).toArray();
        for (const QJsonValue& value : assets) {
            const QJsonObject o = value.toObject();
            Asset asset;
            asset.id = LayerId(o.value(QStringLiteral("id")).toString());
            const QString file = o.value(QStringLiteral("file")).toString();
            asset.format = o.value(QStringLiteral("format")).toString();
            asset.width = o.value(QStringLiteral("width")).toInt();
            asset.height = o.value(QStringLiteral("height")).toInt();
            asset.sha256 = o.value(QStringLiteral("sha256")).toString();

            if (asset.id.isNull() || file.isEmpty() || asset.sha256.isEmpty()) {
                *error = QStringLiteral(
                    "The project file contains an invalid embedded asset.");
                return nullptr;
            }

            asset.encoded = m_media.value(file);
            if (asset.encoded.isEmpty()) {
                *error = QStringLiteral(
                    "An embedded image is missing from the project archive.");
                return nullptr;
            }

            const QString actualHash = QString::fromLatin1(
                QCryptographicHash::hash(asset.encoded,
                                         QCryptographicHash::Sha256).toHex());
            if (actualHash != asset.sha256) {
                *error = QStringLiteral(
                    "An embedded image failed the integrity check (SHA-256).");
                return nullptr;
            }

            doc->assets().restore(asset);
        }

        const QJsonArray layers = m_root.value(QLatin1String(kLayersKey)).toArray();
        for (const QJsonValue& value : layers) {
            std::unique_ptr<Layer> layer;
            if (!readLayer(value.toObject(), error, &layer))
                return nullptr;
            // Asset pendurado (arquivo editado/corrompido) viraria uma
            // camada invisível e silenciosa — falha com erro claro.
            std::function<bool(const Layer*)> validateAssets =
                [&](const Layer* l) -> bool {
                if (l->type() == LayerType::Image) {
                    const auto* img = static_cast<const ImageLayer*>(l);
                    // assetId nulo é legítimo (camada de imagem sem bitmap);
                    // id apontando para um asset que NÃO está no arquivo é
                    // corrupção — a camada ficaria invisível e silenciosa.
                    if (!img->assetId.isNull()
                        && !doc->assets().find(img->assetId)) {
                        *error = QStringLiteral(
                            "The project references an image that is not "
                            "embedded in the archive.");
                        return false;
                    }
                }
                if (l->type() == LayerType::Group) {
                    for (const auto& child :
                         static_cast<const GroupLayer*>(l)->children)
                        if (!validateAssets(child.get()))
                            return false;
                }
                return true;
            };
            if (!validateAssets(layer.get()))
                return nullptr;
            doc->addLayer(std::move(layer));
        }

        const QString selectionFile = m_root.value(QStringLiteral("pixelSelection")).toString();
        if (!selectionFile.isEmpty()) {
            QImage selection;
            const QByteArray selectionBytes = m_media.value(selectionFile);
            if (selectionBytes.isEmpty() || !selection.loadFromData(selectionBytes, "PNG")) {
                *error = QStringLiteral("The project contains an unreadable pixel selection.");
                return nullptr;
            }
            if (selection.size() != QSize(width, height)) {
                *error = QStringLiteral("The project contains a pixel selection with invalid dimensions.");
                return nullptr;
            }
            QImage restored(selection.size(), QImage::Format_Alpha8);
            for (int y = 0; y < selection.height(); ++y) {
                uchar* dst = restored.scanLine(y);
                for (int x = 0; x < selection.width(); ++x)
                    dst[x] = qAlpha(selection.pixel(x, y));
            }
            doc->setPixelSelection(std::move(restored));
        }

        if (m_root.contains(QStringLiteral("layerComps"))) {
            const QJsonArray compsArr = m_root.value(QStringLiteral("layerComps")).toArray();
            for (const QJsonValue& val : compsArr) {
                const QJsonObject compObj = val.toObject();
                LayerComp comp;
                comp.id = compObj.value(QStringLiteral("id")).toString();
                comp.name = compObj.value(QStringLiteral("name")).toString();
                comp.comment = compObj.value(QStringLiteral("comment")).toString();
                comp.applyVisibility = compObj.value(QStringLiteral("applyVisibility")).toBool(true);
                comp.applyPosition = compObj.value(QStringLiteral("applyPosition")).toBool(true);
                comp.applyAppearance = compObj.value(QStringLiteral("applyAppearance")).toBool(true);
                const QJsonArray statesArr = compObj.value(QStringLiteral("states")).toArray();
                for (const QJsonValue& sVal : statesArr) {
                    const QJsonObject sObj = sVal.toObject();
                    CompLayerState state;
                    state.layerId = LayerId(sObj.value(QStringLiteral("layerId")).toString());
                    if (sObj.contains(QStringLiteral("visible")))
                        state.visible = sObj.value(QStringLiteral("visible")).toBool();
                    if (sObj.contains(QStringLiteral("posX")) && sObj.contains(QStringLiteral("posY"))) {
                        state.position = QPointF(sObj.value(QStringLiteral("posX")).toDouble(),
                                                 sObj.value(QStringLiteral("posY")).toDouble());
                    }
                    if (sObj.contains(QStringLiteral("opacity")))
                        state.opacity = static_cast<float>(sObj.value(QStringLiteral("opacity")).toDouble());
                    if (sObj.contains(QStringLiteral("blendMode")))
                        state.blendMode = blendModeFromString(sObj.value(QStringLiteral("blendMode")).toString());
                    comp.states.push_back(std::move(state));
                }
                doc->addLayerComp(std::move(comp));
            }
        }

        if (m_root.contains(QStringLiteral("webSlices"))) {
            const QJsonArray slicesArr = m_root.value(QStringLiteral("webSlices")).toArray();
            for (const QJsonValue& val : slicesArr) {
                const QJsonObject sliceObj = val.toObject();
                WebSlice slice;
                slice.id = sliceObj.value(QStringLiteral("id")).toString();
                slice.name = sliceObj.value(QStringLiteral("name")).toString();
                slice.rect = QRectF(sliceObj.value(QStringLiteral("x")).toDouble(),
                                    sliceObj.value(QStringLiteral("y")).toDouble(),
                                    sliceObj.value(QStringLiteral("w")).toDouble(),
                                    sliceObj.value(QStringLiteral("h")).toDouble());
                slice.kind = sliceObj.value(QStringLiteral("kind")).toString() == QLatin1String("noImage") ? SliceKind::NoImage : SliceKind::Image;
                slice.exportFormat = sliceObj.value(QStringLiteral("format")).toString(QStringLiteral("png"));
                doc->addWebSlice(std::move(slice));
            }
        }

        return doc;
    }

private:
    bool readLayer(const QJsonObject& o, QString* error,
                   std::unique_ptr<Layer>* out)
    {
        const QString type = o.value(QStringLiteral("type")).toString();
        const LayerId id(o.value(QStringLiteral("id")).toString());
        if (type.isEmpty() || id.isNull()) {
            *error = QStringLiteral(
                "A layer in the project file is missing its type or "
                "identifier.");
            return false;
        }

        if (type == QLatin1String("group"))
            return readGroup(o, id, error, out);
        if (type == QLatin1String("image"))
            return readImage(o, id, error, out);
        if (type == QLatin1String("text"))
            return readText(o, id, out);
        if (type == QLatin1String("shape"))
            return readShape(o, id, out);
        if (type == QLatin1String("background"))
            return readBackground(o, id, out);

        qCWarning(lcSerialization)
            << "Unknown layer type" << type
            << "- loading as a placeholder group.";
        auto placeholder = std::make_unique<GroupLayer>();
        if (!readChildren(o, placeholder->children, error))
            return false;
        finishLayer(*placeholder, o, id);
        *out = std::move(placeholder);
        return true;
    }

    bool readGroup(const QJsonObject& o, const LayerId& id, QString* error,
                   std::unique_ptr<Layer>* out)
    {
        auto group = std::make_unique<GroupLayer>();
        group->isExpanded = o.value(QStringLiteral("isExpanded")).toBool(true);
        if (!readChildren(o, group->children, error))
            return false;
        finishLayer(*group, o, id);
        *out = std::move(group);
        return true;
    }

    bool readChildren(const QJsonObject& o,
                      std::vector<std::unique_ptr<Layer>>& children,
                      QString* error)
    {
        // Anti stack-overflow: um arquivo hostil com grupos aninhados até
        // centenas de milhares de níveis estourava a pilha do C. Limite
        // generoso para projetos reais, falha com erro amigável acima dele.
        if (m_readDepth >= 64) {
            *error = QStringLiteral(
                "The project contains groups nested too deeply.");
            return false;
        }
        struct DepthRestore {
            int& d;
            ~DepthRestore() { --d; }
        } restore{++m_readDepth};
        const QJsonArray array =
            o.value(QStringLiteral("children")).toArray();
        for (const QJsonValue& value : array) {
            std::unique_ptr<Layer> child;
            if (!readLayer(value.toObject(), error, &child))
                return false;
            children.push_back(std::move(child));
        }
        return true;
    }

    bool readImage(const QJsonObject& o, const LayerId& id, QString* error,
                   std::unique_ptr<Layer>* out)
    {
        auto image = std::make_unique<ImageLayer>();
        image->assetId = LayerId(o.value(QStringLiteral("assetId")).toString());
        image->naturalWidth = o.value(QStringLiteral("naturalWidth")).toInt();
        image->naturalHeight = o.value(QStringLiteral("naturalHeight")).toInt();

        const QString maskFile = o.value(QStringLiteral("alphaMask")).toString();
        if (!maskFile.isEmpty()) {
            const QByteArray maskBytes = m_media.value(maskFile);
            QImage mask;
            if (maskBytes.isEmpty() || !mask.loadFromData(maskBytes, "PNG")) {
                *error = QStringLiteral("An image layer has an unreadable alpha mask.");
                return false;
            }
            image->alphaMask = mask.convertToFormat(QImage::Format_Alpha8);
            const QSize expectedSize(image->naturalWidth, image->naturalHeight);
            if (expectedSize.isEmpty() || image->alphaMask.size() != expectedSize) {
                *error = QStringLiteral("An image layer has an alpha mask with invalid dimensions.");
                return false;
            }
        }

        const QJsonObject effects = o.value(QStringLiteral("effects")).toObject();
        const QJsonObject outline = effects.value(QStringLiteral("outline")).toObject();
        image->effects.outline.enabled = outline.value(QStringLiteral("enabled")).toBool(false);
        image->effects.outline.color = colorFromString(outline.value(QStringLiteral("color")).toString(QStringLiteral("#ffffff")));
        image->effects.outline.width = outline.value(QStringLiteral("width")).toDouble(8.0);
        image->effects.outline.blur = outline.value(QStringLiteral("blur")).toDouble(0.0);
        image->effects.outline.opacity = outline.value(QStringLiteral("opacity")).toDouble(1.0);

        const QJsonObject dropShadow = effects.value(QStringLiteral("dropShadow")).toObject();
        image->effects.dropShadow.enabled = dropShadow.value(QStringLiteral("enabled")).toBool(false);
        image->effects.dropShadow.color = colorFromString(dropShadow.value(QStringLiteral("color")).toString(QStringLiteral("#000000")));
        image->effects.dropShadow.distance = dropShadow.value(QStringLiteral("distance")).toDouble(10.0);
        image->effects.dropShadow.angleDeg = dropShadow.value(QStringLiteral("angleDeg")).toDouble(120.0);
        image->effects.dropShadow.blur = dropShadow.value(QStringLiteral("blur")).toDouble(15.0);
        image->effects.dropShadow.opacity = dropShadow.value(QStringLiteral("opacity")).toDouble(0.75);

        const QJsonObject innerShadow = effects.value(QStringLiteral("innerShadow")).toObject();
        image->effects.innerShadow.enabled = innerShadow.value(QStringLiteral("enabled")).toBool(false);
        image->effects.innerShadow.color = colorFromString(innerShadow.value(QStringLiteral("color")).toString(QStringLiteral("#000000")));
        image->effects.innerShadow.distance = innerShadow.value(QStringLiteral("distance")).toDouble(8.0);
        image->effects.innerShadow.angleDeg = innerShadow.value(QStringLiteral("angleDeg")).toDouble(120.0);
        image->effects.innerShadow.blur = innerShadow.value(QStringLiteral("blur")).toDouble(10.0);
        image->effects.innerShadow.opacity = innerShadow.value(QStringLiteral("opacity")).toDouble(0.6);

        const QJsonObject colorOverlay = effects.value(QStringLiteral("colorOverlay")).toObject();
        image->effects.colorOverlay.enabled = colorOverlay.value(QStringLiteral("enabled")).toBool(false);
        image->effects.colorOverlay.color = colorFromString(colorOverlay.value(QStringLiteral("color")).toString(QStringLiteral("#ff0000")));
        image->effects.colorOverlay.opacity = colorOverlay.value(QStringLiteral("opacity")).toDouble(0.5);

        finishLayer(*image, o, id);
        *out = std::move(image);
        return true;
    }

    bool readText(const QJsonObject& o, const LayerId& id,
                  std::unique_ptr<Layer>* out)
    {
        auto text = std::make_unique<TextLayer>();
        text->content = o.value(QStringLiteral("content")).toString();
        text->fontFamily = o.value(QStringLiteral("fontFamily"))
                               .toString(QStringLiteral("Sans Serif"));
        text->sizePt = o.value(QStringLiteral("sizePt")).toDouble(48.0);
        text->bold = o.value(QStringLiteral("bold")).toBool(false);
        text->italic = o.value(QStringLiteral("italic")).toBool(false);
        text->underline = o.value(QStringLiteral("underline")).toBool(false);
        text->color =
            colorFromString(o.value(QStringLiteral("color")).toString());
        text->letterSpacingPx =
            o.value(QStringLiteral("letterSpacingPx")).toDouble(0.0);
        text->lineHeightMult =
            o.value(QStringLiteral("lineHeightMult")).toDouble(1.0);
        text->align =
            textAlignFromString(o.value(QStringLiteral("align")).toString())
                .value_or(TextAlignment::Center);
        text->box = QSizeF(o.value(QStringLiteral("boxW")).toDouble(0.0),
                           o.value(QStringLiteral("boxH")).toDouble(0.0));

        const QJsonObject effects = o.value(QStringLiteral("effects")).toObject();
        const QJsonObject outline = effects.value(QStringLiteral("outline")).toObject();
        text->effects.outline.enabled = outline.value(QStringLiteral("enabled")).toBool(false);
        text->effects.outline.color = colorFromString(outline.value(QStringLiteral("color")).toString());
        text->effects.outline.width = outline.value(QStringLiteral("width")).toDouble(4.0);

        const QJsonObject shadow = effects.value(QStringLiteral("shadow")).toObject();
        text->effects.shadow.enabled = shadow.value(QStringLiteral("enabled")).toBool(false);
        text->effects.shadow.color = colorFromString(shadow.value(QStringLiteral("color")).toString());
        text->effects.shadow.offsetX = shadow.value(QStringLiteral("offsetX")).toDouble(4.0);
        text->effects.shadow.offsetY = shadow.value(QStringLiteral("offsetY")).toDouble(4.0);
        text->effects.shadow.blur = shadow.value(QStringLiteral("blur")).toDouble(6.0);

        const QJsonObject gradient = effects.value(QStringLiteral("gradient")).toObject();
        text->effects.gradient.enabled = gradient.value(QStringLiteral("enabled")).toBool(false);
        text->effects.gradient.type = gradient.value(QStringLiteral("type")).toInt(0);
        text->effects.gradient.startColor = colorFromString(gradient.value(QStringLiteral("startColor")).toString());
        text->effects.gradient.endColor = colorFromString(gradient.value(QStringLiteral("endColor")).toString());
        text->effects.gradient.angleDeg = gradient.value(QStringLiteral("angleDeg")).toDouble(0.0);

        const QJsonObject texture = effects.value(QStringLiteral("texture")).toObject();
        text->effects.texture.enabled = texture.value(QStringLiteral("enabled")).toBool(false);
        text->effects.texture.assetId = QUuid::fromString(texture.value(QStringLiteral("assetId")).toString());
        text->effects.texture.opacity = texture.value(QStringLiteral("opacity")).toDouble(1.0);

        text->effects.hollow = effects.value(QStringLiteral("hollow")).toBool(false);
        text->effects.knockout = effects.value(QStringLiteral("knockout")).toBool(false);

        finishLayer(*text, o, id);
        *out = std::move(text);
        return true;
    }

    bool readShape(const QJsonObject& o, const LayerId& id,
                   std::unique_ptr<Layer>* out)
    {
        auto shape = std::make_unique<ShapeLayer>();
        shape->kind =
            shapeKindFromString(o.value(QStringLiteral("kind")).toString())
                .value_or(ShapeKind::Rectangle);
        shape->fill = colorFromString(o.value(QStringLiteral("fill")).toString());
        shape->stroke =
            colorFromString(o.value(QStringLiteral("stroke")).toString());
        shape->strokeWidth =
            o.value(QStringLiteral("strokeWidth")).toDouble(0.0);
        shape->cornerRadius =
            o.value(QStringLiteral("cornerRadius")).toDouble(0.0);
        if (o.contains(QStringLiteral("gradient")))
            shape->gradient =
                gradientFromJson(o.value(QStringLiteral("gradient")).toObject());
        shape->starPoints = qBound(3, o.value(QStringLiteral("starPoints")).toInt(5), 16);
        shape->starInnerRatio = qBound(0.1, o.value(QStringLiteral("starInnerRatio")).toDouble(0.42), 0.9);
        shape->strokeDashRatio = qBound(0.0, o.value(QStringLiteral("strokeDashRatio")).toDouble(0.0), 10.0);
        if (o.contains(QStringLiteral("shadow"))) {
            const QJsonObject sh = o.value(QStringLiteral("shadow")).toObject();
            shape->shadow.enabled = true;
            shape->shadow.color = colorFromString(sh.value(QStringLiteral("color")).toString());
            shape->shadow.offsetX = sh.value(QStringLiteral("offsetX")).toDouble(5.0);
            shape->shadow.offsetY = sh.value(QStringLiteral("offsetY")).toDouble(5.0);
            shape->shadow.blur = sh.value(QStringLiteral("blur")).toDouble(8.0);
        }

        const QJsonArray points = o.value(QStringLiteral("points")).toArray();
        for (const QJsonValue& point : points) {
            const QJsonArray pair = point.toArray();
            if (pair.size() == 2)
                shape->points.append(QPointF(pair.at(0).toDouble(),
                                             pair.at(1).toDouble()));
        }

        finishLayer(*shape, o, id);
        *out = std::move(shape);
        return true;
    }

    bool readBackground(const QJsonObject& o, const LayerId& id,
                        std::unique_ptr<Layer>* out)
    {
        auto background = std::make_unique<BackgroundLayer>();
        background->fill =
            colorFromString(o.value(QStringLiteral("fill")).toString());
        if (o.contains(QStringLiteral("gradient")))
            background->gradient =
                gradientFromJson(o.value(QStringLiteral("gradient")).toObject());
        finishLayer(*background, o, id);
        *out = std::move(background);
        return true;
    }

    void finishLayer(Layer& layer, const QJsonObject& o, const LayerId& id)
    {
        layer.m_id = id;
        layer.name = o.value(QStringLiteral("name")).toString();
        layer.visible = o.value(QStringLiteral("visible")).toBool(true);
        layer.locked = o.value(QStringLiteral("locked")).toBool(false);
        layer.colorTag =
            layerColorTagFromString(o.value(QStringLiteral("colorTag")).toString())
                .value_or(LayerColorTag::None);
        layer.setOpacity(
            float(o.value(QStringLiteral("opacity")).toDouble(1.0)));
        layer.blendMode =
            blendModeFromString(o.value(QStringLiteral("blendMode")).toString())
                .value_or(BlendMode::Normal);
        layer.transform.position = QPointF(
            o.value(QStringLiteral("transformX")).toDouble(0.0),
            o.value(QStringLiteral("transformY")).toDouble(0.0));
        layer.transform.rotationDeg =
            o.value(QStringLiteral("rotation")).toDouble(0.0);
        layer.transform.scaleX =
            o.value(QStringLiteral("scaleX")).toDouble(1.0);
        layer.transform.scaleY =
            o.value(QStringLiteral("scaleY")).toDouble(1.0);
        layer.transform.shearX =
            o.value(QStringLiteral("shearX")).toDouble(0.0);
        layer.transform.shearY =
            o.value(QStringLiteral("shearY")).toDouble(0.0);

        if (o.contains(QStringLiteral("blendIf"))) {
            const QJsonArray rangesArr = o.value(QStringLiteral("blendIf")).toArray();
            for (std::size_t i = 0; i < static_cast<std::size_t>(rangesArr.size()); ++i) {
                const QJsonObject rangeObj = rangesArr.at(static_cast<int>(i)).toObject();
                BlendRange thisRange;
                thisRange.black[0] = static_cast<quint8>(rangeObj.value(QStringLiteral("thisBlackLo")).toInt(0));
                thisRange.black[1] = static_cast<quint8>(rangeObj.value(QStringLiteral("thisBlackHi")).toInt(0));
                thisRange.white[0] = static_cast<quint8>(rangeObj.value(QStringLiteral("thisWhiteLo")).toInt(255));
                thisRange.white[1] = static_cast<quint8>(rangeObj.value(QStringLiteral("thisWhiteHi")).toInt(255));

                BlendRange underRange;
                underRange.black[0] = static_cast<quint8>(rangeObj.value(QStringLiteral("underBlackLo")).toInt(0));
                underRange.black[1] = static_cast<quint8>(rangeObj.value(QStringLiteral("underBlackHi")).toInt(0));
                underRange.white[0] = static_cast<quint8>(rangeObj.value(QStringLiteral("underWhiteLo")).toInt(255));
                underRange.white[1] = static_cast<quint8>(rangeObj.value(QStringLiteral("underWhiteHi")).toInt(255));

                layer.blendIf.set(i, {thisRange, underRange});
            }
        }
    }

    QJsonObject m_root;
    const QHash<QString, QByteArray>& m_media;
    int m_readDepth = 0;
};

bool saveDocument(const Document& doc, const QString& filePath,
                  QString* errorMessage,
                  const QImage* thumbnail)
{
    const QByteArray bytes =
        QJsonDocument(buildProjectJson(doc)).toJson(QJsonDocument::Indented);

    QString localError;
    if (!writeArchive(filePath, bytes, doc, &localError, thumbnail)) {
        qCWarning(lcSerialization) << "saveDocument failed:" << localError
                                   << filePath;
        if (errorMessage)
            *errorMessage = localError;
        return false;
    }
    return true;
}

std::unique_ptr<Document> loadDocument(const QString& filePath,
                                       QString* errorMessage)
{
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, QFile::encodeName(filePath).constData(), 0)) {
        return loadFail(errorMessage, QStringLiteral(
            "The file could not be opened as a CreatorCanvas project. "
            "It may be corrupted or in a different format."), filePath);
    }

    QByteArray bytes;
    QHash<QString, QByteArray> media;
    {
        const mz_uint entryCount = mz_zip_reader_get_num_files(&zip);
        if (entryCount > kMaxProjectEntries) {
            mz_zip_reader_end(&zip);
            return loadFail(errorMessage, QStringLiteral(
                "The project archive contains too many files (possible corrupted file or zip bomb)."));
        }

        mz_uint64 totalUncompressedBytes = 0;

        for (mz_uint i = 0; i < entryCount; ++i) {
            mz_zip_archive_file_stat stat;
            if (!mz_zip_reader_file_stat(&zip, i, &stat))
                continue;

            if (stat.m_is_directory)
                continue;

            if (stat.m_uncomp_size > kMaxSingleAssetSize) {
                mz_zip_reader_end(&zip);
                return loadFail(errorMessage, QStringLiteral(
                    "A file in the project archive exceeds safety size limits."));
            }

            totalUncompressedBytes += stat.m_uncomp_size;
            if (totalUncompressedBytes > kMaxTotalUncompressedBytes) {
                mz_zip_reader_end(&zip);
                return loadFail(errorMessage, QStringLiteral(
                    "Total uncompressed size of the project exceeds safety limits."));
            }

            const QString entryName = QString::fromUtf8(stat.m_filename);

            size_t size = 0;
            if (void* data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0)) {
                const QByteArray entryBytes(static_cast<const char*>(data),
                                            static_cast<qint64>(size));
                if (entryName == QLatin1String(kEntryName))
                    bytes = entryBytes;
                else if (entryName.startsWith(QLatin1String("media/"))
                         || entryName.startsWith(QLatin1String("masks/"))
                         || entryName == QLatin1String("selection.png"))
                    media.insert(entryName, entryBytes);
                mz_free(data);
            }
        }
        mz_zip_reader_end(&zip);
    }

    if (bytes.isEmpty()) {
        return loadFail(errorMessage, QStringLiteral(
            "The project archive does not contain a readable project "
            "description (project.json)."));
    }

    QJsonParseError parseError{};
    const QJsonDocument parsed = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        return loadFail(errorMessage, QStringLiteral(
            "The project description inside the file is not valid."),
            parseError.errorString());
    }

    QString error;
    ProjectReader reader(parsed.object(), media);
    auto document = reader.run(&error);
    if (!document) {
        qCWarning(lcSerialization) << "loadDocument failed:" << error
                                   << filePath;
        if (errorMessage)
            *errorMessage = error;
        return nullptr;
    }
    return document;
}

QImage loadProjectThumbnail(const QString& filePath)
{
    if (!QFile::exists(filePath))
        return {};

    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, QFile::encodeName(filePath).constData(), 0))
        return {};

    int fileIndex = mz_zip_reader_locate_file(&zip, "thumbnail.png", nullptr, 0);
    if (fileIndex < 0) {
        mz_zip_reader_end(&zip);
        return {};
    }

    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&zip, static_cast<mz_uint>(fileIndex), &stat)) {
        mz_zip_reader_end(&zip);
        return {};
    }

    // Limite de segurança: thumbnail não deve ter mais que 8 MB
    if (stat.m_uncomp_size > 8ULL * 1024ULL * 1024ULL) {
        mz_zip_reader_end(&zip);
        return {};
    }

    size_t size = 0;
    void* data = mz_zip_reader_extract_to_heap(&zip, static_cast<mz_uint>(fileIndex), &size, 0);
    mz_zip_reader_end(&zip);

    if (!data)
        return {};

    QImage img;
    img.loadFromData(static_cast<const uchar*>(data), static_cast<int>(size), "PNG");
    mz_free(data);
    return img;
}

} // namespace cc
