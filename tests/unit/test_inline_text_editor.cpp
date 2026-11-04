#include <QtTest>
#include <QWidget>
#include <QSignalSpy>

#include "core/layers/Layer.h"
#include "ui/inlinetexteditor.h"

using namespace cc;

class TestInlineTextEditor final : public QObject
{
    Q_OBJECT

private slots:
    void initialState()
    {
        QWidget parent;
        InlineTextEditor editor(&parent);
        QVERIFY(!editor.isEditing());
        QVERIFY(editor.editingLayerId().isNull());
    }

    void startEditAndCancel()
    {
        QWidget parent;
        InlineTextEditor editor(&parent);
        QSignalSpy spyCancel(&editor, &InlineTextEditor::editCancelled);
        QSignalSpy spyCommit(&editor, &InlineTextEditor::textCommitted);

        TextLayer layer;
        layer.content = QStringLiteral("Sample Text");
        layer.sizePt = 24.0;
        layer.color = QColor(Qt::black);
        const LayerId id = layer.id();

        editor.startEdit(id, layer, QRectF(10, 10, 200, 50), 1.0);
        QVERIFY(editor.isEditing());
        QCOMPARE(editor.editingLayerId(), id);

        editor.cancel();
        QVERIFY(!editor.isEditing());
        QVERIFY(editor.editingLayerId().isNull());
        QCOMPARE(spyCancel.count(), 1);
        QCOMPARE(spyCommit.count(), 0);
    }

    void startEditAndCommitUnchanged()
    {
        QWidget parent;
        InlineTextEditor editor(&parent);
        QSignalSpy spyCommit(&editor, &InlineTextEditor::textCommitted);

        TextLayer layer;
        layer.content = QStringLiteral("Hello");
        const LayerId id = layer.id();

        editor.startEdit(id, layer, QRectF(10, 10, 100, 30), 1.0);
        editor.commit();

        QVERIFY(!editor.isEditing());
        QCOMPARE(spyCommit.count(), 0); // No change -> no signal
    }
};

QTEST_MAIN(TestInlineTextEditor)
#include "test_inline_text_editor.moc"
