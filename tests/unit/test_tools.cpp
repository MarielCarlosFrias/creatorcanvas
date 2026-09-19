#include <QTest>
#include <QImage>
#include "tools/SelectTool.h"
#include "tools/PaintTool.h"
#include "tools/CropTool.h"
#include "tools/ScissorsTool.h"
#include "tools/RasterTools.h"
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

        QCOMPARE(tool.size(), 24);
        QCOMPARE(tool.color(), QColor(Qt::red));
        QCOMPARE(tool.opacity(), 0.8);
    }

    void testCropTool()
    {
        cc::CropTool tool;
        QCOMPARE(tool.type(), cc::CanvasToolType::Crop);

        cc::ToolContext ctx;
        tool.setAspectRatio(1.5, ctx);
        QCOMPARE(tool.aspectRatio(), 1.5);
    }

    void testScissorsTool()
    {
        cc::ScissorsTool tool;
        QCOMPARE(tool.type(), cc::CanvasToolType::Scissors);

        tool.setKeepInside(false);
        QCOMPARE(tool.keepInside(), false);
        tool.setAutoCrop(false);
        QCOMPARE(tool.autoCrop(), false);
    }

    void testRasterTools()
    {
        cc::MagicWandTool wand;
        QCOMPARE(wand.type(), cc::CanvasToolType::MagicWand);
        wand.setTolerance(40);
        QCOMPARE(wand.tolerance(), 40);
        wand.setContiguous(false);
        QCOMPARE(wand.contiguous(), false);

        cc::CloneStampTool clone;
        QCOMPARE(clone.type(), cc::CanvasToolType::CloneStamp);
        clone.setRadius(32);
        QCOMPARE(clone.radius(), 32);
        clone.setHardness(0.5);
        QCOMPARE(clone.hardness(), 0.5);
        clone.setOpacity(0.9);
        QCOMPARE(clone.opacity(), 0.9);

        cc::FloodFillTool fill;
        QCOMPARE(fill.type(), cc::CanvasToolType::FloodFill);
        fill.setColor(Qt::green);
        QCOMPARE(fill.color(), QColor(Qt::green));
        fill.setTolerance(30);
        QCOMPARE(fill.tolerance(), 30);
    }
};

QTEST_MAIN(TestTools)
#include "test_tools.moc"
