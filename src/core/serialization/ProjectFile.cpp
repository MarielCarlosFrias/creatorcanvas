#include <miniz.h>
#include "ProjectFile.h"

#include "core/Document.h"
#include "core/layers/Layer.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>


#include <cstring>
#include <memory>
#include <optional>
#include <utility>

namespace cc {

Q_LOGGING_CATEGORY(lcSerialization, "cc.serialization")

constexpr auto kEntryName   = "project.json";
constexpr auto kFormatKey   = "formatVersion";
constexpr auto kAppKey      = "app";
constexpr auto kDocumentKey = "document";
constexpr auto kLayersKey   = "layers";

// ------------------------------------------------------------ enum mapping

QString blendModeToString(BlendMode mode)
{
    switch (mode) {
    case BlendMode::Normal:   return QStringLiteral("normal");
    case BlendMode::Multiply: return QStringLiteral("multiply");
    case BlendMode::Screen:   return QStringLiteral("screen");
    case BlendMode::Overlay:  return QStringLiteral("overlay");
    case BlendMode::Darken:   return QStringLiteral("darken");
    case BlendMode::Lighten:  return QStringLiteral("lighten");
    case BlendMode::Add:      return QStringLiteral("add");
    }
    return QStringLiteral("normal");
}

std::optional<BlendMode> blendModeFromString(const QString& value)
{
    if (value == QLatin1String("normal"))   return BlendMode::Normal;
    if (value == QLatin1String("multiply")) return BlendMode::Multiply;
    if (value == QLatin1String("screen"))   return BlendMode::Screen;
    if (value == QLatin1String("overlay"))  return BlendMode::Overlay;
    if (value == QLatin1String("darken"))   return BlendMode::Darken;
    if (value == QLatin1String("lighten"))  return BlendMode::Lighten;
    if (value == QLatin1String("add"))      return BlendMode::Add;
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
    }
    return QStringLiteral("rectangle");
}

std::optional<ShapeKind> shapeKindFromString(const QString& value)
{
    if (value == QLatin1String("rectangle"))   return ShapeKind::Rectangle;
    if (value == QLatin1String("roundedRect")) return ShapeKind::RoundedRect;
    if (value == QLatin1String("ellipse"))     return ShapeKind::Ellipse;
    if (value == QLatin1String("line"))        return ShapeKind::Line;
    if (value == QLatin1String("polygon"))     return ShapeKind::Polygon;
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

// ----------------------------------------------------------------- writing

QJsonObject writeLayer(const Layer& layer)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), layer.id().toString(QUuid::WithoutBraces));
    o.insert(QStringLiteral("name"), layer.name);
    o.insert(QStringLiteral("visible"), layer.visible);
    o.insert(QStringLiteral("locked"), layer.locked);
    o.insert(QStringLiteral("opacity"), double(layer.opacity()));
    o.insert(QStringLiteral("blendMode"), blendModeToString(layer.blendMode));

    switch (layer.type()) {
    case LayerType::Group: {
        o.insert(QStringLiteral("type"), QStringLiteral("group"));
        const auto& group = static_cast<const GroupLayer&>(layer);
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
        break;
    }
    }
    return o;
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

    QJsonArray layers;
    for (const auto& child : doc.rootGroup()->children)
        layers.append(writeLayer(*child));
    root.insert(QLatin1String(kLayersKey), layers);

    return root;
}

bool writeArchive(const QString& filePath, const QByteArray& bytes,
                  QString* error)
{
    const QFileInfo info(filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        *error = QStringLiteral("Could not create the destination folder.");
        return false;
    }

    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));

    if (!mz_zip_writer_init_file(&zip, QFile::encodeName(filePath).constData(), 0)) {
        *error = QStringLiteral("Could not create the project file.");
        return false;
    }

    if (!mz_zip_writer_add_mem(&zip, kEntryName, bytes.constData(),
                               static_cast<size_t>(bytes.size()),
                               MZ_DEFAULT_LEVEL)
        || !mz_zip_writer_finalize_archive(&zip)) {
        mz_zip_writer_end(&zip);
        QFile::remove(filePath);
        *error = QStringLiteral("Could not write the project contents.");
        return false;
    }

    mz_zip_writer_end(&zip);
    return true;
}

// ----------------------------------------------------------------- reading

class ProjectReader
{
public:
    explicit ProjectReader(const QJsonObject& root)
        : m_root(root)
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

        const QJsonArray layers = m_root.value(QLatin1String(kLayersKey)).toArray();
        for (const QJsonValue& value : layers) {
            std::unique_ptr<Layer> layer;
            if (!readLayer(value.toObject(), error, &layer))
                return nullptr;
            doc->addLayer(std::move(layer));
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
            return readImage(o, id, out);
        if (type == QLatin1String("text"))
            return readText(o, id, out);
        if (type == QLatin1String("shape"))
            return readShape(o, id, out);
        if (type == QLatin1String("background"))
            return readBackground(o, id, out);

        // Unknown type: placeholder group
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

    bool readImage(const QJsonObject& o, const LayerId& id,
                   std::unique_ptr<Layer>* out)
    {
        auto image = std::make_unique<ImageLayer>();
        image->assetId = LayerId(o.value(QStringLiteral("assetId")).toString());
        image->naturalWidth = o.value(QStringLiteral("naturalWidth")).toInt();
        image->naturalHeight = o.value(QStringLiteral("naturalHeight")).toInt();
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
        finishLayer(*background, o, id);
        *out = std::move(background);
        return true;
    }

    void finishLayer(Layer& layer, const QJsonObject& o, const LayerId& id)
    {
        layer.m_id = id; // friend access
        layer.name = o.value(QStringLiteral("name")).toString();
        layer.visible = o.value(QStringLiteral("visible")).toBool(true);
        layer.locked = o.value(QStringLiteral("locked")).toBool(false);
        layer.setOpacity(
            float(o.value(QStringLiteral("opacity")).toDouble(1.0)));
        layer.blendMode =
            blendModeFromString(o.value(QStringLiteral("blendMode")).toString())
                .value_or(BlendMode::Normal);
    }

    QJsonObject m_root;
};

// ----------------------------------------------------------------- API

bool saveDocument(const Document& doc, const QString& filePath,
                  QString* errorMessage)
{
    const QByteArray bytes =
        QJsonDocument(buildProjectJson(doc)).toJson(QJsonDocument::Indented);

    QString localError;
    if (!writeArchive(filePath, bytes, &localError)) {
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
        qCWarning(lcSerialization) << "loadDocument failed: cannot open ZIP" << filePath;
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "The file could not be opened as a CreatorCanvas project. "
                "It may be corrupted or in a different format.");
        return nullptr;
    }

    QByteArray bytes;
    {
        const int entryIndex =
            mz_zip_reader_locate_file(&zip, kEntryName, nullptr, 0);
        if (entryIndex >= 0) {
            size_t size = 0;
            if (void* data = mz_zip_reader_extract_to_heap(
                    &zip, static_cast<mz_uint>(entryIndex), &size, 0)) {
                bytes = QByteArray(static_cast<const char*>(data),
                                   static_cast<qint64>(size));
                mz_free(data);
            }
        }
        mz_zip_reader_end(&zip);
    }

    if (bytes.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "The project archive does not contain a readable project "
                "description (project.json).");
        return nullptr;
    }

    QJsonParseError parseError{};
    const QJsonDocument parsed = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "The project description inside the file is not valid.");
        return nullptr;
    }

    QString error;
    ProjectReader reader(parsed.object());
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

} // namespace cc
