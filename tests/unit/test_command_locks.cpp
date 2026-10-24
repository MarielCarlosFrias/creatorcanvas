#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "core/commands/BaseCommands.h"
#include "core/commands/LayerStructureCommands.h"
#include "core/commands/LayerTransformCommands.h"
#include "core/commands/LayerPropertyCommands.h"

#include <QtTest>

using namespace cc;

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
};

QTEST_MAIN(TestCommandLocks)
#include "test_command_locks.moc"
