#include "core/Document.h"
#include "core/geometry/AffineTransform.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "core/layers/Layer.h"
#include "core/serialization/ProjectFile.h"
#include "rendering/CanvasRenderer.h"

#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace cc;

namespace {

QByteArray makePngBytes(int w, int h, const QColor& color)
{
    QImage image(w, h, QImage::Format_ARGB32);
    image.fill(color);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

} // namespace

class TestTransform final : public QObject
{
    Q_OBJECT

private slots:
    void matrixMapsCenterToPosition()
    {
        const QRectF bounds(0, 0, 100, 50);
        const AffineTransform identity;
        QCOMPARE(identity.matrix(bounds).map(bounds.center()), QPointF(0, 0));

        AffineTransform moved;
        moved.position = QPointF(200, 300);
        QCOMPARE(moved.matrix(bounds).map(bounds.center()), QPointF(200, 300));
    }

    void rotationIsClockwiseAroundCenter()
    {
        const QRectF bounds(-50, -25, 100, 50);
        AffineTransform t;
        t.rotationDeg = 90.0;
        const QPointF mapped = t.matrix(bounds).map(bounds.topLeft());
        QVERIFY(qAbs(mapped.x() - 25) < 1e-9);
        QVERIFY(qAbs(mapped.y() + 50) < 1e-9);
    }

    void negativeScaleFlips()
    {
        const QRectF bounds(0, 0, 100, 10);
        AffineTransform t;
        t.position = QPointF(50, 5);
        t.scaleX = -1.0;
        QCOMPARE(t.matrix(bounds).map(QPointF(0, 5)), QPointF(100, 5));
    }

    void documentSetLayerTransformEmitsOnce()
    {
        Document doc(100, 100);
        auto layer = std::make_unique<ImageLayer>();
        layer->naturalWidth = 10;
        layer->naturalHeight = 10;
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        QSignalSpy spy(&doc, &Document::layerPropertyChanged);
        AffineTransform t;
        t.position = QPointF(40, 40);
        t.rotationDeg = 15.0;
        QVERIFY(doc.setLayerTransform(id, t));
        QCOMPARE(spy.count(), 1);
        QVERIFY(doc.setLayerTransform(id, t));
        QCOMPARE(spy.count(), 1);
    }

    void transformCommandUndoRedo()
    {
        Document doc(100, 100);
        auto layer = std::make_unique<ImageLayer>();
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        CommandStack stack;
        const AffineTransform oldT;
        AffineTransform newT;
        newT.position = QPointF(30, 30);
        newT.rotationDeg = 45.0;
        stack.execute(
            std::make_unique<SetLayerTransformCommand>(doc, id, oldT, newT));

        QCOMPARE(doc.findLayer(id)->transform, newT);
        stack.undo();
        QCOMPARE(doc.findLayer(id)->transform, oldT);
        stack.redo();
        QCOMPARE(doc.findLayer(id)->transform, newT);
    }

    void transformRoundTripsThroughSerialization()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("t.creatorcanvas");

        Document doc(200, 200);
        auto layer = std::make_unique<TextLayer>();
        layer->content = QStringLiteral("Hi");
        layer->transform.position = QPointF(120, 80);
        layer->transform.rotationDeg = -30.0;
        layer->transform.scaleX = 1.5;
        layer->transform.scaleY = -2.0;
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        QString error;
        QVERIFY(saveDocument(doc, path, &error));
        auto loaded = loadDocument(path, &error);
        QVERIFY(loaded != nullptr);

        const Layer* loadedLayer = loaded->findLayer(id);
        QVERIFY(loadedLayer != nullptr);
        QCOMPARE(loadedLayer->transform.position, QPointF(120, 80));
        QCOMPARE(loadedLayer->transform.rotationDeg, -30.0);
        QCOMPARE(loadedLayer->transform.scaleX, 1.5);
        QCOMPARE(loadedLayer->transform.scaleY, -2.0);
    }

    void groupBoundsComposeChildTransforms()
    {
        auto group = std::make_unique<GroupLayer>();
        auto child = std::make_unique<ImageLayer>();
        child->naturalWidth = 10;
        child->naturalHeight = 10;
        child->transform.position = QPointF(100, 100);
        group->children.push_back(std::move(child));

        QCOMPARE(group->contentBounds(), QRectF(95, 95, 10, 10));
    }

    void rendererAppliesTransform()
    {
        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(255, 0, 0);
        QVERIFY(doc.addLayer(std::move(bg)));

        auto image = std::make_unique<ImageLayer>();
        image->assetId = doc.assets().add(
            makePngBytes(10, 10, QColor(0, 0, 255)), QStringLiteral("png"));
        image->naturalWidth = 10;
        image->naturalHeight = 10;
        image->transform.position = QPointF(50, 50);
        QVERIFY(doc.addLayer(std::move(image)));

        QImage target(100, 100, QImage::Format_RGB32);
        target.fill(Qt::black);
        RenderOptions options;
        options.drawCheckerboard = false;
        QPainter painter(&target);
        renderDocument(doc, &painter, QTransform(), options);
        painter.end();

        QCOMPARE(target.pixelColor(50, 50), QColor(0, 0, 255));
        QCOMPARE(target.pixelColor(10, 10), QColor(255, 0, 0));
    }
};

QTEST_GUILESS_MAIN(TestTransform)
#include "test_transform.moc"
