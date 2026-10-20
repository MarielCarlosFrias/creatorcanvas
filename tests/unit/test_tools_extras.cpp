#include <QImage>
#include <QMouseEvent>
#include <QTest>

#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/image/ImageProcessing.h"
#include "core/layers/Layer.h"
#include "tools/MarqueeTool.h"
#include "tools/PaintExtras.h"

using namespace cc;

namespace {

/// Documento com uma camada de imagem cujo bitmap é mapeado 1:1 no
/// espaço do documento (transform identity) — simplifica os testes.
std::unique_ptr<ImageLayer> makeImageLayer(Document& doc, int w, int h,
                                           const QImage& bitmap)
{
    auto layer = std::make_unique<ImageLayer>();
    layer->assetId = doc.assets().addImage(bitmap);
    layer->naturalWidth = w;
    layer->naturalHeight = h;
    layer->transform.position = QPointF(w / 2.0, h / 2.0); // matrix = identidade
    return layer;
}

QMouseEvent makeMouse(QEvent::Type type, const QPointF& pos,
                      Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    // ctor local+global (o de 5 argumentos está deprecado no Qt 6.10)
    return QMouseEvent(type, pos, pos, Qt::LeftButton, Qt::LeftButton, mods);
}

int diffPixels(const QImage& a, const QImage& b)
{
    if (a.size() != b.size())
        return -1;
    int diff = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x)
            if (a.pixel(x, y) != b.pixel(x, y))
                ++diff;
    return diff;
}

} // namespace

class TestToolsExtras final : public QObject
{
    Q_OBJECT

private slots:
    void toolProperties()
    {
        MarqueeTool marquee;
        QCOMPARE(marquee.type(), CanvasToolType::Marquee);
        marquee.setEllipse(true);
        QVERIFY(marquee.isEllipse());
        marquee.setFeather(500);
        QCOMPARE(marquee.feather(), 50); // clampado

        GradientTool gradient;
        QCOMPARE(gradient.type(), CanvasToolType::Gradient);
        gradient.setRadial(true);
        QVERIFY(gradient.isRadial());
        gradient.setOpacity(0.0);
        QVERIFY(gradient.opacity() >= 0.05); // clampado

        RetouchTool retouch;
        QCOMPARE(retouch.type(), CanvasToolType::Retouch);
        retouch.setMode(3);
        QCOMPARE(retouch.mode(), 3);
        retouch.setSize(0);
        QVERIFY(retouch.size() >= 2);

        HealingTool healing;
        QCOMPARE(healing.type(), CanvasToolType::Healing);
        healing.setMode(1);
        QCOMPARE(healing.mode(), 1);
        healing.setRadius(10000);
        QVERIFY(healing.radius() <= 400);
    }

    void selectionShapesAndCombine()
    {
        const QSize size(40, 30);
        const QImage rectSel = ImageProcessing::makeShapeSelection(
            size, QRectF(10, 10, 20, 10), false);
        QCOMPARE(rectSel.size(), size);
        QCOMPARE(rectSel.format(), QImage::Format_Alpha8);
        QVERIFY(rectSel.constScanLine(12)[15] > 200);  // dentro
        QVERIFY(rectSel.constScanLine(2)[2] == 0);     // fora

        const QImage ellipseSel = ImageProcessing::makeShapeSelection(
            size, QRectF(10, 10, 20, 10), true);
        QVERIFY(ellipseSel.constScanLine(15)[20] > 100); // centro
        QVERIFY(ellipseSel.constScanLine(11)[11] == 0);  // canto do bbox (fora da elipse)

        // Feather suaviza a borda
        const QImage feathered = ImageProcessing::makeShapeSelection(
            size, QRectF(10, 10, 20, 10), false, 4);
        QVERIFY(feathered.constScanLine(10)[15] < 255);
        QVERIFY(feathered.constScanLine(10)[15] > 0);

        // União e diferença
        const QImage left = ImageProcessing::makeShapeSelection(size, QRectF(0, 0, 20, 30), false);
        const QImage right = ImageProcessing::makeShapeSelection(size, QRectF(20, 0, 20, 30), false);
        const QImage united = ImageProcessing::combineSelections(left, right, false);
        QVERIFY(united.constScanLine(5)[2] > 200);
        QVERIFY(united.constScanLine(5)[37] > 200);
        const QImage subtracted = ImageProcessing::combineSelections(united, right, true);
        QVERIFY(subtracted.constScanLine(5)[2] > 200);
        QVERIFY(subtracted.constScanLine(5)[37] == 0);
    }

