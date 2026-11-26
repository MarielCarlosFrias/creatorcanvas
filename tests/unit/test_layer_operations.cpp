#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/commands/RasterCommands.h"
#include "core/commands/LayerStructureCommands.h"
#include "core/serialization/ProjectFile.h"
#include "services/LayerOperations.h"
#include "core/layers/Layer.h"
#include "rendering/CanvasRenderer.h"

#include <QDebug>
#include <QPainter>
#include <QTemporaryDir>
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

    // ---------- helpers de render ----------
    static LayerId addSolidImage(Document& doc, const QColor& c, const QPointF& center,
                                 int w = 100, int h = 80)
    {
        QImage img(w, h, QImage::Format_ARGB32);
        img.fill(c);
        auto layer = std::make_unique<ImageLayer>();
        layer->assetId = doc.assets().addImage(img);
        layer->naturalWidth = w;
        layer->naturalHeight = h;
        layer->transform.position = center;
        const LayerId id = layer->id();
        doc.addLayer(std::move(layer));
        return id;
    }

    static QPoint redRectCenter(const Document& doc, const QColor& probe)
    {
        QImage out(doc.width(), doc.height(), QImage::Format_ARGB32_Premultiplied);
        out.fill(Qt::transparent);
        QPainter p(&out);
        RenderOptions o;
        o.drawCheckerboard = false;
        o.canvasBorder = Qt::transparent;
        renderDocument(doc, &p, QTransform(), o);
        p.end();
        int minX = 1 << 30, minY = 1 << 30, maxX = -1, maxY = -1;
        for (int y = 0; y < out.height(); ++y)
            for (int x = 0; x < out.width(); ++x) {
                const QColor c = out.pixelColor(x, y);
                if (c.alpha() > 200 && qAbs(c.red() - probe.red()) < 40
                    && qAbs(c.green() - probe.green()) < 40 && qAbs(c.blue() - probe.blue()) < 40) {
                    minX = qMin(minX, x); minY = qMin(minY, y);
                    maxX = qMax(maxX, x); maxY = qMax(maxY, y);
                }
            }
        if (maxX < 0)
            return {};
        return {(minX + maxX) / 2, (minY + maxY) / 2};
    }

    static QTransform layerDocumentMatrix(const Document& doc, const LayerId& id)
    {
        const Layer* layer = doc.findLayer(id);
        if (!layer)
            return {};
        QTransform matrix = layer->transform.matrix(layer->contentBounds());
        for (GroupLayer* parent = doc.parentOf(id);
             parent && parent != doc.rootGroup();
             parent = doc.parentOf(parent->id())) {
            matrix = matrix * parent->transform.matrix(parent->contentBounds());
        }
        return matrix;
    }

    static void verifyMatrixClose(const QTransform& actual, const QTransform& expected)
    {
        constexpr double eps = 1e-6;
        QString actualText;
        QString expectedText;
        QDebug(&actualText) << actual;
        QDebug(&expectedText) << expected;
        QVERIFY2(qAbs(actual.m11() - expected.m11()) < eps
                     && qAbs(actual.m12() - expected.m12()) < eps
                     && qAbs(actual.m21() - expected.m21()) < eps
                     && qAbs(actual.m22() - expected.m22()) < eps
                     && qAbs(actual.dx() - expected.dx()) < eps
                     && qAbs(actual.dy() - expected.dy()) < eps,
                 qPrintable(QStringLiteral("matriz atual=%1 esperada=%2")
                                .arg(actualText, expectedText)));
    }

    static void verifyLayerMatrixClose(const Document& doc, const LayerId& id,
                                       const QTransform& expected)
    {
        QVERIFY(doc.findLayer(id));
        verifyMatrixClose(layerDocumentMatrix(doc, id), expected);
    }

    static LayerId groupWithoutHistory(Document& doc, const QList<LayerId>& ids,
                                       const QString& name)
    {
        GroupLayersCommand command(doc, ids, name);
        const LayerId id = command.groupId();
        command.redo();
        return id;
    }

    static void verifyRenderCenterClose(const Document& doc, const QColor& color,
                                        const QPoint& expected)
    {
        const QPoint actual = redRectCenter(doc, color);
        QVERIFY2(qAbs(actual.x() - expected.x()) <= 2
                     && qAbs(actual.y() - expected.y()) <= 2,
                 qPrintable(QStringLiteral("centro atual=(%1,%2) esperado=(%3,%4)")
                                .arg(actual.x()).arg(actual.y())
                                .arg(expected.x()).arg(expected.y())));
    }

    void groupKeepsVisualPosition()
    {
        Document doc(400, 300);
        CommandStack stack;
        const QColor red(200, 30, 30);
        const LayerId id = addSolidImage(doc, red, QPointF(200, 150));

        const QPoint before = redRectCenter(doc, red);
        QVERIFY(before.x() > 0);

        stack.execute(std::make_unique<GroupLayersCommand>(
            doc, QList<LayerId>{id}, QStringLiteral("G")));

        const QPoint after = redRectCenter(doc, red);
        // A imagem deve continuar no mesmo sítio (antes saltava para o canto).
        QVERIFY2(qAbs(after.x() - before.x()) <= 2 && qAbs(after.y() - before.y()) <= 2,
                 qPrintable(QStringLiteral("antes=(%1,%2) depois=(%3,%4)")
                                .arg(before.x()).arg(before.y()).arg(after.x()).arg(after.y())));

        // undo devolve a camada ao root e a posição mantém-se
        stack.undo();
        const QPoint afterUndo = redRectCenter(doc, red);
        QVERIFY(qAbs(afterUndo.x() - before.x()) <= 2 && qAbs(afterUndo.y() - before.y()) <= 2);
    }

    void reorderIntoGroupKeepsPositions()
    {
        Document doc(400, 300);
        CommandStack stack;
        const QColor red(200, 30, 30);
        const QColor green(30, 200, 30);
        const LayerId inGroup = addSolidImage(doc, red, QPointF(200, 150));
        stack.execute(std::make_unique<GroupLayersCommand>(
            doc, QList<LayerId>{inGroup}, QStringLiteral("G")));
        const LayerId groupId = doc.rootGroup()->children.back()->id();
        const LayerId other = addSolidImage(doc, green, QPointF(100, 80));

        const QPoint redBefore = redRectCenter(doc, red);
        const QPoint greenBefore = redRectCenter(doc, green);

        // arrasta a camada verde para DENTRO do grupo
        auto* group = static_cast<GroupLayer*>(doc.findLayer(groupId));
        stack.execute(std::make_unique<ReorderLayerCommand>(doc, other, group, 0));

        const QPoint redAfter = redRectCenter(doc, red);
        const QPoint greenAfter = redRectCenter(doc, green);
        QVERIFY2(qAbs(redAfter.x() - redBefore.x()) <= 2 && qAbs(redAfter.y() - redBefore.y()) <= 2,
                 "conteúdo existente do grupo não pode saltar");
        QVERIFY2(qAbs(greenAfter.x() - greenBefore.x()) <= 2 && qAbs(greenAfter.y() - greenBefore.y()) <= 2,
                 "camada arrastada para o grupo não pode saltar");

        // undo restaura
        stack.undo();
        const QPoint greenUndo = redRectCenter(doc, green);
        QVERIFY(qAbs(greenUndo.x() - greenBefore.x()) <= 2 && qAbs(greenUndo.y() - greenBefore.y()) <= 2);
    }

    void ungroupAfterMovingGroupKeepsPositions()
    {
        Document doc(400, 300);
        CommandStack stack;
        const QColor red(200, 30, 30);
        const LayerId id = addSolidImage(doc, red, QPointF(200, 150));
        stack.execute(std::make_unique<GroupLayersCommand>(
            doc, QList<LayerId>{id}, QStringLiteral("G")));
        const LayerId groupId = doc.rootGroup()->children.back()->id();

        // move o grupo +50,+30
        auto* group = static_cast<GroupLayer*>(doc.findLayer(groupId));
        AffineTransform gt = group->transform;
        gt.position += QPointF(50, 30);
        doc.setLayerTransform(groupId, gt);

        const QPoint before = redRectCenter(doc, red);
        QVERIFY(qAbs(before.x() - 250) <= 2 && qAbs(before.y() - 180) <= 2);

        stack.execute(std::make_unique<UngroupLayersCommand>(doc, groupId));
        const QPoint after = redRectCenter(doc, red);
        // ao desagrupar, o deslocamento do grupo é "cozido" na imagem
        QVERIFY2(qAbs(after.x() - 250) <= 2 && qAbs(after.y() - 180) <= 2,
                 qPrintable(QStringLiteral("depois=(%1,%2)").arg(after.x()).arg(after.y())));

        stack.undo();
        const QPoint afterUndo = redRectCenter(doc, red);
        QVERIFY(qAbs(afterUndo.x() - 250) <= 2 && qAbs(afterUndo.y() - 180) <= 2);
    }

    void rebaseTransformKeepsFullAffine()
    {
        auto layer = createRect(QStringLiteral("Affine"), 120, 90, 40, 24);
        layer->transform.rotationDeg = 27.0;
        layer->transform.scaleX = -1.4;
        layer->transform.scaleY = 0.75;
        layer->transform.shearX = 0.18;
        layer->transform.shearY = -0.09;

        QTransform parentChange;
        parentChange.translate(37, -21);
        parentChange.rotate(-38);
        parentChange.shear(0.16, -0.11);
        parentChange.scale(-1.2, 0.7);
        const QTransform expected = layer->transform.matrix(layer->contentBounds())
            * parentChange;
        const AffineTransform rebased = detail::rebaseTransform(parentChange, *layer);
        verifyMatrixClose(rebased.matrix(layer->contentBounds()), expected);

        // Translação pura não reparametriza flip nem nenhum dos shears.
        const QTransform translation = QTransform::fromTranslate(11, -8);
        const AffineTransform translated = detail::rebaseTransform(translation, *layer);
        QCOMPARE(translated.rotationDeg, layer->transform.rotationDeg);
        QCOMPARE(translated.scaleX, layer->transform.scaleX);
        QCOMPARE(translated.scaleY, layer->transform.scaleY);
        QCOMPARE(translated.shearX, layer->transform.shearX);
        QCOMPARE(translated.shearY, layer->transform.shearY);
        QCOMPARE(translated.position,
                 (layer->transform.matrix(layer->contentBounds()) * translation)
                     .map(layer->contentBounds().center()));
    }

    void reorderRootGroupRoundTripPreservesAffine()
    {
        Document doc(500, 400);
        CommandStack stack;
        const QColor red(200, 30, 30);
        const QColor green(30, 200, 30);
        const LayerId resident = addSolidImage(doc, red, QPointF(245, 195), 30, 20);
        const LayerId groupId = groupWithoutHistory(doc, {resident}, QStringLiteral("G"));
        QVERIFY(doc.findLayer(groupId));

        auto* group = static_cast<GroupLayer*>(doc.findLayer(groupId));
        AffineTransform groupTransform = group->transform;
        groupTransform.position += QPointF(25, -15);
        groupTransform.rotationDeg = 22.0;
        groupTransform.scaleX = -1.15;
        groupTransform.scaleY = 0.85;
        groupTransform.shearX = 0.12;
        groupTransform.shearY = -0.06;
        QVERIFY(doc.setLayerTransform(groupId, groupTransform));

        const LayerId moved = addSolidImage(doc, green, QPointF(145, 125), 24, 18);
        AffineTransform movedTransform = doc.findLayer(moved)->transform;
        movedTransform.rotationDeg = -31.0;
        movedTransform.scaleX = -0.9;
        movedTransform.scaleY = 1.25;
        movedTransform.shearX = -0.14;
        movedTransform.shearY = 0.08;
        QVERIFY(doc.setLayerTransform(moved, movedTransform));

        doc.rootGroup()->transform.position = QPointF(71, 53);
        doc.rootGroup()->transform.rotationDeg = 13.0;
        doc.rootGroup()->transform.scaleX = -2.0;
        const AffineTransform rootTransform = doc.rootGroup()->transform;
        const QTransform movedBefore = layerDocumentMatrix(doc, moved);
        const QTransform residentBefore = layerDocumentMatrix(doc, resident);
        const QTransform groupBefore = layerDocumentMatrix(doc, groupId);

        stack.execute(std::make_unique<ReorderLayerCommand>(doc, moved, group, 0));
        QCOMPARE(doc.parentOf(moved), group);
        verifyLayerMatrixClose(doc, moved, movedBefore);
        verifyLayerMatrixClose(doc, resident, residentBefore);
        verifyLayerMatrixClose(doc, groupId, groupBefore);
        QCOMPARE(doc.rootGroup()->transform, rootTransform);

        stack.undo();
        QCOMPARE(doc.parentOf(moved), doc.rootGroup());
        QCOMPARE(doc.findLayer(moved)->transform, movedTransform);
        verifyLayerMatrixClose(doc, moved, movedBefore);
        verifyLayerMatrixClose(doc, resident, residentBefore);
        verifyLayerMatrixClose(doc, groupId, groupBefore);
        QCOMPARE(doc.rootGroup()->transform, rootTransform);

        stack.redo();
        QCOMPARE(doc.parentOf(moved), group);
        verifyLayerMatrixClose(doc, moved, movedBefore);
        verifyLayerMatrixClose(doc, resident, residentBefore);
        QCOMPARE(doc.rootGroup()->transform, rootTransform);

        // Usar explicitamente rootGroup() como destino também não pode tratar
        // a raiz como um grupo transformável.
        const QTransform movedBeforeOut = layerDocumentMatrix(doc, moved);
        const QTransform residentBeforeOut = layerDocumentMatrix(doc, resident);
        const QTransform groupBeforeOut = layerDocumentMatrix(doc, groupId);
        stack.execute(std::make_unique<ReorderLayerCommand>(
            doc, moved, doc.rootGroup(), 0));
        QCOMPARE(doc.parentOf(moved), doc.rootGroup());
        verifyLayerMatrixClose(doc, moved, movedBeforeOut);
        verifyLayerMatrixClose(doc, resident, residentBeforeOut);
        verifyLayerMatrixClose(doc, groupId, groupBeforeOut);
        QCOMPARE(doc.rootGroup()->transform, rootTransform);

        stack.undo();
        QCOMPARE(doc.parentOf(moved), group);
        verifyLayerMatrixClose(doc, moved, movedBeforeOut);
        verifyLayerMatrixClose(doc, resident, residentBeforeOut);
        stack.redo();
        QCOMPARE(doc.parentOf(moved), doc.rootGroup());
        verifyLayerMatrixClose(doc, moved, movedBeforeOut);
        verifyLayerMatrixClose(doc, resident, residentBeforeOut);
        QCOMPARE(doc.rootGroup()->transform, rootTransform);
    }

    void reorderBetweenGroupsPreservesAffine_data()
    {
        QTest::addColumn<bool>("useHistory");
        QTest::newRow("without history") << false;
        QTest::newRow("with undo redo") << true;
    }

    void reorderBetweenGroupsPreservesAffine()
    {
        QFETCH(bool, useHistory);
        Document doc(500, 400);
        const QColor red(200, 30, 30);
        const QColor green(30, 200, 30);
        const LayerId moved = addSolidImage(doc, green, QPointF(160, 150), 22, 16);
        AffineTransform movedTransform = doc.findLayer(moved)->transform;
        movedTransform.rotationDeg = 18.0;
        movedTransform.scaleX = -1.1;
        movedTransform.scaleY = 0.8;
        movedTransform.shearX = 0.13;
        movedTransform.shearY = -0.07;
        QVERIFY(doc.setLayerTransform(moved, movedTransform));
        const LayerId sourceId = groupWithoutHistory(doc, {moved}, QStringLiteral("G1"));

        const LayerId resident = addSolidImage(doc, red, QPointF(275, 190), 26, 18);
        const LayerId destinationId = groupWithoutHistory(doc, {resident}, QStringLiteral("G2"));
        QVERIFY(doc.findLayer(sourceId));
        QVERIFY(doc.findLayer(destinationId));

        AffineTransform sourceTransform = doc.findLayer(sourceId)->transform;
        sourceTransform.position += QPointF(-12, 9);
        sourceTransform.rotationDeg = -24.0;
        sourceTransform.scaleX = 1.2;
        sourceTransform.scaleY = -0.9;
        sourceTransform.shearX = -0.1;
        sourceTransform.shearY = 0.05;
        QVERIFY(doc.setLayerTransform(sourceId, sourceTransform));
        AffineTransform destinationTransform = doc.findLayer(destinationId)->transform;
        destinationTransform.position += QPointF(15, 12);
        destinationTransform.rotationDeg = 32.0;
        destinationTransform.scaleX = -0.85;
        destinationTransform.scaleY = 1.15;
        destinationTransform.shearX = 0.08;
        destinationTransform.shearY = 0.11;
        QVERIFY(doc.setLayerTransform(destinationId, destinationTransform));

        doc.rootGroup()->transform.position = QPointF(19, 27);
        const AffineTransform rootTransform = doc.rootGroup()->transform;
        const QTransform movedBefore = layerDocumentMatrix(doc, moved);
        const QTransform residentBefore = layerDocumentMatrix(doc, resident);
        const QTransform sourceBefore = layerDocumentMatrix(doc, sourceId);
        const QTransform destinationBefore = layerDocumentMatrix(doc, destinationId);
        const QPoint movedRenderBefore = redRectCenter(doc, green);
        const QPoint residentRenderBefore = redRectCenter(doc, red);
        auto* destination = static_cast<GroupLayer*>(doc.findLayer(destinationId));

        CommandStack stack;
        auto command = std::make_unique<ReorderLayerCommand>(doc, moved, destination, 0);
        ReorderLayerCommand* rawCommand = command.get();
        if (useHistory)
            stack.execute(std::move(command));
        else
            command->redo();

        QCOMPARE(doc.parentOf(moved), destination);
        verifyLayerMatrixClose(doc, moved, movedBefore);
        verifyLayerMatrixClose(doc, resident, residentBefore);
        verifyLayerMatrixClose(doc, sourceId, sourceBefore);
        verifyLayerMatrixClose(doc, destinationId, destinationBefore);
        verifyRenderCenterClose(doc, green, movedRenderBefore);
        verifyRenderCenterClose(doc, red, residentRenderBefore);
        QCOMPARE(doc.rootGroup()->transform, rootTransform);

        if (useHistory)
            stack.undo();
        else
            rawCommand->undo();
        QCOMPARE(doc.parentOf(moved)->id(), sourceId);
        QCOMPARE(doc.findLayer(moved)->transform, movedTransform);
        verifyLayerMatrixClose(doc, moved, movedBefore);
        verifyLayerMatrixClose(doc, resident, residentBefore);
        verifyLayerMatrixClose(doc, sourceId, sourceBefore);
        verifyLayerMatrixClose(doc, destinationId, destinationBefore);

        if (useHistory)
            stack.redo();
        else
            rawCommand->redo();
        QCOMPARE(doc.parentOf(moved), destination);
        verifyLayerMatrixClose(doc, moved, movedBefore);
        verifyLayerMatrixClose(doc, resident, residentBefore);
        verifyRenderCenterClose(doc, green, movedRenderBefore);
        verifyRenderCenterClose(doc, red, residentRenderBefore);
        QCOMPARE(doc.rootGroup()->transform, rootTransform);
    }

    void multiParentGroupPreservesGeometry_data()
    {
        QTest::addColumn<bool>("useHistory");
        QTest::newRow("without history") << false;
        QTest::newRow("with undo redo") << true;
    }

    void multiParentGroupPreservesGeometry()
    {
        QFETCH(bool, useHistory);
        Document doc(500, 400);
        const QColor red(200, 30, 30);
        const QColor green(30, 200, 30);
        const QColor blue(30, 30, 200);
        const QColor yellow(200, 180, 30);
        const QColor magenta(180, 30, 180);

        const LayerId inG1 = addSolidImage(doc, red, QPointF(150, 130), 24, 18);
        const LayerId keepG1 = addSolidImage(doc, green, QPointF(190, 150), 20, 20);
        AffineTransform t = doc.findLayer(inG1)->transform;
        t.rotationDeg = 21.0;
        t.scaleX = -1.2;
        t.shearX = 0.1;
        t.shearY = -0.04;
        QVERIFY(doc.setLayerTransform(inG1, t));
        const LayerId g1 = groupWithoutHistory(doc, {inG1, keepG1}, QStringLiteral("G1"));

        const LayerId inG2 = addSolidImage(doc, blue, QPointF(300, 220), 22, 24);
        const LayerId keepG2 = addSolidImage(doc, yellow, QPointF(330, 190), 18, 26);
        t = doc.findLayer(inG2)->transform;
        t.rotationDeg = -17.0;
        t.scaleY = -0.85;
        t.shearX = -0.08;
        QVERIFY(doc.setLayerTransform(inG2, t));
        const LayerId g2 = groupWithoutHistory(doc, {inG2, keepG2}, QStringLiteral("G2"));

        t = doc.findLayer(g1)->transform;
        t.position += QPointF(12, -8);
        t.rotationDeg = -20.0;
        t.scaleX = -1.1;
        t.shearY = 0.06;
        QVERIFY(doc.setLayerTransform(g1, t));
        t = doc.findLayer(g2)->transform;
        t.position += QPointF(-9, 14);
        t.rotationDeg = 28.0;
        t.scaleY = -1.05;
        t.shearX = 0.09;
        QVERIFY(doc.setLayerTransform(g2, t));

        const LayerId atRoot = addSolidImage(doc, magenta, QPointF(235, 105), 20, 16);
        t = doc.findLayer(atRoot)->transform;
        t.rotationDeg = 33.0;
        t.scaleX = -0.9;
        t.scaleY = 1.2;
        t.shearX = 0.12;
        t.shearY = 0.07;
        QVERIFY(doc.setLayerTransform(atRoot, t));
        QVERIFY(doc.findLayer(g1));
        QVERIFY(doc.findLayer(g2));

        const QList<LayerId> selected{atRoot, inG1, inG2};
        const QList<LayerId> allLayers{atRoot, inG1, keepG1, inG2, keepG2};
        QHash<LayerId, QTransform> before;
        QHash<LayerId, AffineTransform> oldTransforms;
        QHash<LayerId, LayerId> oldParents;
        for (const LayerId& id : allLayers) {
            before.insert(id, layerDocumentMatrix(doc, id));
            oldTransforms.insert(id, doc.findLayer(id)->transform);
            oldParents.insert(id, doc.parentOf(id)->id());
        }
        doc.rootGroup()->transform.position = QPointF(43, 31);
        const AffineTransform rootTransform = doc.rootGroup()->transform;

        CommandStack stack;
        const LayerId groupId = LayerOperations::groupSelected(
            doc, useHistory ? &stack : nullptr, selected, QStringLiteral("Multi"));
        QVERIFY(!groupId.isNull());
        const auto* group = static_cast<const GroupLayer*>(doc.findLayer(groupId));
        QVERIFY(group);
        QCOMPARE(group->children.size(), static_cast<std::size_t>(selected.size()));
        for (const LayerId& id : allLayers)
            verifyLayerMatrixClose(doc, id, before.value(id));
        QCOMPARE(doc.rootGroup()->transform, rootTransform);

        if (useHistory) {
            stack.undo();
            QCOMPARE(doc.findLayer(groupId), nullptr);
            for (const LayerId& id : allLayers) {
                QVERIFY(doc.parentOf(id));
                QCOMPARE(doc.parentOf(id)->id(), oldParents.value(id));
                QCOMPARE(doc.findLayer(id)->transform, oldTransforms.value(id));
                verifyLayerMatrixClose(doc, id, before.value(id));
            }
            QCOMPARE(doc.rootGroup()->transform, rootTransform);

            stack.redo();
            QVERIFY(doc.findLayer(groupId));
            for (const LayerId& id : allLayers)
                verifyLayerMatrixClose(doc, id, before.value(id));
            QCOMPARE(doc.rootGroup()->transform, rootTransform);
        }
    }

    void ungroupIntoTransformedParentPreservesGeometry_data()
    {
        QTest::addColumn<bool>("useHistory");
        QTest::newRow("without history") << false;
        QTest::newRow("with undo redo") << true;
    }

    void ungroupIntoTransformedParentPreservesGeometry()
    {
        QFETCH(bool, useHistory);
        Document doc(500, 400);
        const QColor red(200, 30, 30);
        const QColor green(30, 200, 30);
        const QColor blue(30, 30, 200);
        const LayerId child1 = addSolidImage(doc, red, QPointF(180, 150), 24, 18);
        const LayerId child2 = addSolidImage(doc, green, QPointF(220, 180), 20, 22);
        AffineTransform t = doc.findLayer(child1)->transform;
        t.rotationDeg = 19.0;
        t.scaleX = -1.15;
        t.shearX = 0.1;
        t.shearY = 0.05;
        QVERIFY(doc.setLayerTransform(child1, t));
        t = doc.findLayer(child2)->transform;
        t.rotationDeg = -29.0;
        t.scaleY = -0.8;
        t.shearX = -0.12;
        QVERIFY(doc.setLayerTransform(child2, t));
        const LayerId innerId = groupWithoutHistory(doc, {child1, child2}, QStringLiteral("Inner"));

        const LayerId resident = addSolidImage(doc, blue, QPointF(295, 205), 22, 20);
        const LayerId outerId = groupWithoutHistory(
            doc, {innerId, resident}, QStringLiteral("Outer"));
        QVERIFY(doc.findLayer(innerId));
        QVERIFY(doc.findLayer(outerId));

        t = doc.findLayer(innerId)->transform;
        t.position += QPointF(8, -11);
        t.rotationDeg = 26.0;
        t.scaleX = -0.9;
        t.scaleY = 1.1;
        t.shearX = 0.07;
        t.shearY = -0.08;
        QVERIFY(doc.setLayerTransform(innerId, t));
        const AffineTransform innerOldTransform = t;
        t = doc.findLayer(outerId)->transform;
        t.position += QPointF(-14, 7);
        t.rotationDeg = -18.0;
        t.scaleX = 1.1;
        t.scaleY = -0.95;
        t.shearX = -0.06;
        t.shearY = 0.09;
        QVERIFY(doc.setLayerTransform(outerId, t));

        const QTransform child1Before = layerDocumentMatrix(doc, child1);
        const QTransform child2Before = layerDocumentMatrix(doc, child2);
        const QTransform residentBefore = layerDocumentMatrix(doc, resident);
        const QTransform outerBefore = layerDocumentMatrix(doc, outerId);
        const AffineTransform child1OldTransform = doc.findLayer(child1)->transform;
        const AffineTransform child2OldTransform = doc.findLayer(child2)->transform;
        doc.rootGroup()->transform.position = QPointF(23, 37);
        const AffineTransform rootTransform = doc.rootGroup()->transform;

        CommandStack stack;
        const LayerId next = LayerOperations::ungroup(
            doc, useHistory ? &stack : nullptr, innerId);
        QVERIFY(!next.isNull());
        QCOMPARE(doc.findLayer(innerId), nullptr);
        QCOMPARE(doc.parentOf(child1)->id(), outerId);
        QCOMPARE(doc.parentOf(child2)->id(), outerId);
        verifyLayerMatrixClose(doc, child1, child1Before);
        verifyLayerMatrixClose(doc, child2, child2Before);
        verifyLayerMatrixClose(doc, resident, residentBefore);
        verifyLayerMatrixClose(doc, outerId, outerBefore);
        QCOMPARE(doc.rootGroup()->transform, rootTransform);

        if (useHistory) {
            stack.undo();
            QVERIFY(doc.findLayer(innerId));
            QCOMPARE(doc.parentOf(child1)->id(), innerId);
            QCOMPARE(doc.parentOf(child2)->id(), innerId);
            QCOMPARE(doc.findLayer(innerId)->transform, innerOldTransform);
            QCOMPARE(doc.findLayer(child1)->transform, child1OldTransform);
            QCOMPARE(doc.findLayer(child2)->transform, child2OldTransform);
            verifyLayerMatrixClose(doc, child1, child1Before);
            verifyLayerMatrixClose(doc, child2, child2Before);
            verifyLayerMatrixClose(doc, resident, residentBefore);
            verifyLayerMatrixClose(doc, outerId, outerBefore);

            stack.redo();
            QCOMPARE(doc.findLayer(innerId), nullptr);
            verifyLayerMatrixClose(doc, child1, child1Before);
            verifyLayerMatrixClose(doc, child2, child2Before);
            verifyLayerMatrixClose(doc, resident, residentBefore);
            verifyLayerMatrixClose(doc, outerId, outerBefore);
            QCOMPARE(doc.rootGroup()->transform, rootTransform);
        }
    }

    void invalidReorderDoesNotMutateAnything()
    {
        Document doc(400, 300);
        const QColor red(200, 30, 30);
        const QColor green(30, 200, 30);
        const LayerId child = addSolidImage(doc, red, QPointF(180, 140), 24, 18);
        const LayerId innerId = groupWithoutHistory(doc, {child}, QStringLiteral("Inner"));
        const LayerId outerId = groupWithoutHistory(doc, {innerId}, QStringLiteral("Outer"));
        auto* inner = static_cast<GroupLayer*>(doc.findLayer(innerId));
        QVERIFY(inner);

        AffineTransform t = doc.findLayer(outerId)->transform;
        t.position += QPointF(15, -9);
        t.rotationDeg = 24.0;
        t.scaleX = -1.1;
        t.shearX = 0.1;
        QVERIFY(doc.setLayerTransform(outerId, t));
        const AffineTransform outerTransform = doc.findLayer(outerId)->transform;
        const AffineTransform innerTransform = inner->transform;
        const QTransform childBefore = layerDocumentMatrix(doc, child);
        doc.rootGroup()->transform.position = QPointF(51, 29);
        const AffineTransform rootTransform = doc.rootGroup()->transform;

        ReorderLayerCommand cycle(doc, outerId, inner, 0);
        cycle.redo();
        QCOMPARE(doc.parentOf(outerId), doc.rootGroup());
        QCOMPARE(doc.parentOf(innerId)->id(), outerId);
        QCOMPARE(doc.findLayer(outerId)->transform, outerTransform);
        QCOMPARE(inner->transform, innerTransform);
        verifyLayerMatrixClose(doc, child, childBefore);
        QCOMPARE(doc.rootGroup()->transform, rootTransform);

        // Um destino singular não permite calcular o rebase: o comando deve
        // falhar sem tocar sequer no transform da camada de origem.
        const LayerId moved = addSolidImage(doc, green, QPointF(110, 90), 20, 16);
        t = doc.findLayer(moved)->transform;
        t.rotationDeg = 31.0;
        t.scaleX = -1.3;
        t.shearY = 0.08;
        QVERIFY(doc.setLayerTransform(moved, t));
        const AffineTransform movedTransform = t;
        const int movedIndex = doc.indexOf(moved);
        t = inner->transform;
        t.scaleX = 0.0;
        inner->transform = t;
        const AffineTransform singularTransform = inner->transform;
        ReorderLayerCommand singular(doc, moved, inner, 0);
        singular.redo();
        QCOMPARE(doc.parentOf(moved), doc.rootGroup());
        QCOMPARE(doc.indexOf(moved), movedIndex);
        QCOMPARE(doc.findLayer(moved)->transform, movedTransform);
        QCOMPARE(inner->transform, singularTransform);
        QCOMPARE(doc.findLayer(outerId)->transform, outerTransform);
        QCOMPARE(doc.rootGroup()->transform, rootTransform);
    }

    void legacyProjectGroupIsMigratedOnLoad()
    {
        // Simula um projeto gravado ANTES da correção: grupo com
        // transform.position = (0,0) (assinatura do bug) e um filho no centro.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("legacy.creatorcanvas"));

        const QColor red(200, 30, 30);
        {
            Document doc(400, 300);
            const LayerId id = addSolidImage(doc, red, QPointF(200, 150));
            // grupo "bugado": assembly manual com position (0,0)
            auto group = std::make_unique<GroupLayer>();
            group->name = QStringLiteral("G");
            auto taken = doc.takeLayer(id);
            group->children.push_back(std::move(taken));
            QVERIFY(doc.addLayer(std::move(group)));

            QString err;
            QImage thumb(16, 16, QImage::Format_ARGB32);
            thumb.fill(Qt::white);
            QVERIFY2(cc::saveDocument(doc, path, &err, &thumb), qPrintable(err));
        }

        QString err;
        auto loaded = cc::loadDocument(path, &err);
        QVERIFY2(loaded != nullptr, qPrintable(err));

        // a migração no load deve ter posto position = centro do conteúdo
        const auto* g = static_cast<const GroupLayer*>(loaded->rootGroup()->children.back().get());
        QVERIFY(g->type() == LayerType::Group);
        QVERIFY2(g->transform.position != QPointF(0, 0), "grupo legacy não foi migrado");

        // e o render coloca a imagem no sítio (centro ~200,150), não no canto
        const QPoint c = redRectCenter(*loaded, red);
        QVERIFY2(qAbs(c.x() - 200) <= 2 && qAbs(c.y() - 150) <= 2,
                 qPrintable(QStringLiteral("centro=%1,%2").arg(c.x()).arg(c.y())));
    }

    void mergeDownWithSingleSelection()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto l1 = createRect("Bottom", 100, 100, 100, 100);
        auto l2 = createRect("Top", 150, 150, 100, 100);
        const LayerId id1 = l1->id();
        const LayerId id2 = l2->id();
        doc.addLayer(std::move(l1));
        doc.addLayer(std::move(l2));

        // Uma única seleção faz Merge Down (com a camada abaixo).
        const LayerId mergedId = LayerOperations::mergeSelected(doc, &stack, QList<LayerId>{id2}, "Merged down");
        QVERIFY(!mergedId.isNull());
        QCOMPARE(doc.findLayer(id1), nullptr);
        QCOMPARE(doc.findLayer(id2), nullptr);
        QVERIFY(doc.findLayer(mergedId)->type() == LayerType::Image);

        stack.undo();
        QVERIFY(doc.findLayer(id1) != nullptr);
        QVERIFY(doc.findLayer(id2) != nullptr);

        // Camada no fundo do grupo não tem nada abaixo: falha sem crash.
        const LayerId fail = LayerOperations::mergeSelected(doc, &stack, QList<LayerId>{id1}, "x");
        QVERIFY(fail.isNull());
    }

    void rasterizeSingleLayer()
    {
        Document doc(400, 300);
        CommandStack stack;

        auto shape = createRect("Rect", 50, 50, 120, 80);
        shape->fill = QColor(200, 30, 30);
        const LayerId shapeId = shape->id();
        doc.addLayer(std::move(shape));

        auto raster = LayerOperations::rasterLayerFor(doc, QList<LayerId>{shapeId}, "Rect rasterized");
        QVERIFY(raster != nullptr);
        QVERIFY(raster->type() == LayerType::Image);
        QCOMPARE(raster->naturalWidth, doc.width());
        QCOMPARE(raster->naturalHeight, doc.height());

        const LayerId rasterId = raster->id();
        auto cmd = std::make_unique<ReplaceLayersWithRasterCommand>(
            doc, QList<LayerId>{shapeId}, std::move(raster), QStringLiteral("layer.rasterize"));
        stack.execute(std::move(cmd));

        QCOMPARE(doc.findLayer(shapeId), nullptr);
        Layer* out = doc.findLayer(rasterId);
        QVERIFY(out != nullptr);
        QCOMPARE(out->type(), LayerType::Image);

        // O bitmap rasterizado tem píxeis não transparentes na área da forma.
        const QImage bmp = doc.assets().decodedImage(static_cast<ImageLayer*>(out)->assetId);
        QVERIFY(!bmp.isNull());
        bool anyOpaque = false;
        for (int y = 0; y < bmp.height() && !anyOpaque; ++y)
            for (int x = 0; x < bmp.width(); ++x)
                if (qAlpha(bmp.pixel(x, y)) > 0) { anyOpaque = true; break; }
        QVERIFY(anyOpaque);

        // Undo devolve a camada vetorial original.
        stack.undo();
        QVERIFY(doc.findLayer(shapeId) != nullptr);
        QCOMPARE(doc.findLayer(shapeId)->type(), LayerType::Shape);
        QCOMPARE(doc.findLayer(rasterId), nullptr);
    }
};

QTEST_MAIN(TestLayerOperations)
#include "test_layer_operations.moc"
