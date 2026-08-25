#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"

#include <QSignalSpy>
#include <QtTest>

using namespace cc;

namespace {

std::unique_ptr<TextLayer> makeText(const QString& name)
{
    auto layer = std::make_unique<TextLayer>();
    layer->name = name;
    return layer;
}

} // namespace

class TestHistory final : public QObject
{
    Q_OBJECT

private slots:
    void executeRunsRedoImmediately()
    {
        Document doc(100, 100);
        CommandStack stack;
        auto layer = makeText("A");
        const LayerId id = layer->id();

        stack.execute(std::make_unique<AddLayerCommand>(doc, std::move(layer)));

        QVERIFY(doc.findLayer(id) != nullptr);
        QCOMPARE(stack.undoCount(), 1);
        QCOMPARE(stack.redoCount(), 0);
        QCOMPARE(stack.nextUndoName(), QStringLiteral("layer.add"));
    }

    void undoRedoAddLayer()
    {
        Document doc(100, 100);
        CommandStack stack;
        auto layer = makeText("A");
        const LayerId id = layer->id();

        stack.execute(std::make_unique<AddLayerCommand>(doc, std::move(layer)));
        QVERIFY(doc.findLayer(id) != nullptr);

        stack.undo();
        QVERIFY(doc.findLayer(id) == nullptr);
        QVERIFY(stack.canRedo());

        stack.redo();
        QVERIFY(doc.findLayer(id) != nullptr);
        QCOMPARE(doc.indexOf(id), 0);
    }

    void undoRedoRemoveLayerRestoresPosition()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        auto b = makeText("B");
        auto c = makeText("C");
        const LayerId ib = b->id();
        QVERIFY(doc.addLayer(std::move(a)));
        QVERIFY(doc.addLayer(std::move(b)));
        QVERIFY(doc.addLayer(std::move(c)));

        CommandStack stack;
        stack.execute(std::make_unique<RemoveLayerCommand>(doc, ib));
        QVERIFY(doc.findLayer(ib) == nullptr);

        stack.undo();
        QVERIFY(doc.findLayer(ib) != nullptr);
        QVERIFY(doc.parentOf(ib) == doc.rootGroup());
        QCOMPARE(doc.indexOf(ib), 1);