    void gradientChangesPixels()
    {
        QImage base(40, 20, QImage::Format_ARGB32_Premultiplied);
        base.fill(QColor(30, 30, 30));
        const QImage out = ImageProcessing::paintGradient(
            base, QPointF(0, 10), QPointF(39, 10), QColor(255, 0, 0),
            QColor(0, 0, 255), false, 1.0);
        QCOMPARE(out.size(), base.size());
        const QColor left = QColor::fromRgba(qUnpremultiply(out.pixel(0, 10)));
        const QColor right = QColor::fromRgba(qUnpremultiply(out.pixel(39, 10)));
        QVERIFY(left.red() > right.red());
        QVERIFY(right.blue() > left.blue());
        QVERIFY(diffPixels(base, out) > 0);
    }

    void gradientRespectsSelectionMask()
    {
        QImage base(40, 20, QImage::Format_ARGB32_Premultiplied);
        base.fill(Qt::black);
        const QImage mask = ImageProcessing::makeShapeSelection(
            base.size(), QRectF(0, 0, 20, 20), false);
        const QImage out = ImageProcessing::paintGradient(
            base, QPointF(0, 10), QPointF(39, 10), Qt::white, Qt::white,
            false, 1.0, mask);
        QVERIFY(QColor::fromRgba(qUnpremultiply(out.pixel(5, 10))).red() > 200);
        QVERIFY(QColor::fromRgba(qUnpremultiply(out.pixel(35, 10))).red() < 40);
    }

    void marqueeCreatesDocumentSelection()
    {
        Document doc(40, 30);
        QImage bitmap(40, 30, QImage::Format_ARGB32);
        bitmap.fill(Qt::white);
        auto layer = makeImageLayer(doc, 40, 30, bitmap);
        const LayerId layerId = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        MarqueeTool tool;
        ToolContext ctx;
        ctx.document = &doc;
        ctx.selectedLayerId = layerId;

        auto press = makeMouse(QEvent::MouseButtonPress, QPointF(5, 5));
        auto move = makeMouse(QEvent::MouseMove, QPointF(25, 20));
        auto release = makeMouse(QEvent::MouseButtonRelease, QPointF(25, 20));
        tool.mousePress(&press, QPointF(5, 5), ctx);
        tool.mouseMove(&move, QPointF(25, 20), ctx);
        tool.mouseRelease(&release, QPointF(25, 20), ctx);

        const QImage sel = doc.pixelSelection();
        QVERIFY(!sel.isNull());
        QCOMPARE(sel.size(), QSize(40, 30));
        QVERIFY(sel.constScanLine(10)[10] > 200); // dentro do retângulo
        QVERIFY(sel.constScanLine(2)[2] == 0);    // fora
    }

    void marqueeShiftAddsToSelection()
    {
        Document doc(40, 20);
        QImage bitmap(40, 20, QImage::Format_ARGB32);
        bitmap.fill(Qt::white);
        auto layer = makeImageLayer(doc, 40, 20, bitmap);
        const LayerId layerId = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        MarqueeTool tool;
        ToolContext ctx;
        ctx.document = &doc;
        ctx.selectedLayerId = layerId;

        auto press = makeMouse(QEvent::MouseButtonPress, QPointF(0, 0));
        auto release = makeMouse(QEvent::MouseButtonRelease, QPointF(5, 5));
        tool.mousePress(&press, QPointF(0, 0), ctx);
        tool.mouseRelease(&release, QPointF(5, 5), ctx);

        auto press2 = makeMouse(QEvent::MouseButtonPress, QPointF(30, 10),
                                Qt::ShiftModifier);
        auto release2 = makeMouse(QEvent::MouseButtonRelease, QPointF(38, 18),
                                  Qt::ShiftModifier);
        tool.mousePress(&press2, QPointF(30, 10), ctx);
        tool.mouseRelease(&release2, QPointF(38, 18), ctx);

        const QImage sel = doc.pixelSelection();
        QVERIFY(sel.constScanLine(2)[2] > 200);   // 1ª seleção
        QVERIFY(sel.constScanLine(15)[35] > 200); // 2ª soma
    }

