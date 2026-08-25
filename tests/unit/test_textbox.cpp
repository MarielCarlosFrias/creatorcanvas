#include "core/Document.h"
#include "core/layers/Layer.h"
#include "core/serialization/ProjectFile.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace cc;

class TestTextBox final : public QObject
{
    Q_OBJECT

private slots:
    void setLayerTextBoxChangesBounds()
    {
        Document doc(200, 200);
        auto layer = std::make_unique<TextLayer>();
        layer->content = QStringLiteral("Hello");
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        QVERIFY(doc.setLayerTextBox(id, QSizeF(300, 120)));
        auto* text = static_cast<TextLayer*>(doc.findLayer(id));
        QCOMPARE(text->contentBounds(), QRectF(0, 0, 300, 120));
    }

    void emptyBoxFallsBackToAutoSize()
    {
        Document doc(200, 200);
        auto layer = std::make_unique<TextLayer>();
        layer->content = QStringLiteral("Hello");
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        QVERIFY(doc.setLayerTextBox(id, QSizeF())); // empty = auto
        auto* text = static_cast<TextLayer*>(doc.findLayer(id));
        QVERIFY(!text->contentBounds().isEmpty()); // metrics-based estimate
    }

    void boxRoundTripsThroughSerialization()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("box.creatorcanvas");

        Document doc(400, 400);
        auto layer = std::make_unique<TextLayer>();
        layer->content = QStringLiteral("Wrapped text");
        layer->box = QSizeF(300, 120);
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        QString error;
        QVERIFY(saveDocument(doc, path, &error));
        auto loaded = loadDocument(path, &error);
        QVERIFY(loaded != nullptr);

        auto* loadedLayer = static_cast<TextLayer*>(loaded->findLayer(id));
        QVERIFY(loadedLayer != nullptr);
        QCOMPARE(loadedLayer->box, QSizeF(300, 120));
        QCOMPARE(loadedLayer->contentBounds(), QRectF(0, 0, 300, 120));
    }
};

QTEST_GUILESS_MAIN(TestTextBox)
#include "test_textbox.moc"