        stack.redo();
        QVERIFY(doc.findLayer(ib) == nullptr);
    }

    void undoRedoReorder()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        auto b = makeText("B");
        auto c = makeText("C");
        const LayerId ia = a->id();
        QVERIFY(doc.addLayer(std::move(a)));
        QVERIFY(doc.addLayer(std::move(b)));
        QVERIFY(doc.addLayer(std::move(c)));

        CommandStack stack;
        stack.execute(std::make_unique<ReorderLayerCommand>(doc, ia, nullptr, 2));
        QCOMPARE(doc.indexOf(ia), 2);

        stack.undo();
        QCOMPARE(doc.indexOf(ia), 0);

        stack.redo();
        QCOMPARE(doc.indexOf(ia), 2);
    }

    void undoRedoDuplicateKeepsStableIds()
    {
        Document doc(100, 100);
        auto text = makeText("Title");
        text->content = QStringLiteral("Hello");
        const LayerId originalId = text->id();
        QVERIFY(doc.addLayer(std::move(text)));

        CommandStack stack;
        auto cmd = std::make_unique<DuplicateLayerCommand>(doc, originalId);
        DuplicateLayerCommand* rawCmd = cmd.get();
        stack.execute(std::move(cmd));

        const LayerId copyId = rawCmd->copyId();
        QVERIFY(!copyId.isNull());
        QCOMPARE(doc.indexOf(copyId), 1);
        QCOMPARE(static_cast<TextLayer*>(doc.findLayer(copyId))->content,
                 QStringLiteral("Hello"));

        stack.undo();
        QVERIFY(doc.findLayer(copyId) == nullptr);

        stack.redo();
        auto* copy = static_cast<TextLayer*>(doc.findLayer(copyId));
        QVERIFY(copy != nullptr);
        QCOMPARE(copy->name, QStringLiteral("Title copy"));
        QCOMPARE(doc.indexOf(copyId), 1);
    }

    void undoRedoPropertyCommands()
    {
        Document doc(100, 100);
        auto text = makeText("T");
        const LayerId id = text->id();
        QVERIFY(doc.addLayer(std::move(text)));

        CommandStack stack;
        auto* layer = static_cast<TextLayer*>(doc.findLayer(id));

        using RenameCmd = LayerPropertyCommand<QString>;
        stack.execute(std::make_unique<RenameCmd>(
            doc, id, QStringLiteral("layer.rename"), &Document::setLayerName,
            QStringLiteral("T"), QStringLiteral("Big")));
        QCOMPARE(layer->name, QStringLiteral("Big"));
        stack.undo();
        QCOMPARE(layer->name, QStringLiteral("T"));
        stack.redo();
        QCOMPARE(layer->name, QStringLiteral("Big"));

        using VisibilityCmd = LayerPropertyCommand<bool>;
        stack.execute(std::make_unique<VisibilityCmd>(
            doc, id, QStringLiteral("layer.visible"),
            &Document::setLayerVisible, true, false));
        QCOMPARE(layer->visible, false);
        stack.undo();
        QCOMPARE(layer->visible, true);

        using OpacityCmd = LayerPropertyCommand<float>;
        stack.execute(std::make_unique<OpacityCmd>(
            doc, id, QStringLiteral("layer.opacity"),
            &Document::setLayerOpacity, 1.0f, 0.25f));
        QCOMPARE(layer->opacity(), 0.25f);
        stack.undo();
        QCOMPARE(layer->opacity(), 1.0f);

        using BlendCmd = LayerPropertyCommand<BlendMode>;
        stack.execute(std::make_unique<BlendCmd>(
            doc, id, QStringLiteral("layer.blendMode"),
            &Document::setLayerBlendMode, BlendMode::Normal,
            BlendMode::Multiply));
        QCOMPARE(static_cast<int>(layer->blendMode),
                 static_cast<int>(BlendMode::Multiply));
        stack.undo();
        QCOMPARE(static_cast<int>(layer->blendMode),
                 static_cast<int>(BlendMode::Normal));

        stack.undo();
        stack.undo();
        stack.undo();
        QCOMPARE(layer->name, QStringLiteral("T"));
        QCOMPARE(layer->visible, true);
        QCOMPARE(layer->opacity(), 1.0f);
    }

    void newExecuteClearsRedoStack()
    {
        Document doc(100, 100);
        CommandStack stack;
        auto a = makeText("A");
        const LayerId idA = a->id();
        stack.execute(std::make_unique<AddLayerCommand>(doc, std::move(a)));

        stack.undo();
        QVERIFY(stack.canRedo());

        auto b = makeText("B");
        const LayerId idB = b->id();
        stack.execute(std::make_unique<AddLayerCommand>(doc, std::move(b)));
        QVERIFY(!stack.canRedo());

        stack.redo();
        QVERIFY(doc.findLayer(idB) != nullptr);
        QCOMPARE(stack.undoCount(), 1);

        stack.undo();
        QVERIFY(doc.findLayer(idB) == nullptr);
        QVERIFY(doc.findLayer(idA) == nullptr);
    }

    void limitDropsOldestCommand()
    {
        Document doc(100, 100);
        auto text = makeText("T");
        const LayerId id = text->id();
        QVERIFY(doc.addLayer(std::move(text)));

        CommandStack stack(2);
        using RenameCmd = LayerPropertyCommand<QString>;
        stack.execute(std::make_unique<RenameCmd>(
            doc, id, QStringLiteral("layer.rename"), &Document::setLayerName,
            QStringLiteral("v0"), QStringLiteral("v1")));
        stack.execute(std::make_unique<RenameCmd>(
            doc, id, QStringLiteral("layer.rename"), &Document::setLayerName,
            QStringLiteral("v1"), QStringLiteral("v2")));
        stack.execute(std::make_unique<RenameCmd>(
            doc, id, QStringLiteral("layer.rename"), &Document::setLayerName,
            QStringLiteral("v2"), QStringLiteral("v3")));
        QCOMPARE(stack.undoCount(), 2);
        QCOMPARE(static_cast<TextLayer*>(doc.findLayer(id))->name,
                 QStringLiteral("v3"));

        stack.undo();
        QCOMPARE(static_cast<TextLayer*>(doc.findLayer(id))->name,
                 QStringLiteral("v2"));
        stack.undo();
        QCOMPARE(static_cast<TextLayer*>(doc.findLayer(id))->name,
                 QStringLiteral("v1"));
        QVERIFY(!stack.canUndo());

        stack.redo();
        stack.redo();
        QCOMPARE(static_cast<TextLayer*>(doc.findLayer(id))->name,
                 QStringLiteral("v3"));
    }

    void signalsEmittedOnTransitions()
    {
        Document doc(100, 100);
        CommandStack stack;
        QSignalSpy undoSpy(&stack, &CommandStack::canUndoChanged);
        QSignalSpy redoSpy(&stack, &CommandStack::canRedoChanged);
        QVERIFY(undoSpy.isValid() && redoSpy.isValid());

        auto a = makeText("A");
        stack.execute(std::make_unique<AddLayerCommand>(doc, std::move(a)));
        QCOMPARE(undoSpy.count(), 1);
        QCOMPARE(redoSpy.count(), 0);

        stack.undo();
        QCOMPARE(undoSpy.count(), 2);
        QCOMPARE(redoSpy.count(), 1);

        stack.redo();
        QCOMPARE(undoSpy.count(), 3);
        QCOMPARE(redoSpy.count(), 2);

        auto b = makeText("B");
        stack.execute(std::make_unique<AddLayerCommand>(doc, std::move(b)));
        QCOMPARE(undoSpy.count(), 3);
        QCOMPARE(redoSpy.count(), 2);
    }

    void emptyStackOperationsAreSafe()
    {
        Document doc(100, 100);
        CommandStack stack;
        stack.undo();
        stack.redo();
        stack.clear();
        QVERIFY(stack.nextUndoName().isEmpty());
        QVERIFY(stack.nextRedoName().isEmpty());
        QCOMPARE(doc.rootGroup()->children.size(), std::size_t(0));
    }

    void clearDropsHistoryButKeepsEffects()
    {
        Document doc(100, 100);
        CommandStack stack;
        auto a = makeText("A");
        const LayerId id = a->id();
        stack.execute(std::make_unique<AddLayerCommand>(doc, std::move(a)));

        stack.clear();
        QVERIFY(!stack.canUndo());
        QVERIFY(!stack.canRedo());
        QVERIFY(doc.findLayer(id) != nullptr);
    }

    void interleavedGoldenScenario()
    {
        Document doc(1280, 720);
        CommandStack stack;

        auto bg = std::make_unique<BackgroundLayer>();
        const LayerId bgId = bg->id();
        bg->name = QStringLiteral("Background");
        stack.execute(std::make_unique<AddLayerCommand>(doc, std::move(bg)));

        auto title = makeText("Title");
        const LayerId titleId = title->id();
        title->content = QStringLiteral("Hello");
        stack.execute(std::make_unique<AddLayerCommand>(doc, std::move(title)));

        using RenameCmd = LayerPropertyCommand<QString>;
        stack.execute(std::make_unique<RenameCmd>(
            doc, titleId, QStringLiteral("layer.rename"),
            &Document::setLayerName, QStringLiteral("Title"),
            QStringLiteral("Main Title")));

        stack.execute(std::make_unique<ReorderLayerCommand>(doc, titleId,
                                                            nullptr, 0));
        QCOMPARE(doc.indexOf(titleId), 0);
        QCOMPARE(doc.indexOf(bgId), 1);

        stack.undo();
        QCOMPARE(doc.indexOf(titleId), 1);
        QCOMPARE(doc.indexOf(bgId), 0);

        stack.undo();
        QCOMPARE(static_cast<TextLayer*>(doc.findLayer(titleId))->name,
                 QStringLiteral("Title"));

        stack.undo();
        QVERIFY(doc.findLayer(titleId) == nullptr);
        QVERIFY(doc.findLayer(bgId) != nullptr);

        stack.undo();
        QVERIFY(doc.findLayer(bgId) == nullptr);
        QCOMPARE(doc.rootGroup()->children.size(), std::size_t(0));
        QVERIFY(!stack.canUndo());

        stack.redo();
        QVERIFY(doc.findLayer(bgId) != nullptr);
        stack.redo();
        QVERIFY(doc.findLayer(titleId) != nullptr);
        stack.redo();
        QCOMPARE(static_cast<TextLayer*>(doc.findLayer(titleId))->name,
                 QStringLiteral("Main Title"));
        stack.redo();
        QCOMPARE(doc.indexOf(titleId), 0);
        QVERIFY(!stack.canRedo());
        QCOMPARE(doc.rootGroup()->children.size(), std::size_t(2));
    }
};

QTEST_GUILESS_MAIN(TestHistory)
#include "test_history.moc"