    void gradientToolAppliesAndUndoes()
    {
        Document doc(32, 32);
        QImage bitmap(32, 32, QImage::Format_ARGB32_Premultiplied);
        bitmap.fill(QColor(20, 20, 20));
        auto layer = makeImageLayer(doc, 32, 32, bitmap);
        const LayerId layerId = layer->id();
        auto* img = layer.get();
        const LayerId originalAsset = img->assetId;
        QVERIFY(doc.addLayer(std::move(layer)));

        CommandStack history;
        GradientTool tool;
        tool.setStartColor(Qt::red);
        tool.setEndColor(Qt::blue);

        ToolContext ctx;
        ctx.document = &doc;
        ctx.history = &history;
        ctx.selectedLayerId = layerId;

        auto press = makeMouse(QEvent::MouseButtonPress, QPointF(2, 16));
        auto move = makeMouse(QEvent::MouseMove, QPointF(30, 16));
        auto release = makeMouse(QEvent::MouseButtonRelease, QPointF(30, 16));
        tool.mousePress(&press, QPointF(2, 16), ctx);
        tool.mouseMove(&move, QPointF(30, 16), ctx);
        tool.mouseRelease(&release, QPointF(30, 16), ctx);

        QCOMPARE(history.undoCount(), 1);
        auto* after = static_cast<ImageLayer*>(doc.findLayer(layerId));
        QVERIFY(after->assetId != originalAsset);
        const QImage result = doc.assets().decodedImage(after->assetId);
        QVERIFY(!result.isNull());

        history.undo();
        auto* undone = static_cast<ImageLayer*>(doc.findLayer(layerId));
        QCOMPARE(undone->assetId, originalAsset);
    }

    void retouchStrokeChangesPixelsForAllModes()
    {
        // Imagem colorida com borda vertical: Blur/Sharpen/Smudge dependem
        // da borda; Dodge/Burn/Sponge dependem da cor (sponge é no-op em
        // cinza, então a cor é necessária aqui).
        QImage base(48, 24, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < base.height(); ++y) {
            for (int x = 0; x < base.width(); ++x) {
                base.setPixelColor(x, y, x < 24 ? QColor(200, 40, 40)
                                                : QColor(40, 40, 200));
            }
        }

        for (int mode = 0; mode <= 5; ++mode) {
            const QImage out = ImageProcessing::retouchStroke(
                base, QPointF(20, 12), QPointF(26, 12),
                static_cast<ImageProcessing::RetouchMode>(mode), 12, 0.8);
            QVERIFY2(diffPixels(base, out) > 0,
                     qPrintable(QStringLiteral("mode %1 não alterou pixels").arg(mode)));
        }

        // Blur reduz o contraste na borda
        const QImage blurred = ImageProcessing::retouchStroke(
            base, QPointF(20, 12), QPointF(26, 12),
            ImageProcessing::RetouchMode::Blur, 12, 0.8);
        const int before = qAbs(QColor(base.pixel(23, 12)).red() - QColor(base.pixel(24, 12)).red());
        const int after = qAbs(QColor(blurred.pixel(23, 12)).red() - QColor(blurred.pixel(24, 12)).red());
        QVERIFY2(after < before, qPrintable(QStringLiteral("antes %1, depois %2").arg(before).arg(after)));
    }

