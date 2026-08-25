#!/usr/bin/env bash
set -euo pipefail

cd "${1:-.}"
echo ">> Aplicando M3 FINAL em: $(pwd)"

# ============================================================
# 1. Atualizar src/core/layers/Layer.h (acréscimos)
# ============================================================
# Adicionar forward declaration de ProjectReader antes de using LayerId
if ! grep -q "class ProjectReader;" src/core/layers/Layer.h; then
    sed -i '/namespace cc {/a class ProjectReader; // defined in serialization/ProjectFile.cpp' src/core/layers/Layer.h
fi

# Adicionar friend class ProjectReader na seção private
if ! grep -q "friend class ProjectReader;" src/core/layers/Layer.h; then
    sed -i '/private:/a \    friend class ProjectReader;' src/core/layers/Layer.h
fi

# ============================================================
# 2. Criar src/core/serialization/ProjectFile.h
# ============================================================
mkdir -p src/core/serialization

cat > src/core/serialization/ProjectFile.h << 'PFH'
#pragma once

#include <QString>
#include <memory>

namespace cc {

class Document;

inline constexpr int kProjectFormatVersion = 1;

bool saveDocument(const Document& doc, const QString& filePath,
                  QString* errorMessage = nullptr);

std::unique_ptr<Document> loadDocument(const QString& filePath,
                                       QString* errorMessage = nullptr);

} // namespace cc
PFH

# ============================================================
# 3. Criar src/core/serialization/ProjectFile.cpp
# ============================================================
cat > src/core/serialization/ProjectFile.cpp << 'PFC'
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
        QFile::remove(filePath); // never leave a broken file behind
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

        // Unknown type from a newer version: degrade gracefully to a
        // placeholder group that preserves the subtree (Phase 0 rule).
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

std::unique_ptr<Document> loadFail(QString* out, const QString& message,
                                   const QString& detail = {})
{
    qCWarning(lcSerialization) << "loadDocument failed:" << message << detail;
    if (out)
        *out = message;
    return nullptr;
}

} // namespace

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
        return loadFail(errorMessage, QStringLiteral(
            "The file could not be opened as a CreatorCanvas project. "
            "It may be corrupted or in a different format."), filePath);
    }

    QByteArray bytes;
    {
        const int entryIndex =
            mz_zip_reader_locate_file(&zip, kEntryName, nullptr, 0);
        if (entryIndex >= 0) {
            size_t size = 0;
            if (void* data = mz_zip_reader_extract_to_heap(
                    &zip, static_cast<mz_uint>(entryIndex), &size, nullptr)) {
                bytes = QByteArray(static_cast<const char*>(data),
                                   static_cast<qint64>(size));
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
PFC

# ============================================================
# 4. Substituir tests/unit/test_serialization.cpp
# ============================================================
cat > tests/unit/test_serialization.cpp << 'TST'
#include "core/Document.h"
#include "core/serialization/ProjectFile.h"

#include <QFile>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
#include <miniz.h>

#include <cstring>

using namespace cc;

namespace {

std::unique_ptr<TextLayer> makeText(const QString& name)
{
    auto layer = std::make_unique<TextLayer>();
    layer->name = name;
    return layer;
}

bool craftZip(const QString& path, const char* entryName, const QByteArray& bytes)
{
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, QFile::encodeName(path).constData(), 0))
        return false;
    const bool ok =
        mz_zip_writer_add_mem(&zip, entryName, bytes.constData(),
                              static_cast<size_t>(bytes.size()),
                              MZ_DEFAULT_LEVEL)
        && mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);
    return ok;
}

} // namespace

class TestSerialization final : public QObject
{
    Q_OBJECT

private slots:
    void roundTripPreservesEverything()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("test.creatorcanvas");

        Document doc(1280, 720, 96);

        auto bg = std::make_unique<BackgroundLayer>();
        const LayerId bgId = bg->id();
        bg->name = QStringLiteral("Background");
        bg->fill = QColor(16, 16, 20);

        auto group = std::make_unique<GroupLayer>();
        const LayerId groupId = group->id();
        group->name = QStringLiteral("Characters");

