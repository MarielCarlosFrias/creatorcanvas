#include "core/Document.h"
#include "core/layers/Layer.h"
#include "core/snap/SnapEngine.h"

#include <QList>
#include <QRectF>
#include <QtTest>

using namespace cc;

namespace {

std::unique_ptr<ImageLayer> makeImage(const QString& name, int w, int h)
{
    auto layer = std::make_unique<ImageLayer>();
    layer->name = name;
    layer->naturalWidth = w;
    layer->naturalHeight = h;
    return layer;
}

QList<Layer*> allLayersExcept(Document& doc, const LayerId& skip)
{
    QList<Layer*> out;
    for (const auto& child : doc.rootGroup()->children) {
        if (child->id() != skip)
            out.push_back(child.get());
    }
    return out;
}

/// Helper: doc rect in document space (correct way, using transform.matrix).
QRectF docRectOf(const Layer& layer)
{
    const QRectF local = layer.contentBounds();
    return layer.transform.matrix(local).mapRect(local);
}

} // namespace

class TestSnap final : public QObject
{
    Q_OBJECT

private slots:
    void snapToCanvasCenterHAndV()
    {
        // Canvas 100x100. Place a 20x20 image so that its CENTER (since
        // AffineTransform.matrix treats position as the layer's content
        // center) is 4px above and 4px left of canvas center.
        // -> center = (46, 46), so C edge = (56, 56).
        // -> |C=56 - canvas.C=50| = 6 (within threshold) -> snap.
        // -> delta.x = 50 - 56 = -6, delta.y = 50 - 56 = -6.
        Document doc(100, 100);
        auto layer = makeImage("A", 20, 20);
        layer->transform.position = QPointF(46, 46);
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        const QRectF docRect = docRectOf(*doc.findLayer(id));
        const QRectF canvas(0, 0, 100, 100);

        SnapEngine engine;
        const auto result = engine.computeSnap(*doc.findLayer(id),
                                                docRect, canvas, {});

        QCOMPARE(result.delta.x(), 4.0);
        QCOMPARE(result.delta.y(), 4.0);
        QCOMPARE(result.guides.size(), 2);
    }

    void snapToCanvasLeftEdge()
    {
        // Canvas 100x100. Place a 20x20 image so that its L edge is 2px
        // from canvas L edge.
        // center.x = canvas.L + image.W/2 + 2 = 0 + 10 + 2 = 12.
        Document doc(100, 100);
        auto layer = makeImage("A", 20, 20);
        layer->transform.position = QPointF(12, 50);  // L edge = 2
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        const QRectF docRect = docRectOf(*doc.findLayer(id));
        const QRectF canvas(0, 0, 100, 100);

        SnapEngine engine;
        const auto result = engine.computeSnap(*doc.findLayer(id),
                                                docRect, canvas, {});

        QCOMPARE(result.delta.x(), -2.0);
        // Y: center 50 vs canvas center 50 -> snap, delta.y = 0
        // So we expect 2 guides (1V + 1H).
        QVERIFY(result.guides.size() >= 1);
        QCOMPARE(result.guides.first().kind, GuideLine::Kind::Vertical);
        QCOMPARE(result.guides.first().axisPosition, 0.0);
    }

    void noSnapBeyondThreshold()
    {
        // Canvas 100x100. Layer 20x20 centered at (50, 50) -> exactly the
        // canvas center. That would snap, so move it far away: center
        // (10, 10). edges: L=0, C=10, R=20. canvas L=0: dist 0 -> snap!
        // That's not what we want. Use center (12, 12): edges L=2, C=12,
        // R=22. canvas L=0 dist 2 -> snap. Still snaps.
        // Use center (15, 15): edges L=5, C=15, R=25. canvas L=0 dist 5
        // -> snap. Use center (20, 20): edges L=10, C=20, R=30. canvas
        // L=0 dist 10 -> no snap. canvas C=50: all dist 30+ -> no snap.
        Document doc(100, 100);
        auto layer = makeImage("A", 20, 20);
        layer->transform.position = QPointF(20, 20);
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        const QRectF docRect = docRectOf(*doc.findLayer(id));
        const QRectF canvas(0, 0, 100, 100);

        SnapEngine engine;
        const auto result = engine.computeSnap(*doc.findLayer(id),
                                                docRect, canvas, {});

        QCOMPARE(result.delta.x(), 0.0);
        QCOMPARE(result.delta.y(), 0.0);
        QVERIFY(result.guides.isEmpty());
    }

