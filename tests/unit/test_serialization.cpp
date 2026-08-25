#include <miniz.h>
#include "core/Document.h"
#include "core/serialization/ProjectFile.h"

#include <QFile>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

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
