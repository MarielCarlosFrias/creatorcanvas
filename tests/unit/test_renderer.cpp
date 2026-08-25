#include "core/Document.h"
#include "core/layers/Layer.h"
#include "rendering/CanvasRenderer.h"

#include <QImage>
#include <QPainter>
#include <QtTest>

using namespace cc;

namespace {

void renderTo(QImage& image, const Document& doc, const RenderOptions& options = {})
{
    QPainter painter(&image);
    renderDocument(doc, &painter, QTransform(), options);
}

} // namespace

class TestRenderer final : public QObject
{
    Q_OBJECT

private slots:
    void backgroundFillsCanvas()
    {
        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(200, 30, 30);
        QVERIFY(doc.addLayer(std::move(bg)));

        QImage image(100, 100, QImage::Format_RGB32);
        image.fill(Qt::black);
        renderTo(image, doc);

        QCOMPARE(image.pixelColor(50, 50), QColor(200, 30, 30));
        QCOMPARE(image.pixelColor(2, 2), QColor(200, 30, 30));
    }

    void invisibleAndZeroOpacityLayersAreSkipped()
    {
        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(200, 30, 30);
        QVERIFY(doc.addLayer(std::move(bg)));

        auto hidden = std::make_unique<ShapeLayer>();
        hidden->visible = false;
        hidden->kind = ShapeKind::Rectangle;
        hidden->fill = QColor(0, 255, 0);
        hidden->points = QPolygonF({QPointF(0, 0), QPointF(100, 100)});
        QVERIFY(doc.addLayer(std::move(hidden)));

        auto transparent = std::make_unique<ShapeLayer>();
        transparent->setOpacity(0.0f);
        transparent->kind = ShapeKind::Rectangle;
        transparent->fill = QColor(0, 255, 0);
        transparent->points = QPolygonF({QPointF(0, 0), QPointF(100, 100)});
        QVERIFY(doc.addLayer(std::move(transparent)));

        QImage image(100, 100, QImage::Format_RGB32);
        image.fill(Qt::black);
        renderTo(image, doc);

        QCOMPARE(image.pixelColor(50, 50), QColor(200, 30, 30));
    }

    void shapeRendersWithOpacityBlend()
    {
        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(255, 0, 0);
        QVERIFY(doc.addLayer(std::move(bg)));

        auto shape = std::make_unique<ShapeLayer>();
        shape->kind = ShapeKind::Rectangle;
        shape->fill = QColor(0, 0, 255);
        shape->setOpacity(0.5f);
        shape->points = QPolygonF({QPointF(25, 25), QPointF(75, 75)});
        QVERIFY(doc.addLayer(std::move(shape)));

        QImage image(100, 100, QImage::Format_RGB32);
        image.fill(Qt::black);
        renderTo(image, doc);

        // Inside: ~50% blue over red. Allow +-1 for rounding.
        const QColor inside = image.pixelColor(50, 50);
        QVERIFY(qAbs(inside.red() - 128) <= 1);
        QCOMPARE(inside.green(), 0);
        QVERIFY(qAbs(inside.blue() - 128) <= 1);

        // Outside the rect: pure red.
        QCOMPARE(image.pixelColor(5, 5), QColor(255, 0, 0));
    }

    void checkerboardShowsThroughTransparentBackground()
    {
        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(0, 0, 0, 0); // fully transparent
        QVERIFY(doc.addLayer(std::move(bg)));

        QImage image(100, 100, QImage::Format_RGB32);
        image.fill(Qt::black);

        RenderOptions options;
        options.checkerSize = 10;
        options.checkerLight = QColor(255, 255, 255);
        options.checkerDark = QColor(0, 0, 0);
        renderTo(image, doc, options);

        QCOMPARE(image.pixelColor(5, 5), QColor(0, 0, 0));
        QCOMPARE(image.pixelColor(15, 5), QColor(255, 255, 255));
    }

    void imagePlaceholderRenders()
    {
        Document doc(100, 100);
        auto image = std::make_unique<ImageLayer>();
        image->naturalWidth = 40;
        image->naturalHeight = 30;
        QVERIFY(doc.addLayer(std::move(image)));

        QImage target(100, 100, QImage::Format_RGB32);
        target.fill(Qt::black);
        RenderOptions options;
        options.drawCheckerboard = false;
        renderTo(target, doc, options);

        QCOMPARE(target.pixelColor(10, 10), QColor(0x3a, 0x3b, 0x40));
    }
};

QTEST_GUILESS_MAIN(TestRenderer)
#include "test_renderer.moc"