    void snapToOtherLayerRightEdge()
    {
        // A: 200x200 at position (500, 100) -> doc rect (400, 0, 200, 200)
        //    edges: L=400, C=500, R=600
        // B: 200x200 at position (697, 400) -> doc rect (597, 300, 200, 200)
        //    edges: L=597, C=697, R=797
        // |B.L=597 - A.R=600| = 3 -> snap, delta.x = 600-597 = +3
        // Y: B.C=500 vs A.C=100 (B top) -- no, A.C=100. Distance 400, no snap.
        //     B.C=500 vs canvas C=500 -> snap (delta.y=0)
        Document doc(1000, 1000);
        auto a = makeImage("A", 200, 200);
        a->transform.position = QPointF(500, 100);
        QVERIFY(doc.addLayer(std::move(a)));

        auto b = makeImage("B", 200, 200);
        b->transform.position = QPointF(697, 400);
        const LayerId bId = b->id();
        QVERIFY(doc.addLayer(std::move(b)));

        Layer* bPtr = doc.findLayer(bId);
        const QRectF bRect = docRectOf(*bPtr);
        const QRectF canvas(0, 0, 1000, 1000);
        const auto others = allLayersExcept(doc, bId);

        SnapEngine engine;
        const auto result = engine.computeSnap(*bPtr, bRect, canvas, others);

        QCOMPARE(result.delta.x(), 3.0);
        QCOMPARE(result.delta.y(), 0.0);
        QVERIFY(!result.guides.isEmpty());
    }

    void hiddenLayerIsNotASnapTarget()
    {
        Document doc(1000, 1000);
        auto a = makeImage("A", 200, 200);
        a->transform.position = QPointF(500, 100);
        a->visible = false;
        QVERIFY(doc.addLayer(std::move(a)));

        auto b = makeImage("B", 100, 100);
        // B's L edge would be 3px from A's R edge, but A is hidden.
        // B center: (597, 100), doc rect: (547, 50, 100, 100), L=547
        // A R = 700. |547 - 700| = 153, way out. Place closer:
        // We want |B.L - A.R| <= 6. B.L = (B.cx - 50). A.R = 700.
        // B.cx = 694, B.position = (694, 100). doc rect = (644, 50, 100, 100).
        // |644 - 700| = 56, still out. To get within 6: B.cx = 646.
        b->transform.position = QPointF(646, 100);
        const LayerId bId = b->id();
        QVERIFY(doc.addLayer(std::move(b)));

        const QRectF bRect = docRectOf(*doc.findLayer(bId));
        const QRectF canvas(0, 0, 1000, 1000);
        const auto others = allLayersExcept(doc, bId);

        SnapEngine engine;
        const auto result = engine.computeSnap(*doc.findLayer(bId),
                                                bRect, canvas, others);

        // A is hidden: no snap to it. (Canvas has no candidates either.)
        QVERIFY(result.guides.isEmpty());
    }

    void lockedLayerIsNotASnapTarget()
    {
        Document doc(1000, 1000);
        auto a = makeImage("A", 200, 200);
        a->transform.position = QPointF(500, 100);
        a->locked = true;
        QVERIFY(doc.addLayer(std::move(a)));

        auto b = makeImage("B", 100, 100);
        b->transform.position = QPointF(646, 100);  // would snap to A.R=700
        const LayerId bId = b->id();
        QVERIFY(doc.addLayer(std::move(b)));

        const QRectF bRect = docRectOf(*doc.findLayer(bId));
        const QRectF canvas(0, 0, 1000, 1000);
        const auto others = allLayersExcept(doc, bId);

        SnapEngine engine;
        const auto result = engine.computeSnap(*doc.findLayer(bId),
                                                bRect, canvas, others);

        QVERIFY(result.guides.isEmpty());
    }

    void maxTwoGuidesSimultaneously()
    {
        // Two candidates on each axis; pickBest should keep at most 1H+1V.
        // Need at least one candidate per axis. Use 50px threshold to
        // ensure many candidates fire, but pickBest will reduce to 2.
        Document doc(1000, 1000);
        auto a = makeImage("A", 200, 200);
        a->transform.position = QPointF(0, 0);
        QVERIFY(doc.addLayer(std::move(a)));
        auto b = makeImage("B", 200, 200);
        b->transform.position = QPointF(195, 195);
        const LayerId bId = b->id();
        QVERIFY(doc.addLayer(std::move(b)));
        auto c = makeImage("C", 200, 200);
        c->transform.position = QPointF(400, 400);
        QVERIFY(doc.addLayer(std::move(c)));

        Layer* bPtr = doc.findLayer(bId);
        const QRectF bRect = docRectOf(*bPtr);
        const QRectF canvas(0, 0, 1000, 1000);
        const auto others = allLayersExcept(doc, bId);

        SnapEngine engine;
        engine.setThreshold(50.0);
        const auto result = engine.computeSnap(*bPtr, bRect, canvas, others);

        QVERIFY(result.guides.size() <= 2);
        QVERIFY(!result.guides.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSnap)
#include "test_snap.moc"
