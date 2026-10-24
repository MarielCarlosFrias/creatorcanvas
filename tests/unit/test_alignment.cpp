#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/geometry/LayerAlignment.h"
#include "core/layers/Layer.h"

#include <QtTest>

using namespace cc;

class TestAlignment final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<ShapeLayer> createRectLayer(const QString& name, double x, double y, double w, double h)
    {
        auto layer = std::make_unique<ShapeLayer>();
        layer->name = name;
        layer->kind = ShapeKind::Rectangle;
        layer->points = QPolygonF({QPointF(0, 0), QPointF(w, 0), QPointF(w, h), QPointF(0, h)});
        layer->transform.position = QPointF(x, y);
        return layer;
    }

private slots:
    void singleLayerAlignCanvas()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto l1 = createRectLayer("Box", 300, 200, 100, 100);
        const LayerId id = l1->id();
        doc.addLayer(std::move(l1));

        // ContentBounds is (-50..50, -50..50) centered around (0,0) or (0..100, 0..100)
        // Let's verify initial docBounds
        Layer* layer = doc.findLayer(id);
        QVERIFY(layer != nullptr);
        QRectF initialBounds = layer->transform.matrix(layer->contentBounds()).mapRect(layer->contentBounds());
        QCOMPARE(initialBounds.width(), 100.0);
        QCOMPARE(initialBounds.height(), 100.0);

        // Align Left
        QVERIFY(LayerAlignment::alignLayer(doc, &stack, id, AlignTarget::Left));
        QRectF bLeft = layer->transform.matrix(layer->contentBounds()).mapRect(layer->contentBounds());
        QCOMPARE(bLeft.left(), 0.0);

        // Align Right
        QVERIFY(LayerAlignment::alignLayer(doc, &stack, id, AlignTarget::Right));
        QRectF bRight = layer->transform.matrix(layer->contentBounds()).mapRect(layer->contentBounds());
        QCOMPARE(bRight.right(), 800.0);

        // Align Top
        QVERIFY(LayerAlignment::alignLayer(doc, &stack, id, AlignTarget::Top));
        QRectF bTop = layer->transform.matrix(layer->contentBounds()).mapRect(layer->contentBounds());
        QCOMPARE(bTop.top(), 0.0);

        // Align Bottom
        QVERIFY(LayerAlignment::alignLayer(doc, &stack, id, AlignTarget::Bottom));
        QRectF bBottom = layer->transform.matrix(layer->contentBounds()).mapRect(layer->contentBounds());
        QCOMPARE(bBottom.bottom(), 600.0);

        // Align CenterBoth
        QVERIFY(LayerAlignment::alignLayer(doc, &stack, id, AlignTarget::CenterBoth));
        QRectF bCenter = layer->transform.matrix(layer->contentBounds()).mapRect(layer->contentBounds());
        QCOMPARE(bCenter.center().x(), 400.0);
        QCOMPARE(bCenter.center().y(), 300.0);

        // Verify Undo stack restores initial position (consecutive transforms merge on same layer)
        stack.undo();
        QRectF afterUndo = layer->transform.matrix(layer->contentBounds()).mapRect(layer->contentBounds());
        QCOMPARE(afterUndo.bottom(), 250.0);
        QCOMPARE(afterUndo.center(), QPointF(300.0, 200.0));
    }

    void multipleLayersAlignUnion()
    {
        Document doc(1000, 1000);
        CommandStack stack;

        auto l1 = createRectLayer("L1", 100, 100, 100, 100);
        auto l2 = createRectLayer("L2", 500, 300, 100, 100);
        const LayerId id1 = l1->id();
        const LayerId id2 = l2->id();
        doc.addLayer(std::move(l1));
        doc.addLayer(std::move(l2));

        Layer* p1 = doc.findLayer(id1);
        Layer* p2 = doc.findLayer(id2);
        QRectF b1 = p1->transform.matrix(p1->contentBounds()).mapRect(p1->contentBounds());
        QRectF b2 = p2->transform.matrix(p2->contentBounds()).mapRect(p2->contentBounds());
        const double expectedLeft = std::min(b1.left(), b2.left());

        // Align Left: both layers align to the leftmost bounding edge
        QVERIFY(LayerAlignment::alignLayers(doc, &stack, {id1, id2}, AlignTarget::Left));

        QRectF nb1 = p1->transform.matrix(p1->contentBounds()).mapRect(p1->contentBounds());
        QRectF nb2 = p2->transform.matrix(p2->contentBounds()).mapRect(p2->contentBounds());
        QCOMPARE(nb1.left(), expectedLeft);
        QCOMPARE(nb2.left(), expectedLeft);

        // Undo restores L2 back to its right position
        stack.undo();
        QRectF rb2 = p2->transform.matrix(p2->contentBounds()).mapRect(p2->contentBounds());
        QCOMPARE(rb2.left(), b2.left());
    }

    void distributeEquidistant()
    {
        Document doc(1000, 1000);
        CommandStack stack;

        auto l1 = createRectLayer("L1", 100, 200, 50, 50);
        auto l2 = createRectLayer("L2", 150, 200, 50, 50);
        auto l3 = createRectLayer("L3", 500, 200, 50, 50);
        const LayerId id1 = l1->id();
        const LayerId id2 = l2->id();
        const LayerId id3 = l3->id();
        doc.addLayer(std::move(l1));
        doc.addLayer(std::move(l2));
        doc.addLayer(std::move(l3));

        Layer* p1 = doc.findLayer(id1);
        Layer* p2 = doc.findLayer(id2);
        Layer* p3 = doc.findLayer(id3);

        const double c1 = p1->transform.matrix(p1->contentBounds()).mapRect(p1->contentBounds()).center().x();
        const double c3 = p3->transform.matrix(p3->contentBounds()).mapRect(p3->contentBounds()).center().x();
        const double expectedMiddle = (c1 + c3) / 2.0;

        QVERIFY(LayerAlignment::distributeHorizontally(doc, &stack, {id1, id2, id3}));

        const double nc2 = p2->transform.matrix(p2->contentBounds()).mapRect(p2->contentBounds()).center().x();
        QCOMPARE(nc2, expectedMiddle);

        stack.undo();
        const double restoredC2 = p2->transform.matrix(p2->contentBounds()).mapRect(p2->contentBounds()).center().x();
        QCOMPARE(restoredC2, 150.0);
    }

    void lockedLayerResistsAlignment()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto l1 = createRectLayer("L1", 200, 200, 100, 100);
        const LayerId id = l1->id();
        doc.addLayer(std::move(l1));

        // Lock position on layer
        LayerLocks locks;
        locks.position = true;
        doc.setLayerLocks(id, locks);

        Layer* layer = doc.findLayer(id);
        const QPointF posBefore = layer->transform.position;

        // Alignment should be rejected!
        QVERIFY(!LayerAlignment::alignLayer(doc, &stack, id, AlignTarget::Left));
        QCOMPARE(layer->transform.position, posBefore);

        // Unlock and verify alignment now works
        doc.setLayerLocks(id, LayerLocks());
        QVERIFY(LayerAlignment::alignLayer(doc, &stack, id, AlignTarget::Left));
        QVERIFY(layer->transform.position != posBefore);
    }
};

QTEST_MAIN(TestAlignment)
#include "test_alignment.moc"