        auto text = std::make_unique<TextLayer>();
        const LayerId textId = text->id();
        text->name = QStringLiteral("Title");
        text->content = QStringLiteral("HELLO");
        text->fontFamily = QStringLiteral("Impact");
        text->sizePt = 72.0;
        text->bold = true;
        text->underline = true;
        text->color = QColor(255, 128, 0);
        text->letterSpacingPx = 2.5;
        text->lineHeightMult = 1.2;
        text->align = TextAlignment::Right;

        auto shape = std::make_unique<ShapeLayer>();
        const LayerId shapeId = shape->id();
        shape->name = QStringLiteral("Badge");
        shape->kind = ShapeKind::RoundedRect;
        shape->fill = QColor(10, 200, 30);
        shape->stroke = QColor(255, 255, 255);
        shape->strokeWidth = 3.0;
        shape->cornerRadius = 12.0;

        auto image = std::make_unique<ImageLayer>();
        const LayerId imageId = image->id();
        image->name = QStringLiteral("Photo");
        const LayerId assetId = newLayerId();
        image->assetId = assetId;
        image->naturalWidth = 1920;
        image->naturalHeight = 1080;

        group->children.push_back(std::move(text));
        group->children.push_back(std::move(shape));

        QVERIFY(doc.addLayer(std::move(bg)));
        QVERIFY(doc.addLayer(std::move(group)));
        QVERIFY(doc.addLayer(std::move(image)));

        QString error;
        QVERIFY(saveDocument(doc, path, &error));

