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

         // With the transform system, the rect (points 25..75) renders
        // centered on its transform position (0,0): doc rect (-25..25)^2.
        const QColor inside = image.pixelColor(10, 10);
        QVERIFY(qAbs(inside.red() - 128) <= 1);
        QCOMPARE(inside.green(), 0);
        QVERIFY(qAbs(inside.blue() - 128) <= 1);

        // Outside the rect: pure red.
        QCOMPARE(image.pixelColor(50, 50), QColor(255, 0, 0));

        // Outside the rect: pure red.
        QCOMPARE(image.pixelColor(50, 50), QColor(255, 0, 0));
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

    void dropShadowCachingAndClearCaches()
    {
        Document doc(100, 100);
        QImage bitmap(40, 40, QImage::Format_ARGB32_Premultiplied);
        bitmap.fill(Qt::blue);
        const LayerId assetId = doc.assets().addImage(bitmap);

        auto image = std::make_unique<ImageLayer>();
        image->assetId = assetId;
        image->naturalWidth = 40;
        image->naturalHeight = 40;
        image->effects.dropShadow.enabled = true;
        image->effects.dropShadow.distance = 5.0;
        image->effects.dropShadow.blur = 4.0;
        image->effects.dropShadow.color = QColor(Qt::black);
        image->effects.dropShadow.opacity = 0.8;
        QVERIFY(doc.addLayer(std::move(image)));

        QImage target1(100, 100, QImage::Format_ARGB32_Premultiplied);
        target1.fill(Qt::transparent);
        renderTo(target1, doc);

        // Subsequent render uses cached drop shadow and must produce identical output
        QImage target2(100, 100, QImage::Format_ARGB32_Premultiplied);
        target2.fill(Qt::transparent);
        renderTo(target2, doc);

        QCOMPARE(target1, target2);

        // Clearing caches does not break subsequent rendering
        clearRendererCaches();

        QImage target3(100, 100, QImage::Format_ARGB32_Premultiplied);
        target3.fill(Qt::transparent);
        renderTo(target3, doc);

        QCOMPARE(target1, target3);
    }

    void checkerboardCachingAndClippingComposition()
    {
        Document doc(60, 60);
        RenderOptions opts;
        opts.drawCheckerboard = true;
        opts.checkerSize = 8;

        QImage img1(60, 60, QImage::Format_ARGB32_Premultiplied);
        img1.fill(Qt::transparent);
        renderTo(img1, doc, opts);

        // Second render should use cached brush and match pixel-for-pixel
        QImage img2(60, 60, QImage::Format_ARGB32_Premultiplied);
        img2.fill(Qt::transparent);
        renderTo(img2, doc, opts);
        QCOMPARE(img1, img2);

        // Add a base layer and a clipped layer to exercise composite composition with reused temp buffer
        auto base = std::make_unique<ShapeLayer>();
        base->kind = ShapeKind::Rectangle;
        base->fill = Qt::red;
        base->points = QPolygonF({QPointF(10, 10), QPointF(50, 50)});
        base->transform.position = QPointF(30, 30);
        QVERIFY(doc.addLayer(std::move(base)));

        auto clipped = std::make_unique<ShapeLayer>();
        clipped->kind = ShapeKind::Rectangle;
        clipped->fill = Qt::yellow;
        clipped->points = QPolygonF({QPointF(0, 0), QPointF(60, 60)});
        clipped->transform.position = QPointF(30, 30);
        clipped->clipped = true;
        QVERIFY(doc.addLayer(std::move(clipped)));

        QImage compImg(60, 60, QImage::Format_ARGB32_Premultiplied);
        compImg.fill(Qt::transparent);
        renderTo(compImg, doc, opts);

        // Outside base bounds (e.g. 5, 5), clipped layer should NOT show because alpha was 0
        // Inside base bounds (e.g. 30, 30), clipped layer should show (yellow)
        QCOMPARE(compImg.pixelColor(30, 30), QColor(Qt::yellow));

        clearRendererCaches();
    }
};

QTEST_GUILESS_MAIN(TestRenderer)
#include "test_renderer.moc"
