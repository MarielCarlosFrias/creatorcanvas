#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"

#include <QImage>
#include <QSignalSpy>
#include <QtTest>

using namespace cc;

/// O historico deixa de ser apenas por contagem: um orcamento de memoria
/// descarta os passos mais antigos quando os snapshots ficam grandes.
class TestHistoryBudget final : public QObject
{
    Q_OBJECT

private:
    static QImage bigSelection(int width, int height)
    {
        QImage image(width, height, QImage::Format_ARGB32);
        image.fill(Qt::white);
        return image;
    }

    void pushSelection(CommandStack& stack, Document& doc)
    {
        QImage selection = bigSelection(1300, 1000);
        stack.execute(std::make_unique<SetPixelSelectionCommand>(
            doc, doc.pixelSelection(), doc.pixelSelectionSpace(),
            selection, doc.pixelSelectionSpace()));
    }

private slots:
    void memoryBudgetDropsOldestSteps()
    {
        Document doc(400, 300);
        const qint64 budget = 48ll * 1024 * 1024;
        CommandStack stack(100, budget);
        stack.setAssetFootprintProbe([](const QSet<LayerId>&) -> qint64 { return 0; });

        for (int i = 0; i < 8; ++i)
            pushSelection(stack, doc);

        QVERIFY(stack.undoCount() >= 1);
        QVERIFY(stack.retainedBytes() <= budget * 2);
        QVERIFY(stack.undoCount() < 8);
    }

    void zeroBudgetMeansNoMemoryCap()
    {
        Document doc(400, 300);
        CommandStack stack(100, 0);
        QCOMPARE(stack.byteBudget(), 0);

        for (int i = 0; i < 6; ++i)
            pushSelection(stack, doc);
        QCOMPARE(stack.undoCount(), 6);
    }

    void countLimitStillWinsForCheapCommands()
    {
        Document doc(400, 300);
        CommandStack stack(5, 512ll * 1024 * 1024);
        for (int i = 0; i < 12; ++i) {
            auto layer = std::make_unique<TextLayer>();
            layer->name = QStringLiteral("L%1").arg(i);
            stack.execute(std::make_unique<AddLayerCommand>(doc, std::move(layer)));
        }
        QCOMPARE(stack.undoCount(), 5);
    }

    void loweringBudgetTrimsImmediately()
    {
        Document doc(400, 300);
        CommandStack stack(100, 1024ll * 1024 * 1024);
        for (int i = 0; i < 6; ++i)
            pushSelection(stack, doc);
        const int before = stack.undoCount();
        stack.setByteBudget(16ll * 1024 * 1024);
        QVERIFY(stack.undoCount() < before);
        QVERIFY(stack.retainedBytes() <= 16ll * 1024 * 1024 * 2);
        QVERIFY(stack.undoCount() >= 1);
    }
};

QTEST_MAIN(TestHistoryBudget)
#include "test_history_budget.moc"
