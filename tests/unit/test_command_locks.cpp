#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "core/commands/BaseCommands.h"
#include "core/commands/LayerStructureCommands.h"
#include "core/commands/LayerTransformCommands.h"
#include "core/commands/LayerPropertyCommands.h"
#include "core/selection/SelectionOperations.h"

#include <QPainter>
#include <QtTest>

using namespace cc;

namespace {

QRgb unmult(const QImage& img, int x, int y)
{
    return qUnpremultiply(img.pixel(x, y));
}

} // namespace

class TestCommandLocks final : public QObject
{
    Q_OBJECT

private slots:
    void lockedLayerBlocksTransformCommand()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto layer = std::make_unique<TextLayer>();
        layer->name = QStringLiteral("Title");
        const LayerId id = layer->id();
        doc.addLayer(std::move(layer));

        // 1. Initial transform works when unlocked
        AffineTransform t1;
        t1.position = QPointF(50, 50);
        stack.execute(std::make_unique<SetLayerTransformCommand>(doc, id, AffineTransform(), t1));
        QCOMPARE(doc.findLayer(id)->transform.position, QPointF(50, 50));

        // 2. Lock position
        LayerLocks positionLock;
        positionLock.position = true;
        doc.setLayerLocks(id, positionLock);

        // Try transform via Command
        AffineTransform t2;
        t2.position = QPointF(100, 100);
        auto cmd = std::make_unique<SetLayerTransformCommand>(doc, id, t1, t2);
        QVERIFY(!cmd->canExecute());
        stack.execute(std::move(cmd));
        // Must remain unchanged at 50, 50!
        QCOMPARE(doc.findLayer(id)->transform.position, QPointF(50, 50));

        // Direct Document setter must also defensively reject
        QVERIFY(!doc.setLayerTransform(id, t2));
        QCOMPARE(doc.findLayer(id)->transform.position, QPointF(50, 50));

