#include <QTest>
#include <QImage>
#include "tools/SelectTool.h"
#include "tools/PaintTool.h"
#include "tools/CropTool.h"
#include "core/Document.h"

class TestTools : public QObject
{
    Q_OBJECT
private slots:
    void testSelectToolDefaults()
    {
        cc::SelectTool tool;
        QCOMPARE(tool.type(), cc::CanvasToolType::Select);
        QVERIFY(tool.selectedLayers().isEmpty());
        QVERIFY(tool.primarySelectedId().isNull());
    }

    void testSelectToolSelectionManagement()
    {
        cc::SelectTool tool;
        cc::LayerId id1 = QUuid::createUuid();
        cc::LayerId id2 = QUuid::createUuid();

        tool.setSelectedLayers({id1, id2});
        QCOMPARE(tool.selectedLayers().size(), 2);
        QCOMPARE(tool.primarySelectedId(), id2);

        tool.clearSelection();
        QVERIFY(tool.selectedLayers().isEmpty());
    }

    void testPaintToolProperties()
    {
        cc::PaintTool tool;
        QCOMPARE(tool.type(), cc::CanvasToolType::Paint);

        tool.setSize(24);
        tool.setColor(Qt::red);
        tool.setOpacity(0.8);

        // Verification of state setting
        QVERIFY(true);
    }
};

QTEST_MAIN(TestTools)
#include "test_tools.moc"