    void retouchToolCommitsOneUndoStep()
    {
        Document doc(48, 24);
        QImage bitmap(48, 24, QImage::Format_ARGB32_Premultiplied);
        bitmap.fill(QColor(120, 120, 120));
        auto layer = makeImageLayer(doc, 48, 24, bitmap);
        const LayerId layerId = layer->id();
        auto* img = layer.get();
        const LayerId originalAsset = img->assetId;
        QVERIFY(doc.addLayer(std::move(layer)));

        CommandStack history;
        RetouchTool tool;
        tool.setMode(0); // Blur
        tool.setSize(16);
        tool.setStrength(0.6);

        ToolContext ctx;
        ctx.document = &doc;
        ctx.history = &history;
        ctx.selectedLayerId = layerId;

        auto press = makeMouse(QEvent::MouseButtonPress, QPointF(10, 10));
        auto move = makeMouse(QEvent::MouseMove, QPointF(30, 10));
        auto release = makeMouse(QEvent::MouseButtonRelease, QPointF(30, 10));
        tool.mousePress(&press, QPointF(10, 10), ctx);
        tool.mouseMove(&move, QPointF(30, 10), ctx);
        tool.mouseRelease(&release, QPointF(30, 10), ctx);

        QCOMPARE(history.undoCount(), 1);
        auto* after = static_cast<ImageLayer*>(doc.findLayer(layerId));
        QVERIFY(after->assetId != originalAsset);
        history.undo();
        QCOMPARE(static_cast<ImageLayer*>(doc.findLayer(layerId))->assetId, originalAsset);
    }

    void spotHealCopiesNearbyPixels()
    {
        QImage base(64, 32, QImage::Format_ARGB32_Premultiplied);
        base.fill(QColor(50, 50, 50));
        // "Defeito" vermelho na região alvo
        for (int y = 12; y <= 20; ++y)
            for (int x = 12; x <= 20; ++x)
                base.setPixelColor(x, y, QColor(220, 20, 20));

        const QImage healed = ImageProcessing::spotHeal(
            base, QPoint(16, 16), QPoint(48, 16), 10, 0.95);
        const QColor center = QColor::fromRgba(qUnpremultiply(healed.pixel(16, 16)));
        QVERIFY(center.red() < 120); // defeito removido
    }

    void redEyeNeutralizesRedPixels()
    {
        QImage base(32, 32, QImage::Format_ARGB32_Premultiplied);
        base.fill(QColor(60, 60, 60));
        for (int y = 12; y <= 20; ++y)
            for (int x = 12; x <= 20; ++x)
                base.setPixelColor(x, y, QColor(240, 30, 30));

        const QImage fixed = ImageProcessing::redEyeRemove(base, QPoint(16, 16), 10);
        const QColor center = QColor::fromRgba(qUnpremultiply(fixed.pixel(16, 16)));
        QVERIFY(center.red() < 120);
        QVERIFY(center.green() > 20);
    }

    void healingToolRedEyeCommits()
    {
        Document doc(32, 32);
        QImage bitmap(32, 32, QImage::Format_ARGB32_Premultiplied);
        bitmap.fill(QColor(60, 60, 60));
        for (int y = 12; y <= 20; ++y)
            for (int x = 12; x <= 20; ++x)
                bitmap.setPixelColor(x, y, QColor(240, 30, 30));
        auto layer = makeImageLayer(doc, 32, 32, bitmap);
        const LayerId layerId = layer->id();
        auto* img = layer.get();
        const LayerId originalAsset = img->assetId;
        QVERIFY(doc.addLayer(std::move(layer)));

        CommandStack history;
        HealingTool tool;
        tool.setMode(1); // Red Eye
        tool.setRadius(10);

        ToolContext ctx;
        ctx.document = &doc;
        ctx.history = &history;
        ctx.selectedLayerId = layerId;

        auto press = makeMouse(QEvent::MouseButtonPress, QPointF(16, 16));
        auto release = makeMouse(QEvent::MouseButtonRelease, QPointF(16, 16));
        tool.mousePress(&press, QPointF(16, 16), ctx);
        tool.mouseRelease(&release, QPointF(16, 16), ctx);

        QCOMPARE(history.undoCount(), 1);
        auto* after = static_cast<ImageLayer*>(doc.findLayer(layerId));
        QVERIFY(after->assetId != originalAsset);
        const QImage result = doc.assets().decodedImage(after->assetId);
        const QColor center = QColor::fromRgba(qUnpremultiply(result.pixel(16, 16)));
        QVERIFY(center.red() < 120);
    }
};

QTEST_MAIN(TestToolsExtras)
#include "test_tools_extras.moc"