        auto loaded = loadDocument(path, &error);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->width(), 1280);
        QCOMPARE(loaded->height(), 720);
        QCOMPARE(loaded->dpi(), 96);

        QVERIFY(loaded->rootGroup()->id() != doc.rootGroup()->id());

        const GroupLayer* root = loaded->rootGroup();
        QCOMPARE(root->children.size(), std::size_t(3));
        QVERIFY(root->children[0]->id() == bgId);
        QVERIFY(root->children[1]->id() == groupId);
        QVERIFY(root->children[2]->id() == imageId);

        auto* loadedBg = static_cast<BackgroundLayer*>(root->children[0].get());
        QCOMPARE(loadedBg->name, QStringLiteral("Background"));
        QCOMPARE(loadedBg->fill, QColor(16, 16, 20));

        auto* loadedGroup = static_cast<GroupLayer*>(root->children[1].get());
        QCOMPARE(loadedGroup->name, QStringLiteral("Characters"));
        QCOMPARE(loadedGroup->children.size(), std::size_t(2));
        QVERIFY(loadedGroup->children[0]->id() == textId);
        QVERIFY(loadedGroup->children[1]->id() == shapeId);

        auto* loadedText = static_cast<TextLayer*>(loadedGroup->children[0].get());
        QCOMPARE(loadedText->content, QStringLiteral("HELLO"));
        QCOMPARE(loadedText->fontFamily, QStringLiteral("Impact"));
        QCOMPARE(loadedText->sizePt, 72.0);
        QCOMPARE(loadedText->bold, true);
        QCOMPARE(loadedText->underline, true);
        QCOMPARE(loadedText->color, QColor(255, 128, 0));
        QCOMPARE(loadedText->letterSpacingPx, 2.5);
        QCOMPARE(loadedText->lineHeightMult, 1.2);
        QCOMPARE(static_cast<int>(loadedText->align),
                 static_cast<int>(TextAlignment::Right));

        auto* loadedShape = static_cast<ShapeLayer*>(loadedGroup->children[1].get());
        QCOMPARE(static_cast<int>(loadedShape->kind),
                 static_cast<int>(ShapeKind::RoundedRect));
        QCOMPARE(loadedShape->strokeWidth, 3.0);
        QCOMPARE(loadedShape->cornerRadius, 12.0);
        QCOMPARE(loadedShape->fill, QColor(10, 200, 30));
        QCOMPARE(loadedShape->stroke, QColor(255, 255, 255));

        auto* loadedImage = static_cast<ImageLayer*>(root->children[2].get());
        QCOMPARE(loadedImage->assetId, assetId);
        QCOMPARE(loadedImage->naturalWidth, 1920);
        QCOMPARE(loadedImage->naturalHeight, 1080);
    }

    void roundTripEmptyDocument()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("empty.creatorcanvas");

        Document doc(500, 500, 72);
        QString error;
        QVERIFY(saveDocument(doc, path, &error));

        auto loaded = loadDocument(path, &error);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->width(), 500);
        QCOMPARE(loaded->height(), 500);
        QCOMPARE(loaded->dpi(), 72);
        QCOMPARE(loaded->rootGroup()->children.size(), std::size_t(0));
    }

    void roundTripSpecialValues()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("special.creatorcanvas");

        Document doc(100, 100);
        auto text = makeText("T");
        text->visible = false;
        text->locked = true;
        text->setOpacity(0.25f);
        text->blendMode = BlendMode::Add;
        text->color = QColor(10, 20, 30, 40);
        QVERIFY(doc.addLayer(std::move(text)));

        QString error;
        QVERIFY(saveDocument(doc, path, &error));
        auto loaded = loadDocument(path, &error);
        QVERIFY(loaded != nullptr);

        auto* layer = static_cast<TextLayer*>(loaded->rootGroup()->children[0].get());
        QCOMPARE(layer->visible, false);
        QCOMPARE(layer->locked, true);
        QCOMPARE(layer->opacity(), 0.25f);
        QCOMPARE(static_cast<int>(layer->blendMode), static_cast<int>(BlendMode::Add));
        QCOMPARE(layer->color, QColor(10, 20, 30, 40));
    }

    void notAZipRejected()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("garbage.creatorcanvas");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is plain text, not a zip archive");
        f.close();

        QString error;
        QVERIFY(loadDocument(path, &error) == nullptr);
        QVERIFY(!error.isEmpty());
    }

    void missingEntryRejected()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("noentry.creatorcanvas");
        QVERIFY(craftZip(path, "readme.txt", QByteArray("hello")));

        QString error;
        QVERIFY(loadDocument(path, &error) == nullptr);
        QVERIFY(!error.isEmpty());
    }

    void corruptJsonRejected()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("badjson.creatorcanvas");
        QVERIFY(craftZip(path, "project.json", QByteArray("this is not { json")));

        QString error;
        QVERIFY(loadDocument(path, &error) == nullptr);
        QVERIFY(!error.isEmpty());
    }

    void futureVersionRejected()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("future.creatorcanvas");
        const QByteArray json = R"({
            "formatVersion": 999,
            "document": {"width": 800, "height": 600, "dpi": 96},
            "layers": []
        })";
        QVERIFY(craftZip(path, "project.json", json));

        QString error;
        QVERIFY(loadDocument(path, &error) == nullptr);
        QVERIFY(!error.isEmpty());
    }

    void unknownLayerTypeDegradesToPlaceholder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("unknown.creatorcanvas");

        const QString placeholderId =
            QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString childId =
            QUuid::createUuid().toString(QUuid::WithoutBraces);

        const QByteArray json = QString(R"({
            "formatVersion": 1,
            "document": {"width": 800, "height": 600, "dpi": 96},
            "layers": [
                {"type": "hologram", "id": "%1", "name": "Future",
                 "visible": false, "locked": true, "opacity": 0.5,
                 "blendMode": "normal",
                 "children": [
                     {"type": "text", "id": "%2", "name": "Kid",
                      "content": "Hi"}
                 ]}
            ]
        })").arg(placeholderId, childId).toUtf8();

        QVERIFY(craftZip(path, "project.json", json));

        QString error;
        auto doc = loadDocument(path, &error);
        QVERIFY(doc != nullptr);

        QCOMPARE(doc->rootGroup()->children.size(), std::size_t(1));
        Layer* placeholder = doc->rootGroup()->children[0].get();
        QVERIFY(placeholder->id() == LayerId(placeholderId));
        QCOMPARE(placeholder->type(), LayerType::Group);
        QCOMPARE(placeholder->name, QStringLiteral("Future"));
        QCOMPARE(placeholder->visible, false);
        QCOMPARE(placeholder->locked, true);
        QCOMPARE(placeholder->opacity(), 0.5f);

        auto* group = static_cast<GroupLayer*>(placeholder);
        QCOMPARE(group->children.size(), std::size_t(1));
        QVERIFY(group->children[0]->id() == LayerId(childId));
        QCOMPARE(static_cast<TextLayer*>(group->children[0].get())->content,
                 QStringLiteral("Hi"));
    }

    void saveToEmptyPathFails()
    {
        Document doc(100, 100);
        QString error;
        QVERIFY(!saveDocument(doc, QString(), &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSerialization)
#include "test_serialization.moc"
TST

# ============================================================
# 5. Atualizar CMakeLists.txt raiz (adicionar LANGUAGES C CXX)
# ============================================================
if ! grep -q "LANGUAGES C CXX" CMakeLists.txt; then
    sed -i 's/project(CreatorCanvas VERSION 0.1.0)/project(CreatorCanvas VERSION 0.1.0 LANGUAGES C CXX)/' CMakeLists.txt
fi

# ============================================================
# 6. Substituir src/CMakeLists.txt
# ============================================================
cat > src/CMakeLists.txt << 'SRC_CMAKE'
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ------------------------------------------------------------ 3rdparty: miniz --
add_library(miniz STATIC ${PROJECT_SOURCE_DIR}/3rdparty/miniz/miniz.c)
target_include_directories(miniz SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/3rdparty/miniz)

# --------------------------------------------------------------------- core --
add_library(cc_core STATIC
    core/layers/Layer.h
    core/layers/Layer.cpp
    core/Document.h
    core/Document.cpp
    core/history/Command.h
    core/history/Command.cpp
    core/history/CommandStack.h
    core/history/CommandStack.cpp
    core/history/DocumentCommands.h
    core/serialization/ProjectFile.h
    core/serialization/ProjectFile.cpp
)
target_link_libraries(cc_core PUBLIC Qt6::Core Qt6::Gui miniz)
target_compile_definitions(cc_core PRIVATE CC_PROJECT_VERSION="${PROJECT_VERSION}")
target_include_directories(cc_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
cc_enable_warnings(cc_core)

# ---------------------------------------------------------------- services --
add_library(cc_services STATIC
    services/logservice.h
    services/logservice.cpp
    services/settingsservice.h
    services/settingsservice.cpp
)
target_link_libraries(cc_services PUBLIC Qt6::Core)
target_include_directories(cc_services PUBLIC ${PROJECT_SOURCE_DIR}/src)
cc_enable_warnings(cc_services)

# ------------------------------------------------------------ localization --
add_library(cc_localization STATIC
    localization/i18nservice.h
    localization/i18nservice.cpp
    ${PROJECT_SOURCE_DIR}/resources/locales.qrc
)
target_link_libraries(cc_localization PUBLIC Qt6::Core cc_services)
target_include_directories(cc_localization PUBLIC ${PROJECT_SOURCE_DIR}/src)
cc_enable_warnings(cc_localization)

# ------------------------------------------------------------- application --
add_executable(creatorcanvas
    main.cpp
    ui/darktheme.h
    ui/darktheme.cpp
    ui/mainwindow.h
    ui/mainwindow.cpp
)
target_link_libraries(creatorcanvas
    PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets
            cc_core cc_services cc_localization)
target_include_directories(creatorcanvas PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(creatorcanvas PRIVATE APP_VERSION="${PROJECT_VERSION}")
set_target_properties(creatorcanvas PROPERTIES
    OUTPUT_NAME "CreatorCanvas"
    WIN32_EXECUTABLE $<BOOL:WIN32>
    MACOSX_BUNDLE TRUE)
cc_enable_warnings(creatorcanvas)
SRC_CMAKE

# ============================================================
# 7. Substituir tests/CMakeLists.txt
# ============================================================
cat > tests/CMakeLists.txt << 'TESTS_CMAKE'
function(cc_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE Qt6::Test cc_core cc_services cc_localization)
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    cc_enable_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

cc_add_test(test_settings      unit/test_settings.cpp)
cc_add_test(test_i18n          unit/test_i18n.cpp)
cc_add_test(test_logging       unit/test_logging.cpp)
cc_add_test(test_layers        unit/test_layers.cpp)
cc_add_test(test_document      unit/test_document.cpp)
cc_add_test(test_history       unit/test_history.cpp)
cc_add_test(test_serialization unit/test_serialization.cpp)
TESTS_CMAKE

# ============================================================
# 8. Atualizar README
# ============================================================
sed -i 's/| M3 | Serialization (.creatorcanvas) | .* |/| M3 | Serialization (.creatorcanvas) | Code complete |/' README.md

echo ""
echo ">> M3 FINAL aplicado com sucesso!"
echo ">> Agora compile:"
echo "   cmake --preset linux-debug"
echo "   cmake --build --preset linux-debug"
echo "   ctest --test-dir build/linux-debug --output-on-failure"
