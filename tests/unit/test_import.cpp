#include "core/Document.h"
#include "core/assets/AssetStore.h"
#include "core/layers/Layer.h"
#include "core/serialization/ProjectFile.h"
#include "imageio/ImageImporter.h"

#include <QBuffer>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace cc;

namespace {

QByteArray makePngBytes(int w = 8, int h = 6, QColor color = {255, 0, 0})
{
    QImage image(w, h, QImage::Format_ARGB32);
    image.fill(color);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

QByteArray makeJpegBytes(int w = 8, int h = 6)
{
    QImage image(w, h, QImage::Format_ARGB32);
    image.fill(QColor(0, 128, 255));
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "JPEG");
    return bytes;
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(bytes) == bytes.size();
}

} // namespace

class TestImport final : public QObject
{
    Q_OBJECT

private slots:
    void importValidPngByContent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("photo.png");
        const QByteArray bytes = makePngBytes();
        QVERIFY(writeFile(path, bytes));

        const auto result = importImageFromFile(path);
        QCOMPARE(static_cast<int>(result.status), static_cast<int>(ImportStatus::Ok));
        QCOMPARE(result.format, QStringLiteral("png"));
        QCOMPARE(result.image.width(), 8);
        QCOMPARE(result.image.height(), 6);
        QCOMPARE(result.encoded, bytes);
    }

    void formatSniffingIgnoresExtension()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("actually_jpeg.png"); // lying extension
        QVERIFY(writeFile(path, makeJpegBytes()));

        const auto result = importImageFromFile(path);
        QCOMPARE(static_cast<int>(result.status), static_cast<int>(ImportStatus::Ok));
        QCOMPARE(result.format, QStringLiteral("jpeg"));
    }

    void rejectsNonImageBytes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("text.png");
        QVERIFY(writeFile(path, QByteArray("just some text, no image here")));

        const auto result = importImageFromFile(path);
        QCOMPARE(static_cast<int>(result.status),
                 static_cast<int>(ImportStatus::UnsupportedFormat));
    }

    void rejectsCorruptPng()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("corrupt.png");
        QByteArray bytes = makePngBytes();
        bytes.chop(bytes.size() / 2); // truncated payload, valid magic
        QVERIFY(writeFile(path, bytes));

        const auto result = importImageFromFile(path);
        QCOMPARE(static_cast<int>(result.status),
                 static_cast<int>(ImportStatus::CorruptImage));
    }

    void rejectsMissingFile()
    {
        const auto result = importImageFromFile("/nonexistent/path/x.png");
        QCOMPARE(static_cast<int>(result.status),
                 static_cast<int>(ImportStatus::FileNotFound));
    }

    void rejectsOversizedFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("big.png");
        QVERIFY(writeFile(path, makePngBytes(64, 64)));

        const auto result = importImageFromFile(path, /*maxFileBytes=*/10);
        QCOMPARE(static_cast<int>(result.status),
                 static_cast<int>(ImportStatus::TooLarge));
    }

    void assetStoreRegistersAndDecodes()
    {
        AssetStore store;
        const QByteArray bytes = makePngBytes(8, 6, QColor(10, 200, 30));
        const LayerId id = store.add(bytes, QStringLiteral("png"));
        QVERIFY(!id.isNull());

        const Asset* asset = store.find(id);
        QVERIFY(asset != nullptr);
        QCOMPARE(asset->format, QStringLiteral("png"));
        QCOMPARE(asset->width, 8);
        QCOMPARE(asset->height, 6);
        QCOMPARE(asset->sha256.size(), 64);
        QCOMPARE(store.decodedImage(id).pixelColor(3, 3), QColor(10, 200, 30));
    }

    void serializationRoundTripWithAsset()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("withasset.creatorcanvas");

        Document doc(200, 200);
        const QByteArray bytes = makePngBytes(16, 16, QColor(5, 100, 200));
        const LayerId assetId = doc.assets().add(bytes, QStringLiteral("png"));
        QVERIFY(!assetId.isNull());

        auto layer = std::make_unique<ImageLayer>();
        layer->name = QStringLiteral("Photo");
        layer->assetId = assetId;
        layer->naturalWidth = 16;
        layer->naturalHeight = 16;
        QVERIFY(doc.addLayer(std::move(layer)));

        QString error;
        QVERIFY(saveDocument(doc, path, &error));

        auto loaded = loadDocument(path, &error);
        QVERIFY(loaded != nullptr);

        // The ASSET round-trips: same id, original bytes, matching hash.
        QCOMPARE(loaded->assets().assets().size(), 1);
        const Asset& asset = loaded->assets().assets()[0];
        QCOMPARE(asset.id, assetId);
        QCOMPARE(asset.encoded, bytes);
        QCOMPARE(asset.sha256, doc.assets().find(assetId)->sha256);

        // The LAYER keeps its own id; its asset REFERENCE must survive.
        QCOMPARE(loaded->rootGroup()->children.size(), std::size_t(1));
        auto* loadedLayer = static_cast<ImageLayer*>(
            loaded->rootGroup()->children[0].get());
        QCOMPARE(loadedLayer->name, QStringLiteral("Photo"));
        QCOMPARE(loadedLayer->assetId, assetId);
        QCOMPARE(loadedLayer->naturalWidth, 16);
        QCOMPARE(loadedLayer->naturalHeight, 16);
        QCOMPARE(loaded->assets().decodedImage(assetId).pixelColor(0, 0),
                 QColor(5, 100, 200));
    }
};

QTEST_GUILESS_MAIN(TestImport)
#include "test_import.moc"
