#include "core/Document.h"
#include "core/geometry/LayerTransform.h"
#include "core/layers/Layer.h"
#include "core/snap/SnapEngine.h"
#include "tools/ToolCommit.h"

#include <QList>
#include <QRectF>
#include <QSet>
#include <QSignalSpy>
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

    void toolTransformUsesLayerThenParentOrder()
    {
        Document doc(400, 400);
        auto outer = std::make_unique<GroupLayer>();
        outer->transform.position = QPointF(240, 180);
        outer->transform.rotationDeg = 31.0;
        outer->transform.scaleX = 1.2;
        outer->transform.scaleY = 0.8;
        auto inner = std::make_unique<GroupLayer>();
        inner->transform.position = QPointF(70, 45);
        inner->transform.rotationDeg = -53.0;
        inner->transform.shearX = 0.15;
        auto image = makeImage(QStringLiteral("nested"), 12, 8);
        image->transform.position = QPointF(20, 25);
        image->transform.rotationDeg = 77.0;
        image->transform.scaleX = 1.4;
        const LayerId imageId = image->id();

        inner->children.push_back(std::move(image));
        outer->children.push_back(std::move(inner));
        QVERIFY(doc.addLayer(std::move(outer)));

        const Layer* nested = doc.findLayer(imageId);
        const auto* innerGroup = static_cast<const GroupLayer*>(doc.parentOf(imageId));
        const auto* outerGroup = static_cast<const GroupLayer*>(doc.parentOf(innerGroup->id()));
        const QRectF bounds = nested->contentBounds();
        const QTransform imageMatrix = nested->transform.matrix(bounds);
        const QTransform innerMatrix = innerGroup->transform.matrix(innerGroup->contentBounds());
        const QTransform outerMatrix = outerGroup->transform.matrix(outerGroup->contentBounds());
        const QTransform expected = imageMatrix * innerMatrix * outerMatrix;
        const QTransform actual = layerToDocumentTransform(doc, *nested);

        for (const QPointF& point : {bounds.topLeft(), bounds.center(), bounds.bottomRight()}) {
            QVERIFY((actual.map(point) - expected.map(point)).manhattanLength() < 1e-7);
            QVERIFY((layerToDocumentMatrix(doc, *nested).map(point)
                     - expected.map(point)).manhattanLength() < 1e-7);
        }
        const QPointF oldOrderPoint = (outerMatrix * innerMatrix * imageMatrix).map(bounds.topLeft());
        QVERIFY((expected.map(bounds.topLeft()) - oldOrderPoint).manhattanLength() > 1.0);
    }

    void documentSelectionUsesLayerThenParentOrder()
    {
        Document doc(300, 300);
        auto group = std::make_unique<GroupLayer>();
        group->transform.position = QPointF(160, 140);
        group->transform.rotationDeg = 70.0;
        group->transform.scaleX = 1.3;
        group->transform.scaleY = 0.9;
        auto image = makeImage(QStringLiteral("source"), 5, 4);
        image->transform.position = QPointF(45, 35);
        image->transform.rotationDeg = 25.0;
        image->transform.shearX = 0.2;
        const LayerId imageId = image->id();
        group->children.push_back(std::move(image));
        QVERIFY(doc.addLayer(std::move(group)));

        const Layer* sourceLayer = doc.findLayer(imageId);
        const auto* parent = static_cast<const GroupLayer*>(doc.parentOf(imageId));
        const QRectF bounds = sourceLayer->contentBounds();
        const QTransform correct = sourceLayer->transform.matrix(bounds)
            * parent->transform.matrix(parent->contentBounds());
        const QTransform oldOrder = parent->transform.matrix(parent->contentBounds())
            * sourceLayer->transform.matrix(bounds);

        QImage selection(doc.width(), doc.height(), QImage::Format_Alpha8);
        selection.fill(0);
        QSet<QPoint> selectedPoints;
        for (int y = 0; y < bounds.height(); ++y) {
            for (int x = 0; x < bounds.width(); ++x) {
                const QPointF docPoint = correct.map(QPointF(x + 0.5, y + 0.5));
                const QPoint point(qRound(docPoint.x()), qRound(docPoint.y()));
                if (selection.rect().contains(point)) {
                    selection.scanLine(point.y())[point.x()] = 255;
                    selectedPoints.insert(point);
                }
            }
        }
        bool distinguishesOrder = false;
        for (int y = 0; y < bounds.height(); ++y) {
            for (int x = 0; x < bounds.width(); ++x) {
                const QPointF docPoint = oldOrder.map(QPointF(x + 0.5, y + 0.5));
                const QPoint point(qRound(docPoint.x()), qRound(docPoint.y()));
                distinguishesOrder = distinguishesOrder || !selectedPoints.contains(point);
            }
        }
        QVERIFY(distinguishesOrder);

        doc.setPixelSelection(selection, SelectionSpace::Document);
        const LayerId adjustmentId = doc.createAdjustmentLayer(AdjustmentType::Invert, imageId);
        const auto* adjustment = doc.findAdjustmentLayer(adjustmentId);
        QVERIFY(adjustment != nullptr);
        QCOMPARE(adjustment->vectorMask.size(), bounds.size().toSize());
        for (int y = 0; y < bounds.height(); ++y)
            for (int x = 0; x < bounds.width(); ++x)
                QCOMPARE(int(adjustment->vectorMask.constScanLine(y)[x]), 255);
    }

    void setLayerTransformNotifiesAdjustedAncestorsOnce()
    {
        Document doc(300, 300);
        auto outer = std::make_unique<GroupLayer>();
        auto inner = std::make_unique<GroupLayer>();
        auto image = makeImage(QStringLiteral("child"), 20, 10);
        image->transform.position = QPointF(30, 20);
        const LayerId imageId = image->id();
        const LayerId innerId = inner->id();
        const LayerId outerId = outer->id();
        inner->children.push_back(std::move(image));
        outer->children.push_back(std::move(inner));
        QVERIFY(doc.addLayer(std::move(outer)));

        QSignalSpy spy(&doc, &Document::layerPropertyChanged);
        AffineTransform moved = doc.findLayer(imageId)->transform;
        moved.position += QPointF(12, 7);
        QVERIFY(doc.setLayerTransform(imageId, moved));

        QList<LayerId> notified;
        for (const auto& args : spy)
            notified.push_back(args.at(0).toUuid());
        QCOMPARE(notified.count(imageId), 1);
        QCOMPARE(notified.count(innerId), 1);
        QCOMPARE(notified.count(outerId), 1);
        QCOMPARE(notified.size(), 3);
    }

    void snapTargetComposesTransformedAncestors()
    {
        Document doc(1000, 1000);
        auto group = std::make_unique<GroupLayer>();
        group->transform.position = QPointF(360, 280);
        group->transform.rotationDeg = 90.0;
        group->transform.scaleX = 1.4;
        auto target = makeImage(QStringLiteral("target"), 40, 20);
        target->transform.position = QPointF(40, 30);
        target->transform.rotationDeg = 25.0;
        const LayerId targetId = target->id();
        group->children.push_back(std::move(target));
        QVERIFY(doc.addLayer(std::move(group)));
        auto moving = makeImage(QStringLiteral("moving"), 20, 20);
        const LayerId movingId = moving->id();
        QVERIFY(doc.addLayer(std::move(moving)));

        const Layer* targetLayer = doc.findLayer(targetId);
        const QRectF targetRect = layerToDocumentMatrix(doc, *targetLayer)
            .mapRect(targetLayer->contentBounds());
        const QRectF movingRect(targetRect.right() + 3.0, 700.0, 20.0, 20.0);
        SnapEngine engine;
        const SnapResult result = engine.computeSnap(
            doc, *doc.findLayer(movingId), movingRect,
            QRectF(0, 0, 1000, 1000), {doc.findLayer(targetId)});

        QVERIFY(qAbs(result.delta.x() + 3.0) < 1e-7);
        QCOMPARE(result.guides.size(), 1);
        QCOMPARE(result.guides.first().kind, GuideLine::Kind::Vertical);
        QVERIFY(qAbs(result.guides.first().axisPosition - targetRect.right()) < 1e-7);
    }

    void movingGroupAndDescendantsAreNotSnapTargets()
    {
        Document doc(1000, 1000);
        auto group = std::make_unique<GroupLayer>();
        group->transform.position = QPointF(250, 250);
        group->transform.rotationDeg = 35.0;
        auto child = makeImage(QStringLiteral("child"), 60, 40);
        child->transform.position = QPointF(30, 20);
        const LayerId childId = child->id();
        group->children.push_back(std::move(child));
        const LayerId groupId = group->id();
        QVERIFY(doc.addLayer(std::move(group)));

        Layer* moving = doc.findLayer(groupId);
        Layer* descendant = doc.findLayer(childId);
        const QRectF movingRect = layerToDocumentMatrix(doc, *descendant)
            .mapRect(descendant->contentBounds());
        SnapEngine engine;
        const SnapResult result = engine.computeSnap(
            doc, *moving, movingRect, QRectF(0, 0, 1000, 1000),
            {moving, descendant});

        QCOMPARE(result.delta, QPointF(0, 0));
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
