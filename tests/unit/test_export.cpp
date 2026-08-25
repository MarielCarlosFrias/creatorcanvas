#include "core/Document.h"
#include "core/layers/Layer.h"
#include "imageio/DocumentExporter.h"

#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

using namespace cc;

class TestExport final : public QObject
{
    Q_OBJECT

private slots:
    void exportsPngWithContent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("out.png");

        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(200, 30, 30);
        QVERIFY(doc.addLayer(std::move(bg)));

        QString error;
        QVERIFY(exportDocumentToImage(doc, path, QStringLiteral("png"),
                                      95, 1.0, &error));

        QImage loaded(path);
        QCOMPARE(loaded.width(), 100);
        QCOMPARE(loaded.height(), 100);
        QCOMPARE(loaded.pixelColor(50, 50), QColor(200, 30, 30));
    }

    void exportScaleMultipliesSize()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("big.png");

        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(Qt::white);
        QVERIFY(doc.addLayer(std::move(bg)));

        QString error;
        QVERIFY(exportDocumentToImage(doc, path, QStringLiteral("png"),
                                      95, 2.0, &error));

        QImage loaded(path);
        QCOMPARE(loaded.width(), 200);
        QCOMPARE(loaded.height(), 200);
    }

    void exportJpegFlattensAlpha()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("out.jpg");

        Document doc(100, 100); // no layers = fully transparent
        QString error;
        QVERIFY(exportDocumentToImage(doc, path, QStringLiteral("jpeg"),
                                      95, 1.0, &error));

        QImage loaded(path);
        QCOMPARE(loaded.pixelColor(50, 50).alpha(), 255); // flattened
    }

    void exportToBadPathFails()
    {
        Document doc(100, 100);
        QString error;
        QVERIFY(!exportDocumentToImage(doc, QString(), QStringLiteral("png"),
                                       95, 1.0, &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestExport)
#include "test_export.moc"
