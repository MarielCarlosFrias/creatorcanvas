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

void appendU16(QByteArray& d, quint16 v)
{
    d.append(char(v >> 8));
    d.append(char(v & 0xff));
}

void appendU32(QByteArray& d, quint32 v)
{
    d.append(char(v >> 24));
    d.append(char((v >> 16) & 0xff));
    d.append(char((v >> 8) & 0xff));
    d.append(char(v & 0xff));
}

/// Monta um PSD mínimo (8/16-bit RGB, só composite) com dados raw ou RLE.
/// planes: w*h*3 amostras, plano a plano (R, G, B); em 16-bit cada amostra
/// ocupa dois bytes big-endian.
QByteArray makePsdBytes(int w, int h, const QByteArray& planes, bool rle,
                        quint16 depth = 8)
{
    QByteArray d;
    d.append("8BPS", 4);
    appendU16(d, 1);          // versão
    d.append(QByteArray(6, '\0')); // reservado
    appendU16(d, 3);          // 3 canais (RGB)
    appendU32(d, quint32(h));
    appendU32(d, quint32(w));
    appendU16(d, depth);      // depth
    appendU16(d, 3);          // modo RGB
    appendU32(d, 0);          // color mode data
    appendU32(d, 0);          // image resources
    appendU32(d, 0);          // layer/mask
    appendU16(d, rle ? 1 : 0); // compressão
    if (!rle) {
        d.append(planes);
    } else {
        // Tabela de contagens: h linhas × 3 canais. Cada linha PackBits é um
        // literal run de w*bytesPerSample bytes (válido para estes PSDs pequenos).
        const int bytesPerSample = depth / 8;
        const int sampleBytes = w * bytesPerSample;
        const int rowBytes = 1 + sampleBytes;
        for (int c = 0; c < 3; ++c)
            for (int y = 0; y < h; ++y)
                appendU16(d, quint16(rowBytes));
        for (int c = 0; c < 3; ++c) {
            for (int y = 0; y < h; ++y) {
                d.append(char(sampleBytes - 1));
                d.append(planes.mid((c * h + y) * sampleBytes, sampleBytes));
            }
        }
    }
    return d;
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

    void importPsdRawRgb()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // 2x2 RGB: plano R = [10,20,30,40], G = [50,60,70,80], B = [90,100,110,120]
        QByteArray planes;
        for (int v : {10, 20, 30, 40}) planes.append(char(v));
        for (int v : {50, 60, 70, 80}) planes.append(char(v));
        for (int v : {90, 100, 110, 120}) planes.append(char(v));
        const QString path = dir.filePath("img.psd");
        QVERIFY(writeFile(path, makePsdBytes(2, 2, planes, false)));

        const auto result = importImageFromFile(path);
        QCOMPARE(static_cast<int>(result.status), static_cast<int>(ImportStatus::Ok));
        QCOMPARE(result.image.width(), 2);
        QCOMPARE(result.image.height(), 2);
        QCOMPARE(result.image.pixelColor(0, 0), QColor(10, 50, 90));
        QCOMPARE(result.image.pixelColor(1, 1), QColor(40, 80, 120));
        QCOMPARE(qAlpha(result.image.pixel(0, 0)), 255);
        // re-codificado como PNG para o AssetStore
        QCOMPARE(result.format, QStringLiteral("png"));
    }

    void importPsdRleRgb()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QByteArray planes;
        for (int c = 0; c < 3; ++c)
            for (int i = 0; i < 12; ++i)
                planes.append(char(10 + c * 40 + i));
        const QString path = dir.filePath("rle.psd");
        QVERIFY(writeFile(path, makePsdBytes(4, 3, planes, true)));

        const auto result = importImageFromFile(path);
        QCOMPARE(static_cast<int>(result.status), static_cast<int>(ImportStatus::Ok));
        QCOMPARE(result.image.width(), 4);
        QCOMPARE(result.image.height(), 3);
        QCOMPARE(result.image.pixelColor(0, 0), QColor(10, 50, 90));
        QCOMPARE(result.image.pixelColor(3, 2), QColor(10 + 11, 50 + 11, 90 + 11));
    }

    void importPsdRgb16RawAndRle_data()
    {
        QTest::addColumn<bool>("rle");
        QTest::newRow("raw") << false;
        QTest::newRow("packbits") << true;
    }

    void importPsdRgb16RawAndRle()
    {
        QFETCH(bool, rle);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // Dois píxeis RGB de 16 bits. Os bytes baixos são deliberadamente
        // diferentes: a redução para 8 bits deve escolher os bytes altos.
        QByteArray planes;
        for (quint16 sample : {quint16(0x1234), quint16(0xabcd),
                               quint16(0x2244), quint16(0xbcde),
                               quint16(0x3355), quint16(0xcdef)})
            appendU16(planes, sample);
        const QString path = dir.filePath(rle ? QStringLiteral("rgb16-rle.psd")
                                              : QStringLiteral("rgb16-raw.psd"));
        QVERIFY(writeFile(path, makePsdBytes(2, 1, planes, rle, 16)));

        const auto result = importImageFromFile(path);
        QVERIFY2(result.status == ImportStatus::Ok, qPrintable(result.errorMessage));
        QCOMPARE(result.image.size(), QSize(2, 1));
        QCOMPARE(result.image.pixelColor(0, 0), QColor(0x12, 0x22, 0x33));
        QCOMPARE(result.image.pixelColor(1, 0), QColor(0xab, 0xbc, 0xcd));
    }

    void importPsdRejectsGarbage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // assinatura errada
        QString path = dir.filePath("bad1.psd");
        QVERIFY(writeFile(path, QByteArray("XXXX" + QByteArray(100, '\0'))));
        QCOMPARE(static_cast<int>(importImageFromFile(path).status),
                 static_cast<int>(ImportStatus::UnsupportedFormat));

        // assinatura certa mas truncado
        path = dir.filePath("bad2.psd");
        QVERIFY(writeFile(path, QByteArray("8BPS") + QByteArray(4, '\0')));
        QCOMPARE(static_cast<int>(importImageFromFile(path).status),
                 static_cast<int>(ImportStatus::CorruptImage));

        // PSD válido no header mas sem dados de imagem
        QByteArray planes;
        path = dir.filePath("bad3.psd");
        QByteArray truncated = makePsdBytes(2, 2, planes, false);
        truncated.chop(6); // corta o fim dos dados raw
        QVERIFY(writeFile(path, truncated));
        QCOMPARE(static_cast<int>(importImageFromFile(path).status),
                 static_cast<int>(ImportStatus::CorruptImage));
    }
};

QTEST_GUILESS_MAIN(TestImport)
#include "test_import.moc"
