#include <QTest>
#include <QImage>
#include <QKeyEvent>
#include "tools/SelectTool.h"
#include "tools/PaintTool.h"
#include "tools/CropTool.h"
#include "tools/ScissorsTool.h"
#include "tools/RasterTools.h"
#include "tools/PenTool.h"
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

    void testFloodFillExecution()
    {
        cc::Document doc(100, 100);

        auto imgLayer = std::make_unique<cc::ImageLayer>();
        QImage blackImg(100, 100, QImage::Format_ARGB32);
        blackImg.fill(Qt::black);
        const cc::LayerId assetId = doc.assets().addImage(blackImg);
        imgLayer->assetId = assetId;
        imgLayer->naturalWidth = 100;
        imgLayer->naturalHeight = 100;
        imgLayer->transform.position = QPointF(50, 50);
        imgLayer->name = QStringLiteral("TestImage");

        const cc::LayerId layerId = imgLayer->id();
        doc.addLayer(std::move(imgLayer));

        cc::FloodFillTool fillTool;
        fillTool.setColor(Qt::white);
        fillTool.setTolerance(20);

        cc::ToolContext ctx;
        ctx.document = &doc;
        ctx.selectedLayerId = layerId;

        QMouseEvent press(QEvent::MouseButtonPress, QPointF(50, 50), QPointF(50, 50),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        fillTool.mousePress(&press, QPointF(50, 50), ctx);

        auto* afterLayer = static_cast<cc::ImageLayer*>(doc.findLayer(layerId));
        QVERIFY(afterLayer != nullptr);
        QVERIFY(afterLayer->assetId != assetId);

        const QImage result = doc.assets().decodedImage(afterLayer->assetId);
        QVERIFY(!result.isNull());
        QCOMPARE(result.pixelColor(50, 50), QColor(Qt::white));
    }

    void testPenToolReturnKeyAndFinish()
    {
        cc::Document doc(200, 200);

        cc::PenTool pen;
        pen.setStrokeColor(Qt::blue);
        pen.setFillColor(Qt::red);

        cc::ToolContext ctx;
        ctx.document = &doc;

        pen.activate(ctx);

        QMouseEvent p1(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10),
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        pen.mousePress(&p1, QPointF(10, 10), ctx);

        QMouseEvent p2(QEvent::MouseButtonPress, QPointF(50, 10), QPointF(50, 10),
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        pen.mousePress(&p2, QPointF(50, 10), ctx);

        QMouseEvent p3(QEvent::MouseButtonPress, QPointF(50, 50), QPointF(50, 50),
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        pen.mousePress(&p3, QPointF(50, 50), ctx);

        QVERIFY(pen.hasPath());
        QCOMPARE(pen.m_anchors.size(), size_t(3));

        // Pressionar Enter para finalizar e criar a VectorLayer
        QKeyEvent enterKey(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        pen.keyPress(&enterKey, ctx);

        // Caminho ativo agora deve estar limpo
        QVERIFY(!pen.hasPath());

        // Deve existir uma nova camada VectorLayer no documento
        const auto& children = doc.rootGroup()->children;
        bool foundVector = false;
        for (const auto& child : children) {
            if (child->type() == cc::LayerType::Vector) {
                foundVector = true;
                auto* vl = static_cast<cc::VectorLayer*>(child.get());
                QCOMPARE(vl->nodes.size(), size_t(3));
                QVERIFY(vl->closed);
                break;
            }
        }
        QVERIFY(foundVector);
    }
};

QTEST_MAIN(TestTools)
#include "test_tools.moc"
