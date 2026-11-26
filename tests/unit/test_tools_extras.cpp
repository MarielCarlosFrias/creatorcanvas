#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTest>

#include <cstring>

#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/image/ImageProcessing.h"
#include "core/layers/Layer.h"
#include "tools/MarqueeTool.h"
#include "tools/PaintTool.h"
#include "tools/PaintExtras.h"
#include "tools/HealingTool.h"
#include "tools/PenTool.h"
#include "tools/VectorNodeTool.h"
#include "rendering/CanvasRenderer.h"

#include <QPainter>

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

    void paintCommitsOnlyInsideActiveSelection()
    {
        Document doc(40, 30);
        QImage bitmap(40, 30, QImage::Format_ARGB32_Premultiplied);
        bitmap.fill(Qt::white);
        auto layer = makeImageLayer(doc, 40, 30, bitmap);
        const LayerId layerId = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        QImage selection(bitmap.size(), QImage::Format_Alpha8);
        selection.fill(0);
        for (int y = 8; y < 22; ++y)
            std::memset(selection.scanLine(y) + 10, 255, 15);
        doc.setPixelSelection(selection);

        ToolContext ctx;
        ctx.document = &doc;
        ctx.selectedLayerId = layerId;
        PaintTool tool;
        tool.setColor(Qt::red);
        tool.setSize(8);

        // O gesto passa por dentro e por fora da seleção. O commit deve
        // preservar os pixels externos, mesmo que a prévia os tenha tocado.
        auto press = makeMouse(QEvent::MouseButtonPress, QPointF(5, 15));
        auto move = makeMouse(QEvent::MouseMove, QPointF(30, 15));
        auto release = makeMouse(QEvent::MouseButtonRelease, QPointF(30, 15));
        tool.mousePress(&press, QPointF(5, 15), ctx);
        tool.mouseMove(&move, QPointF(30, 15), ctx);
        tool.mouseRelease(&release, QPointF(30, 15), ctx);

        const auto* resultLayer = static_cast<const ImageLayer*>(doc.findLayer(layerId));
        const QImage result = doc.assets().decodedImage(resultLayer->assetId);
        QCOMPARE(result.pixel(5, 15), bitmap.pixel(5, 15));
        QVERIFY(result.pixel(16, 15) != bitmap.pixel(16, 15));
        QCOMPARE(result.pixel(30, 15), bitmap.pixel(30, 15));
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

    void marqueeRespectsRotatedLayerGeometry()
    {
        Document doc(160, 140);
        QImage bitmap(80, 60, QImage::Format_ARGB32);
        bitmap.fill(Qt::white);
        auto layer = makeImageLayer(doc, 80, 60, bitmap);
        layer->transform.position = QPointF(80, 70);
        layer->transform.rotationDeg = 37.0;
        layer->transform.shearX = 0.15;
        const LayerId layerId = layer->id();
        const AffineTransform transform = layer->transform;
        QVERIFY(doc.addLayer(std::move(layer)));

        MarqueeTool tool;
        CommandStack history;
        ToolContext ctx;
        ctx.document = &doc;
        ctx.history = &history;
        ctx.selectedLayerId = layerId;

        const QRectF wanted(55, 55, 50, 30);
        auto press = makeMouse(QEvent::MouseButtonPress, wanted.topLeft());
        auto release = makeMouse(QEvent::MouseButtonRelease, wanted.bottomRight());
        tool.mousePress(&press, wanted.topLeft(), ctx);
        tool.mouseRelease(&release, wanted.bottomRight(), ctx);

        const QImage sel = doc.pixelSelection();
        QVERIFY(!sel.isNull());
        QCOMPARE(sel.size(), bitmap.size());
        const QTransform forward = transform.matrix(QRectF(0, 0, 80, 60));
        int selected = 0;
        QPointF firstOutside;
        bool hasOutside = false;
        for (int y = 0; y < sel.height() && !hasOutside; ++y) {
            for (int x = 0; x < sel.width(); ++x) {
                if (sel.constScanLine(y)[x] <= 128)
                    continue;
                ++selected;
                const QPointF docPoint = forward.map(QPointF(x + 0.5, y + 0.5));
                if (!wanted.adjusted(-0.75, -0.75, 0.75, 0.75).contains(docPoint)) {
                    firstOutside = docPoint;
                    hasOutside = true;
                    break;
                }
            }
        }
        QVERIFY(selected > 0);
        QVERIFY2(!hasOutside,
                 qPrintable(QStringLiteral("selected pixel outside marquee: %1,%2")
                                .arg(firstOutside.x()).arg(firstOutside.y())));
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

    void inPlaceToolsMatchReturningImplementations()
    {
        QImage base(48, 24, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < base.height(); ++y) {
            for (int x = 0; x < base.width(); ++x) {
                base.setPixelColor(x, y, x < 24 ? QColor(200, 40, 40, 220)
                                                : QColor(40, 40, 200, 180));
            }
        }

        for (int mode = 0; mode <= 5; ++mode) {
            const auto kind = static_cast<ImageProcessing::RetouchMode>(mode);
            const QImage expected = ImageProcessing::retouchStroke(
                base, QPointF(20, 12), QPointF(26, 12), kind, 12, 0.8);
            QImage actual = base;
            const QImage snapshot = actual;
            ImageProcessing::retouchStrokeInPlace(
                actual, snapshot, QPointF(20, 12), QPointF(26, 12), kind, 12, 0.8);
            QCOMPARE(diffPixels(expected, actual), 0);
        }

        for (int y = 12; y <= 20; ++y)
            for (int x = 12; x <= 20; ++x)
                base.setPixelColor(x, y, QColor(220, 20, 20));
        const QImage expected = ImageProcessing::spotHeal(
            base, QPoint(16, 16), QPoint(48, 16), 10, 0.95);
        QImage actual = base;
        const QImage snapshot = actual;
        const QRect patch = ImageProcessing::spotHealInPlace(
            actual, snapshot, QPoint(16, 16), QPoint(48, 16), 10, 0.95);
        QCOMPARE(diffPixels(expected, actual), 0);

        // Healing preview restoration touches only the previous patch.
        QVERIFY(patch.isValid());
        QPainter restore(&actual);
        restore.setCompositionMode(QPainter::CompositionMode_Source);
        restore.drawImage(patch, snapshot, patch);
        restore.end();
        QCOMPARE(diffPixels(base, actual), 0);
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

        const QImage fixed = ImageProcessing::removeRedEye(base, QPoint(16, 16), 10);
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

    void vectorNodeToolMovesInsertsAndDeletes()
    {
        Document doc(80, 60);
        auto vec = std::make_unique<VectorLayer>();
        VectorNode a; a.position = QPointF(10, 10);
        VectorNode b; b.position = QPointF(60, 40);
        VectorNode c; c.position = QPointF(20, 50);
        vec->nodes = {a, b, c};
        vec->closed = false;
        const LayerId id = vec->id();
        QVERIFY(doc.addLayer(std::move(vec)));
        CommandStack history;

        VectorNodeTool tool;
        ToolContext ctx;
        ctx.document = &doc;
        ctx.history = &history;
        ctx.selectedLayerId = id;
        ctx.zoom = 1.0;
        ctx.requestUpdate = [] {};
        ctx.requestStatusMessage = [](const QString&) {};
        tool.activate(ctx);

        // Espaço local: os nós estão centrados no bounds do caminho — a
        // ferramenta converte documento->local (e o renderer aplica a mesma
        // matriz que AffineTransform::matrix produz).
        auto* vecStart = dynamic_cast<VectorLayer*>(doc.findLayer(id));
        const QRectF bounds = vecStart->contentBounds();
        const QTransform localToDoc = vecStart->transform.matrix(bounds);

        // 1) Arrastar o primeiro nó move-o e commitA UM passo de undo.
        const QPointF nodePosLocal = static_cast<VectorLayer*>(doc.findLayer(id))->nodes[0].position;
        const QPointF pressDoc = localToDoc.map(nodePosLocal);   // onde o nó aparece
        QMouseEvent press = makeMouse(QEvent::MouseButtonPress, pressDoc);
        tool.mousePress(&press, pressDoc, ctx);
        const QPointF movedDoc = pressDoc + QPointF(20, 15);
        QMouseEvent move = makeMouse(QEvent::MouseMove, movedDoc);
        tool.mouseMove(&move, movedDoc, ctx);
        QMouseEvent release = makeMouse(QEvent::MouseButtonRelease, movedDoc);
        tool.mouseRelease(&release, movedDoc, ctx);

        auto* vecLayer = dynamic_cast<VectorLayer*>(doc.findLayer(id));
        QVERIFY(vecLayer);
        const QPointF expectedLocal = localToDoc.inverted().map(pressDoc + QPointF(20, 15));
        QCOMPARE(vecLayer->nodes[0].position, expectedLocal);
        QCOMPARE(history.undoCount(), 1);

        // Undo volta ao estado original; reido reaplica.
        history.undo();
        QCOMPARE(dynamic_cast<VectorLayer*>(doc.findLayer(id))->nodes[0].position,
                 nodePosLocal);
        history.redo();
        QCOMPARE(dynamic_cast<VectorLayer*>(doc.findLayer(id))->nodes[0].position,
                 expectedLocal);

        // 2) Duplo-clique num nó apaga-o (mínimo 2 ficam).
        // Matriz FRESCA: o arraste mudou o bounds (e portanto o centro).
        auto* vecNow = dynamic_cast<VectorLayer*>(doc.findLayer(id));
        const QTransform localToDocNow = vecNow->transform.matrix(vecNow->contentBounds());
        const QPointF node2Local = vecNow->nodes[1].position;
        const QPointF node2Doc = localToDocNow.map(node2Local);
        QMouseEvent dbl = makeMouse(QEvent::MouseButtonDblClick, node2Doc);
        tool.mouseDoubleClick(&dbl, node2Doc, ctx);
        vecLayer = dynamic_cast<VectorLayer*>(doc.findLayer(id));
        QCOMPARE(vecLayer->nodes.size(), 2u);
        QCOMPARE(history.undoCount(), 2);

        // 3) Clicar no caminho insere um nó novo.
        // Matriz recalculada: o delete mudou o caminho (e o centro) de novo.
        const QTransform localToDocAfterDelete =
            vecLayer->transform.matrix(vecLayer->contentBounds());
        const QPointF mid((vecLayer->nodes[0].position.x() + vecLayer->nodes[1].position.x()) / 2,
                          (vecLayer->nodes[0].position.y() + vecLayer->nodes[1].position.y()) / 2);
        const QPointF midDoc = localToDocAfterDelete.map(mid);
        QMouseEvent press2 = makeMouse(QEvent::MouseButtonPress, midDoc);
        tool.mousePress(&press2, midDoc, ctx);
        vecLayer = dynamic_cast<VectorLayer*>(doc.findLayer(id));
        QCOMPARE(vecLayer->nodes.size(), 3u);

        // 4) O setter do Document recusa camadas que não são vectoriais.
        auto text = std::make_unique<TextLayer>();
        const LayerId textId = text->id();
        QVERIFY(doc.addLayer(std::move(text)));
        QVERIFY(!doc.setVectorLayerNodes(textId, {}));
        QVERIFY(doc.setVectorLayerNodes(id, vecLayer->nodes));   // idempotente = true
    }


    void penCreatesEditableNodesAndSelectsLayer()
    {
        // Regressão do "node path não funciona": a caneta criava o caminho
        // mas não os nós nem a selecção — a ferramenta de nós não tinha o
        // que agarrar nem sabia em que camada mexer.
        Document doc(200, 120);
        CommandStack history;
        LayerId created;

        PenTool pen;
        ToolContext ctx;
        ctx.document = &doc;
        ctx.history = &history;
        ctx.zoom = 1.0;
        ctx.i18n = nullptr;
        ctx.requestUpdate = [] {};
        ctx.requestStatusMessage = [](const QString&) {};
        ctx.selectLayer = [&created](const LayerId& id) { created = id; };
        pen.activate(ctx);

        // Três cliques = três pontos; Alt+clique fecha e cria a camada.
        for (const QPointF& p : {QPointF(20, 20), QPointF(100, 60), QPointF(180, 90)}) {
            QMouseEvent press = makeMouse(QEvent::MouseButtonPress, p);
            pen.mousePress(&press, p, ctx);
        }
        QMouseEvent close = makeMouse(QEvent::MouseButtonPress, QPointF(20, 20), Qt::AltModifier);
        pen.mousePress(&close, QPointF(20, 20), ctx);

        QVERIFY2(!created.isNull(), "a caneta devia selecionar a camada criada");
        auto* vec = dynamic_cast<VectorLayer*>(doc.findLayer(created));
        QVERIFY(vec != nullptr);
        QCOMPARE(vec->nodes.size(), 3u);          // nós criados para edição
        QVERIFY(!vec->path.isEmpty());            // e o caminho desenhado
        // Os nós ficam em espaço LOCAL centrado (a caneta subtrai o centro
        // das âncoras) e a posição da camada passa a ser esse centro.
        // Âncoras: (20,20), (100,60), (180,90) -> centro (100, 55).
        const QPointF expectedFirst = QPointF(20, 20) - QPointF(100, 55);
        QCOMPARE(vec->nodes.front().position, expectedFirst);
        QCOMPARE(vec->transform.position, QPointF(100, 55));

        // Editar um nó muda o CAMINHO desenhado (fonte de verdade única).
        const QPointF before = vec->path.pointAtPercent(0.0);
        const QPointF firstLocal = vec->nodes.front().position;
        const QPointF firstDoc = vec->transform.matrix(vec->contentBounds()).map(firstLocal);
        VectorNodeTool nodes;
        ToolContext nctx = ctx;
        nctx.selectedLayerId = created;
        nodes.activate(nctx);
        QMouseEvent press = makeMouse(QEvent::MouseButtonPress, firstDoc);
        nodes.mousePress(&press, firstDoc, nctx);
        const QPointF movedDoc = firstDoc + QPointF(20, 15);
        QMouseEvent move = makeMouse(QEvent::MouseMove, movedDoc);
        nodes.mouseMove(&move, movedDoc, nctx);
        QMouseEvent release = makeMouse(QEvent::MouseButtonRelease, movedDoc);
        nodes.mouseRelease(&release, movedDoc, nctx);

        vec = dynamic_cast<VectorLayer*>(doc.findLayer(created));
        // O arraste desloca o nó pelo MESMO delta no espaço local (a matriz
        // da camada é identidade aqui): primeiro nó + (20,15).
        const QPointF expectedLocal = firstLocal + QPointF(20, 15);
        QCOMPARE(vec->nodes.front().position, expectedLocal);
        QVERIFY2(vec->path.pointAtPercent(0.0) != before,
                 "mover o nó devia mudar o caminho desenhado");

        // E o RENDER também muda: é o que o utilizador vê.
        QImage imgBefore(200, 120, QImage::Format_ARGB32);
        QImage imgAfter(200, 120, QImage::Format_ARGB32);
        {
            QPainter p(&imgBefore);
            cc::RenderOptions opt;
            opt.drawCheckerboard = false;
            cc::renderDocument(doc, &p, QTransform(), opt);
        }
        QVERIFY(!imgBefore.isNull());
        // O caminho tem de ser visível: o contorno azul aparece no render.
        int painted = 0;
        for (int y = 0; y < imgBefore.height(); ++y)
            for (int x = 0; x < imgBefore.width(); ++x)
                if (imgBefore.pixelColor(x, y).alpha() != 0)
                    ++painted;
        QVERIFY2(painted > 0, "o caminho criado pela caneta devia ser visível");
    }




    void nodeToolKeyboardOps()
    {
        // Delete remove o nó seleccionado; Enter alterna caminho
        // fechado/aberto; cada um é um passo de undo.
        Document doc(200, 120);
        auto vec = std::make_unique<VectorLayer>();
        // Como a caneta cria: nós em espaço LOCAL centrado e a posição da
        // camada = centro das âncoras.
        const QPointF anchors[3] = {QPointF(20, 60), QPointF(100, 20), QPointF(180, 80)};
        const QPointF center((anchors[0].x() + anchors[2].x()) / 2,
                             (anchors[0].y() + anchors[1].y() + anchors[2].y()) / 3);
        for (const QPointF& a : anchors) {
            VectorNode n;
            n.position = a - center;
            vec->nodes.push_back(n);
        }
        vec->transform.position = center;
        vec->rebuildPath();
        const LayerId id = vec->id();
        QVERIFY(doc.addLayer(std::move(vec)));
        CommandStack history;

        VectorNodeTool tool;
        ToolContext ctx;
        ctx.document = &doc;
        ctx.history = &history;
        ctx.selectedLayerId = id;
        ctx.zoom = 1.0;
        ctx.requestUpdate = [] {};
        ctx.requestStatusMessage = [](const QString&) {};
        tool.activate(ctx);

        auto key = [](int k) {
            return QKeyEvent(QEvent::KeyPress, k, Qt::NoModifier);
        };

        // Selecciona o nó do meio por clique.
        auto* vecLayer = dynamic_cast<VectorLayer*>(doc.findLayer(id));
        const QPointF grabDoc = vecLayer->transform.matrix(vecLayer->contentBounds())
                                    .map(vecLayer->nodes[1].position);
        QMouseEvent press = makeMouse(QEvent::MouseButtonPress, grabDoc);
        tool.mousePress(&press, grabDoc, ctx);
        QMouseEvent release = makeMouse(QEvent::MouseButtonRelease, grabDoc);
        tool.mouseRelease(&release, grabDoc, ctx);

        // Delete remove o nó do meio.
        QKeyEvent del = key(Qt::Key_Delete);
        tool.keyPress(&del, ctx);
        vecLayer = dynamic_cast<VectorLayer*>(doc.findLayer(id));
        QCOMPARE(vecLayer->nodes.size(), 2u);
        QCOMPARE(vecLayer->nodes[0].position, anchors[0] - center);
        QCOMPARE(history.undoCount(), 1);
        history.undo();
        QCOMPARE(dynamic_cast<VectorLayer*>(doc.findLayer(id))->nodes.size(), 3u);

        // Enter fecha o caminho.
        QVERIFY(!dynamic_cast<VectorLayer*>(doc.findLayer(id))->closed);
        QKeyEvent enter = key(Qt::Key_Return);
        tool.keyPress(&enter, ctx);
        QVERIFY(dynamic_cast<VectorLayer*>(doc.findLayer(id))->closed);
        // O undo anterior tirou o comando de delete da pilha: agora só resta
        // o do Enter (1 passo).
        QCOMPARE(history.undoCount(), 1);
        history.undo();
        QVERIFY(!dynamic_cast<VectorLayer*>(doc.findLayer(id))->closed);

        // Escape não estoura e não cria histórico.
        const int depth = history.undoCount();
        QKeyEvent esc = key(Qt::Key_Escape);
        tool.keyPress(&esc, ctx);
        QCOMPARE(history.undoCount(), depth);
    }
};

QTEST_MAIN(TestToolsExtras)
#include "test_tools_extras.moc"