        // 3. Unlock position
        doc.setLayerLocks(id, LayerLocks());
        QVERIFY(doc.setLayerTransform(id, t2));
        QCOMPARE(doc.findLayer(id)->transform.position, QPointF(100, 100));
    }

    void lockedLayerBlocksRemoveCommand()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto layer = std::make_unique<TextLayer>();
        layer->name = QStringLiteral("Protected");
        const LayerId id = layer->id();
        doc.addLayer(std::move(layer));

        // Lock layer completely
        doc.setLayerLocked(id, true);
        QVERIFY(doc.effectiveLocks(id).all);

        // Command execution must be blocked
        auto removeCmd = std::make_unique<RemoveLayerCommand>(doc, id);
        QVERIFY(!removeCmd->canExecute());
        stack.execute(std::move(removeCmd));

        // Layer must still exist!
        QVERIFY(doc.findLayer(id) != nullptr);

        // Direct removeLayer must also defensively reject
        QVERIFY(!doc.removeLayer(id));
        QVERIFY(doc.findLayer(id) != nullptr);

        // Unlock allows removal
        doc.setLayerLocked(id, false);
        QVERIFY(!doc.effectiveLocks(id).all);
        QVERIFY(doc.removeLayer(id));
        QVERIFY(doc.findLayer(id) == nullptr);
    }

    void lockedLayerBlocksPropertyChanges()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto layer = std::make_unique<TextLayer>();
        layer->name = QStringLiteral("OriginalName");
        layer->setOpacity(1.0f);
        const LayerId id = layer->id();
        doc.addLayer(std::move(layer));

        // Lock completely
        doc.setLayerLocked(id, true);

        // Opacity command blocked
        auto opCmd = std::make_unique<LayerOpacityCommand>(
            doc, id, QStringLiteral("layer.opacity"), &Document::setLayerOpacity, 1.0f, 0.2f);
        QVERIFY(!opCmd->canExecute());
        stack.execute(std::move(opCmd));
        QCOMPARE(doc.findLayer(id)->opacity(), 1.0f);

        // Direct setter blocked
        QVERIFY(!doc.setLayerOpacity(id, 0.2f));
        QCOMPARE(doc.findLayer(id)->opacity(), 1.0f);

        // Rename blocked
        QVERIFY(!doc.setLayerName(id, QStringLiteral("HackedName")));
        QCOMPARE(doc.findLayer(id)->name, QStringLiteral("OriginalName"));

        // Unlock and verify properties change normally
        doc.setLayerLocked(id, false);
        QVERIFY(doc.setLayerOpacity(id, 0.5f));
        QCOMPARE(doc.findLayer(id)->opacity(), 0.5f);
        QVERIFY(doc.setLayerName(id, QStringLiteral("NewName")));
        QCOMPARE(doc.findLayer(id)->name, QStringLiteral("NewName"));
    }

    void lockedLayerBlocksPixelAndTextChanges()
    {
        Document doc(800, 600);

        auto layer = std::make_unique<TextLayer>();
        layer->content = QStringLiteral("Hello");
        const LayerId id = layer->id();
        doc.addLayer(std::move(layer));

        // Lock pixels
        LayerLocks pixelLock;
        pixelLock.pixels = true;
        doc.setLayerLocks(id, pixelLock);

        // Text editing blocked
        QVERIFY(!doc.setLayerTextContent(id, QStringLiteral("World")));
        QCOMPARE(static_cast<TextLayer*>(doc.findLayer(id))->content, QStringLiteral("Hello"));

        // Unlock pixels
        doc.setLayerLocks(id, LayerLocks());
        QVERIFY(doc.setLayerTextContent(id, QStringLiteral("World")));
        QCOMPARE(static_cast<TextLayer*>(doc.findLayer(id))->content, QStringLiteral("World"));
    }

    void groupLockPropagatesToChildren()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto group = std::make_unique<GroupLayer>();
        group->name = QStringLiteral("Folder");
        const LayerId groupId = group->id();
        doc.addLayer(std::move(group));

        auto child = std::make_unique<TextLayer>();
        child->name = QStringLiteral("Child");
        const LayerId childId = child->id();
        doc.addLayer(std::move(child), static_cast<GroupLayer*>(doc.findLayer(groupId)));

        // Child initially unlocked
        QVERIFY(!doc.effectiveLocks(childId).all);
        QVERIFY(!doc.effectiveLocks(childId).position);

        // Lock group
        doc.setLayerLocked(groupId, true);

        // Child inherits lock via effectiveLocks
        QVERIFY(doc.effectiveLocks(childId).all);

        // Transform command on child is blocked by ancestor group lock
        AffineTransform newT;
        newT.position = QPointF(200, 200);
        auto cmd = std::make_unique<SetLayerTransformCommand>(doc, childId, AffineTransform(), newT);
        QVERIFY(!cmd->canExecute());
        stack.execute(std::move(cmd));
        QCOMPARE(doc.findLayer(childId)->transform.position, QPointF(0, 0));

        // Direct removal of child blocked
        QVERIFY(!doc.removeLayer(childId));
        QVERIFY(doc.findLayer(childId) != nullptr);

        // Direct reorder of child blocked
        QVERIFY(!doc.reorderLayer(childId, doc.rootGroup(), 0));

        // Unlock group
        doc.setLayerLocked(groupId, false);
        QVERIFY(!doc.effectiveLocks(childId).all);

        // Child can now be modified
        QVERIFY(doc.setLayerTransform(childId, newT));
        QCOMPARE(doc.findLayer(childId)->transform.position, QPointF(200, 200));
    }

    void unlockAlwaysPermitted()
    {
        Document doc(800, 600);

        auto layer = std::make_unique<TextLayer>();
        const LayerId id = layer->id();
        doc.addLayer(std::move(layer));

        // Lock via boolean
        QVERIFY(doc.setLayerLocked(id, true));
        QVERIFY(doc.effectiveLocks(id).all);

        // Unlocking via boolean is always permitted
        QVERIFY(doc.setLayerLocked(id, false));
        QVERIFY(!doc.effectiveLocks(id).all);

        // Lock via LayerLocks
        LayerLocks fullLock;
        fullLock.all = true;
        fullLock.position = true;
        fullLock.pixels = true;
        fullLock.transparency = true;
        QVERIFY(doc.setLayerLocks(id, fullLock));
        QVERIFY(doc.effectiveLocks(id).all);

        // Unlocking via LayerLocks is always permitted
        QVERIFY(doc.setLayerLocks(id, LayerLocks()));
        QVERIFY(!doc.effectiveLocks(id).all);
    }

    void lockAndUnlockViaCommandStack()
    {
        Document doc(800, 600);
        CommandStack stack;

        auto layer = std::make_unique<TextLayer>();
        const LayerId id = layer->id();
        doc.addLayer(std::move(layer));

        // 1. Lock layer via LayerLockCommand (name: "layer.lock")
        auto lockCmd = std::make_unique<LayerLockCommand>(
            doc, id, QStringLiteral("layer.lock"),
            &Document::setLayerLocked, false, true);
        QVERIFY(lockCmd->canExecute());
        stack.execute(std::move(lockCmd));

        QVERIFY(doc.findLayer(id)->locked);
        QVERIFY(doc.findLayer(id)->locks.all);
        QVERIFY(doc.effectiveLocks(id).all);

        // 2. Unlock layer via LayerLockCommand - MUST be allowed to execute!
        auto unlockCmd = std::make_unique<LayerLockCommand>(
            doc, id, QStringLiteral("layer.lock"),
            &Document::setLayerLocked, true, false);
        QVERIFY(unlockCmd->canExecute());
        stack.execute(std::move(unlockCmd));

        QVERIFY(!doc.findLayer(id)->locked);
        QVERIFY(!doc.findLayer(id)->locks.all);
        QVERIFY(!doc.effectiveLocks(id).all);

        // 3. Test Undo (locks again)
        stack.undo();
        QVERIFY(doc.findLayer(id)->locked);
        QVERIFY(doc.effectiveLocks(id).all);

        // 4. Test Redo (unlocks again)
        stack.redo();
        QVERIFY(!doc.findLayer(id)->locked);
        QVERIFY(!doc.effectiveLocks(id).all);
    }

    void synchronizeLockedAndLocksAll()
    {
        Document doc(800, 600);

        auto layer = std::make_unique<TextLayer>();
        const LayerId id = layer->id();
        doc.addLayer(std::move(layer));

        // Boolean setter synchronizes locks.all
        doc.setLayerLocked(id, true);
        QVERIFY(doc.findLayer(id)->locked);
        QVERIFY(doc.findLayer(id)->locks.all);

        doc.setLayerLocked(id, false);
        QVERIFY(!doc.findLayer(id)->locked);
        QVERIFY(!doc.findLayer(id)->locks.all);

        // LayerLocks setter synchronizes boolean locked
        LayerLocks fullLock;
        fullLock.all = true;
        doc.setLayerLocks(id, fullLock);
        QVERIFY(doc.findLayer(id)->locked);
        QVERIFY(doc.findLayer(id)->locks.all);

        doc.setLayerLocks(id, LayerLocks());
        QVERIFY(!doc.findLayer(id)->locked);
        QVERIFY(!doc.findLayer(id)->locks.all);
    }

    void pixelLockBlocksImageModification()
    {
        Document doc(800, 600);
        CommandStack stack;

        QImage bitmap(800, 600, QImage::Format_ARGB32_Premultiplied);
        bitmap.fill(Qt::white);
        auto layer = std::make_unique<ImageLayer>();
        layer->name = QStringLiteral("Image");
        layer->assetId = doc.assets().addImage(bitmap);
        layer->naturalWidth = 800;
        layer->naturalHeight = 600;
        layer->transform.position = QPointF(400, 300);
        const LayerId id = layer->id();
        const LayerId oldAssetId = layer->assetId;
        const AffineTransform oldTransform = layer->transform;
        doc.addLayer(std::move(layer));

        // Unlocked: modification succeeds
        QImage modified(800, 600, QImage::Format_ARGB32_Premultiplied);
        modified.fill(Qt::black);
        const LayerId newAssetId = doc.assets().addImage(modified);
        auto cmd = std::make_unique<ModifyImageLayerCommand>(
            doc, id, oldAssetId, 800, 600, oldTransform,
            newAssetId, 800, 600, oldTransform,
            QStringLiteral("image.modify"));
        QVERIFY(cmd->canExecute());
        stack.execute(std::move(cmd));
        QCOMPARE(static_cast<ImageLayer*>(doc.findLayer(id))->assetId, newAssetId);

        // Lock pixels: modification blocked via command
        LayerLocks pixelLock;
        pixelLock.pixels = true;
        doc.setLayerLocks(id, pixelLock);
        QVERIFY(SelectionOperations::canEditPixels(doc, id) == false);

        auto blockedCmd = std::make_unique<ModifyImageLayerCommand>(
            doc, id, newAssetId, 800, 600, oldTransform,
            newAssetId, 800, 600, oldTransform,
            QStringLiteral("image.modify"));
        QVERIFY(!blockedCmd->canExecute());
        stack.execute(std::move(blockedCmd));
        QCOMPARE(static_cast<ImageLayer*>(doc.findLayer(id))->assetId, newAssetId);

        // Direct Document setter also defensively rejects
        QVERIFY(!doc.setImageLayerAsset(id, newAssetId, 800, 600));
        QCOMPARE(static_cast<ImageLayer*>(doc.findLayer(id))->assetId, newAssetId);

        // Unlock allows modification again
        doc.setLayerLocks(id, LayerLocks());
        QVERIFY(doc.setImageLayerAsset(id, newAssetId, 800, 600));
    }

    void selectionMaskRestrictsFilterToSelectedArea()
    {
        // Comportamento central do PhotoCraft: com uma seleção ativa,
        // um filtro/ajuste só afeta a ÁREA SELECIONADA — o resto fica
        // intacto.
        QImage original(10, 10, QImage::Format_ARGB32_Premultiplied);
        original.fill(QColor(200, 200, 200));

        QImage modified(10, 10, QImage::Format_ARGB32_Premultiplied);
        modified.fill(QColor(0, 0, 0));

        // Seleção: apenas a metade esquerda (x < 5) selecionada.
        QImage selection(10, 10, QImage::Format_Alpha8);
        selection.fill(0);
        for (int y = 0; y < 10; ++y)
            for (int x = 0; x < 5; ++x)
                selection.scanLine(y)[x] = 255;

        const QImage masked = SelectionOperations::applySelectionMask(
            original, modified, selection);

        // Esquerda (selecionada): modificado (preto).
        QCOMPARE(qRed(unmult(masked, 2, 5)), 0);
        // Direita (não selecionada): original (cinza claro).
        QCOMPARE(qRed(unmult(masked, 7, 5)), 200);

        // Sem seleção: modificado completo.
        const QImage unmasked = SelectionOperations::applySelectionMask(
            original, modified, QImage());
        QCOMPARE(qRed(unmult(unmasked, 7, 5)), 0);
    }

    void groupLockPropagatesToImageChild()
    {
        Document doc(100, 100);

        auto group = std::make_unique<GroupLayer>();
        group->name = QStringLiteral("Folder");
        const LayerId groupId = group->id();
        doc.addLayer(std::move(group));

        QImage bitmap(100, 100, QImage::Format_ARGB32_Premultiplied);
        bitmap.fill(Qt::white);
        auto child = std::make_unique<ImageLayer>();
        child->name = QStringLiteral("ImageChild");
        child->assetId = doc.assets().addImage(bitmap);
        child->naturalWidth = 100;
        child->naturalHeight = 100;
        const LayerId childId = child->id();
        doc.addLayer(std::move(child), static_cast<GroupLayer*>(doc.findLayer(groupId)));

        // O filho não tem lock próprio, mas herda o do grupo
        QVERIFY(doc.effectiveLocks(childId).pixels == false); // group not locked yet

        // Tranca o grupo — o filho herda o lock de pixels
        LayerLocks groupLock;
        groupLock.pixels = true;
        doc.setLayerLocks(groupId, groupLock);
        QVERIFY(doc.effectiveLocks(childId).pixels);
        QVERIFY(!SelectionOperations::canEditPixels(doc, childId));

        // Qualquer modificação no filho é bloqueada
        QImage black(100, 100, QImage::Format_ARGB32_Premultiplied);
        black.fill(Qt::black);
        const LayerId newAssetId = doc.assets().addImage(black);
        QVERIFY(!doc.setImageLayerAsset(childId, newAssetId, 100, 100));

        // Destravar o grupo devolve a liberdade ao filho
        doc.setLayerLocks(groupId, LayerLocks());
        QVERIFY(!doc.effectiveLocks(childId).pixels);
        QVERIFY(SelectionOperations::canEditPixels(doc, childId));
    }
};

QTEST_MAIN(TestCommandLocks)
#include "test_command_locks.moc"
