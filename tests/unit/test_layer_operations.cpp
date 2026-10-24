#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/operations/LayerOperations.h"
#include "core/layers/Layer.h"

#include <QtTest>

using namespace cc;

class TestLayerOperations final : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<ShapeLayer> createRect(const QString& name, double x, double y, double w, double h)
    {
        auto layer = std::make_unique<ShapeLayer>();
        layer->name = name;
        layer->kind = ShapeKind::Rectangle;
        layer->points = QPolygonF({QPointF(0, 0), QPointF(w, 0), QPointF(w, h), QPointF(0, h)});
        layer->transform.position = QPointF(x, y);
        return layer;
    }

private slots:
    void areMergeableSiblings()
    {
        Document doc(800, 600);
        auto l1 = createRect("L1", 10, 10, 50, 50);
        auto l2 = createRect("L2", 20, 20, 50, 50);
        const LayerId id1 = l1->id();
        const LayerId id2 = l2->id();
        doc.addLayer(std::move(l1));
        doc.addLayer(std::move(l2));

        // Single layer is not mergeable
        QVERIFY(!LayerOperations::areMergeableSiblings(doc, QList<LayerId>{id1}));

        // Sibling layers are mergeable
        QVERIFY(LayerOperations::areMergeableSiblings(doc, QList<LayerId>{id1, id2}));

        // Background is not mergeable
        auto bg = std::make_unique<BackgroundLayer>();
        const LayerId bgId = bg->id();
        doc.addLayer(std::move(bg));
        QVERIFY(!LayerOperations::areMergeableSiblings(doc, QList<LayerId>{bgId, id1}));

        // Layers in different groups are not mergeable
        auto group = std::make_unique<GroupLayer>();
        group->name = "Group";
        auto l3 = createRect("L3", 30, 30, 50, 50);
        const LayerId id3 = l3->id();
        group->children.push_back(std::move(l3));
        doc.addLayer(std::move(group));

        QVERIFY(!LayerOperations::areMergeableSiblings(doc, QList<LayerId>{id1, id3}));
    }

    void mergeSelectedAndUndo()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto l1 = createRect("Layer1", 100, 100, 100, 100);
        auto l2 = createRect("Layer2", 150, 150, 100, 100);
        const LayerId id1 = l1->id();
        const LayerId id2 = l2->id();
        doc.addLayer(std::move(l1));
        doc.addLayer(std::move(l2));

        const LayerId mergedId = LayerOperations::mergeSelected(doc, &stack, QList<LayerId>{id1, id2}, "Merged");
        QVERIFY(!mergedId.isNull());
        QVERIFY(doc.findLayer(mergedId) != nullptr);
        QCOMPARE(doc.findLayer(id1), nullptr);
        QCOMPARE(doc.findLayer(id2), nullptr);

        // Undo restores both layers
        stack.undo();
        QVERIFY(doc.findLayer(id1) != nullptr);
        QVERIFY(doc.findLayer(id2) != nullptr);
        QCOMPARE(doc.findLayer(mergedId), nullptr);

        // Redo re-merges them
        stack.redo();
        QVERIFY(doc.findLayer(mergedId) != nullptr);
        QCOMPARE(doc.findLayer(id1), nullptr);
        QCOMPARE(doc.findLayer(id2), nullptr);
    }

    void mergeDownSingleLayer()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto l1 = createRect("Bottom", 50, 50, 50, 50);
        auto l2 = createRect("Top", 60, 60, 50, 50);
        const LayerId id1 = l1->id();
        const LayerId id2 = l2->id();
        doc.addLayer(std::move(l1));
        doc.addLayer(std::move(l2));

        // Selecting only top layer triggers merge down
        const LayerId mergedId = LayerOperations::mergeSelected(doc, &stack, QList<LayerId>{id2}, "MergedDown");
        QVERIFY(!mergedId.isNull());
        QVERIFY(doc.findLayer(mergedId) != nullptr);
        QCOMPARE(doc.findLayer(id1), nullptr);
        QCOMPARE(doc.findLayer(id2), nullptr);
    }

    void mergeVisibleRespectsLocks()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto l1 = createRect("Visible1", 10, 10, 50, 50);
        auto l2 = createRect("Visible2", 20, 20, 50, 50);
        auto lLocked = createRect("Locked", 30, 30, 50, 50);
        lLocked->locked = true;

        const LayerId id1 = l1->id();
        const LayerId id2 = l2->id();
        const LayerId idLocked = lLocked->id();

        doc.addLayer(std::move(l1));
        doc.addLayer(std::move(l2));
        doc.addLayer(std::move(lLocked));

        const LayerId mergedId = LayerOperations::mergeVisible(doc, &stack, "VisibleMerged");
        QVERIFY(!mergedId.isNull());
        QVERIFY(doc.findLayer(mergedId) != nullptr);
        QCOMPARE(doc.findLayer(id1), nullptr);
        QCOMPARE(doc.findLayer(id2), nullptr);
        // Locked layer is preserved!
        QVERIFY(doc.findLayer(idLocked) != nullptr);
        QVERIFY(doc.findLayer(idLocked)->locked);
    }

    void flattenRespectsLocks()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto l1 = createRect("Normal1", 10, 10, 50, 50);
        auto l2 = createRect("Normal2", 20, 20, 50, 50);
        auto lLocked = createRect("Locked", 30, 30, 50, 50);
        lLocked->locked = true;

        const LayerId id1 = l1->id();
        const LayerId id2 = l2->id();
        const LayerId idLocked = lLocked->id();

        doc.addLayer(std::move(l1));
        doc.addLayer(std::move(l2));
        doc.addLayer(std::move(lLocked));

        const LayerId flatId = LayerOperations::flatten(doc, &stack, "Flattened");
        QVERIFY(!flatId.isNull());
        QVERIFY(doc.findLayer(flatId) != nullptr);
        QCOMPARE(doc.findLayer(id1), nullptr);
        QCOMPARE(doc.findLayer(id2), nullptr);
        QVERIFY(doc.findLayer(idLocked) != nullptr);

        // All locked layers case
        Document docAllLocked(800, 600);
        auto allLocked = createRect("AllLocked", 10, 10, 50, 50);
        allLocked->locked = true;
        docAllLocked.addLayer(std::move(allLocked));
        QVERIFY(LayerOperations::flatten(docAllLocked, &stack).isNull());
    }

    void groupAndUngroup()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto l1 = createRect("Item1", 10, 10, 50, 50);
        auto l2 = createRect("Item2", 20, 20, 50, 50);
        const LayerId id1 = l1->id();
        const LayerId id2 = l2->id();
        doc.addLayer(std::move(l1));
        doc.addLayer(std::move(l2));

        const LayerId groupId = LayerOperations::groupSelected(doc, &stack, QList<LayerId>{id1, id2}, "MyGroup");
        QVERIFY(!groupId.isNull());
        Layer* grp = doc.findLayer(groupId);
        QVERIFY(grp != nullptr);
        QCOMPARE(grp->type(), LayerType::Group);
        auto* groupLayer = static_cast<GroupLayer*>(grp);
        QCOMPARE(groupLayer->children.size(), static_cast<size_t>(2));

        // Ungroup
        const LayerId nextSelected = LayerOperations::ungroup(doc, &stack, groupId);
        QVERIFY(!nextSelected.isNull());
        QCOMPARE(doc.findLayer(groupId), nullptr);
        QVERIFY(doc.findLayer(id1) != nullptr);
        QVERIFY(doc.findLayer(id2) != nullptr);

        // Undo restores the group
        stack.undo();
        QVERIFY(doc.findLayer(groupId) != nullptr);
    }

    void flipAndRotate()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto l1 = createRect("Transformable", 100, 100, 100, 100);
        const LayerId id = l1->id();
        doc.addLayer(std::move(l1));

        Layer* layer = doc.findLayer(id);
        const double initialScaleX = layer->transform.scaleX;
        const double initialScaleY = layer->transform.scaleY;
        const double initialRot = layer->transform.rotationDeg;

        // Flip horizontal
        QVERIFY(LayerOperations::flip(doc, &stack, id, true));
        QCOMPARE(layer->transform.scaleX, -initialScaleX);

        // Flip vertical
        QVERIFY(LayerOperations::flip(doc, &stack, id, false));
        QCOMPARE(layer->transform.scaleY, -initialScaleY);

        // Rotate
        QVERIFY(LayerOperations::rotate(doc, &stack, id, 45.0));
        QCOMPARE(layer->transform.rotationDeg, initialRot + 45.0);

        // Undo rotate
        stack.undo();
        QCOMPARE(layer->transform.rotationDeg, initialRot);

        // Position locked layer cannot be flipped or rotated
        layer->locks.position = true;
        QVERIFY(!LayerOperations::flip(doc, &stack, id, true));
        QVERIFY(!LayerOperations::rotate(doc, &stack, id, 45.0));
    }

    void deleteLayer()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto l1 = createRect("Deletable", 10, 10, 50, 50);
        const LayerId id = l1->id();
        doc.addLayer(std::move(l1));

        // Locked layer cannot be deleted
        doc.setLayerLocked(id, true);
        QVERIFY(!LayerOperations::deleteLayer(doc, &stack, id));
        QVERIFY(doc.findLayer(id) != nullptr);

        // Unlocked layer can be deleted
        doc.setLayerLocked(id, false);
        QVERIFY(LayerOperations::deleteLayer(doc, &stack, id));
        QCOMPARE(doc.findLayer(id), nullptr);

        // Undo restores it
        stack.undo();
        QVERIFY(doc.findLayer(id) != nullptr);
    }
};

QTEST_MAIN(TestLayerOperations)
#include "test_layer_operations.moc"
