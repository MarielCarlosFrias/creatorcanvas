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
#include <QLoggingCategory>
#include <QCryptographicHash>

#include <miniz.h>

#include <cstring>
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

QJsonObject writeLayer(const Layer& layer)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), layer.id().toString(QUuid::WithoutBraces));
    o.insert(QStringLiteral("name"), layer.name);
    o.insert(QStringLiteral("visible"), layer.visible);
    o.insert(QStringLiteral("locked"), layer.locked);
    o.insert(QStringLiteral("opacity"), double(layer.opacity()));
    o.insert(QStringLiteral("blendMode"), blendModeToString(layer.blendMode));
    o.insert(QStringLiteral("transformX"), layer.transform.position.x());
    o.insert(QStringLiteral("transformY"), layer.transform.position.y());
    o.insert(QStringLiteral("rotation"), layer.transform.rotationDeg);
    o.insert(QStringLiteral("scaleX"), layer.transform.scaleX);
    o.insert(QStringLiteral("scaleY"), layer.transform.scaleY);

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
        o.insert(QStringLiteral("boxW"), text.box.width());
        o.insert(QStringLiteral("boxH"), text.box.height());
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

    return root;
}

bool writeArchive(const QString& filePath, const QByteArray& jsonBytes,
                  const Document& doc, QString* error)
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

    if (!assetsOk || !mz_zip_writer_finalize_archive(&zip)) {
        mz_zip_writer_end(&zip);
        QFile::remove(filePath); // never leave a broken file behind
        *error = QStringLiteral("Could not write the project contents.");
        return false;
    }

    mz_zip_writer_end(&zip);
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
    explicit ProjectReader(const QJsonObject& root)
        : m_root(root)
    {
    }

    std::unique_ptr<Document> run(QString* error,
                                  const QHash<QString, QByteArray>& media)
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

            asset.encoded = media.value(file);
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
        text->box = QSizeF(o.value(QStringLiteral("boxW")).toDouble(0.0),
                           o.value(QStringLiteral("boxH")).toDouble(0.0));
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
        layer.m_id = id;
        layer.name = o.value(QStringLiteral("name")).toString();
        layer.visible = o.value(QStringLiteral("visible")).toBool(true);
        layer.locked = o.value(QStringLiteral("locked")).toBool(false);
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
    }

    QJsonObject m_root;
};

bool saveDocument(const Document& doc, const QString& filePath,
                  QString* errorMessage)
{
    const QByteArray bytes =
        QJsonDocument(buildProjectJson(doc)).toJson(QJsonDocument::Indented);

    QString localError;
    if (!writeArchive(filePath, bytes, doc, &localError)) {
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
        const int entryCount = mz_zip_reader_get_num_files(&zip);
        for (int i = 0; i < entryCount; ++i) {
            char name[512];
            if (!mz_zip_reader_get_filename(&zip, static_cast<mz_uint>(i),
                                            name, sizeof(name)))
                continue;
            const QString entryName = QString::fromUtf8(name);

            size_t size = 0;
            if (void* data = mz_zip_reader_extract_to_heap(
                    &zip, static_cast<mz_uint>(i), &size, 0)) {
                const QByteArray entryBytes(static_cast<const char*>(data),
                                            static_cast<qint64>(size));
                if (entryName == QLatin1String(kEntryName))
                    bytes = entryBytes;
                else if (entryName.startsWith(QLatin1String("media/")))
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
    ProjectReader reader(parsed.object());
    auto document = reader.run(&error, media);
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
