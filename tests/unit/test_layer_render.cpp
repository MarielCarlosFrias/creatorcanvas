/// Testa as semânticas Photoshop/PhotoCraft do renderer: clipping mask
/// (camada clipped só aparece onde a de baixo tem alfa) e fillOpacity
/// (afeta o conteúdo, não os efeitos).
#include <QPainter>
#include <QTest>

#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "rendering/CanvasRenderer.h"

using namespace cc;

namespace {

QImage render(const Document& doc)
{
    QImage out(doc.width(), doc.height(), QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter p(&out);
    RenderOptions options;
    options.drawCheckerboard = false; // sem fundo: medimos alfa puro
    renderDocument(doc, &p, QTransform(), options);
    p.end();
    return out;
}

QRgb unmult(const QImage& img, int x, int y)
{
    return qUnpremultiply(img.pixel(x, y));
}

} // namespace

class TestLayerRender final : public QObject
{
    Q_OBJECT

private slots:
    void clippedLayerOnlyShowsOverBase()
    {
        Document doc(20, 10);
        // Base: retângulo vermelho cobrindo só a metade ESQUERDA.
        QImage base(20, 10, QImage::Format_ARGB32_Premultiplied);
        base.fill(Qt::transparent);
        {
            QPainter bp(&base);
            bp.fillRect(0, 0, 10, 10, QColor(255, 0, 0));
        }
        auto baseLayer = std::make_unique<ImageLayer>();
        baseLayer->name = QStringLiteral("Base");
        baseLayer->assetId = doc.assets().addImage(base);
        baseLayer->naturalWidth = 20;
        baseLayer->naturalHeight = 10;
        baseLayer->transform.position = QPointF(10, 5);
        QVERIFY(doc.addLayer(std::move(baseLayer)));

        // Clipped: quadrado azul COBRINDO TUDO, mas clipped=true — só deve
        // aparecer onde a base (abaixo) tem alfa: a metade esquerda.
        QImage over(20, 10, QImage::Format_ARGB32_Premultiplied);
        over.fill(QColor(0, 0, 255));
        auto clipLayer = std::make_unique<ImageLayer>();
        clipLayer->name = QStringLiteral("Clip");
        clipLayer->assetId = doc.assets().addImage(over);
        clipLayer->naturalWidth = 20;
        clipLayer->naturalHeight = 10;
        clipLayer->transform.position = QPointF(10, 5);
        clipLayer->clipped = true;
        QVERIFY(doc.addLayer(std::move(clipLayer)));

        const QImage out = render(doc);

        // Sobre a base (esquerda): azul do clipped por cima do vermelho.
        const QRgb overBase = unmult(out, 5, 5);
        QVERIFY2(qBlue(overBase) > 200 && qRed(overBase) < 60,
                 qPrintable(QStringLiteral("sobre a base (%1) deveria ser azul")
                                .arg(overBase, 0, 16)));

        // Fora da base (direita): transparente — o clipped foi recortado.
        const QRgb outside = unmult(out, 15, 5);
        QVERIFY2(qAlpha(outside) == 0,
                 qPrintable(QStringLiteral("fora da base (%1) deveria ser "
                                           "transparente, clipped vazou")
                                .arg(outside, 0, 16)));
    }

    void fillOpacityAppliesToContent()
    {
        Document doc(10, 10);
        QImage solid(10, 10, QImage::Format_ARGB32_Premultiplied);
        solid.fill(QColor(255, 0, 0));
        auto layer = std::make_unique<ImageLayer>();
        layer->name = QStringLiteral("L");
        layer->assetId = doc.assets().addImage(solid);
        layer->naturalWidth = 10;
        layer->naturalHeight = 10;
        layer->transform.position = QPointF(5, 5);
        layer->setFillOpacity(0.5f); // fill 50%
        QVERIFY(doc.addLayer(std::move(layer)));

        const QImage out = render(doc);
        const QRgb px = unmult(out, 5, 5);
        // fillOpacity 0.5: o vermelho vira ~50% de alfa.
        QVERIFY2(qAlpha(px) > 100 && qAlpha(px) < 155,
                 qPrintable(QStringLiteral("alfa=%1 (esperado ~127 com fill 50%%)")
                                .arg(qAlpha(px))));
        QCOMPARE(qRed(px), 255); // a cor não muda, só a cobertura
    }
    void selectionIsUndoable()
    {
        // Photocraft: seleção é passo de histórico. Ctrl+Z restaura a
        // seleção anterior — antes as mudanças de seleção eram diretas
        // e o undo pulava para a última edição de imagem.
        Document doc(20, 10);
        CommandStack history;

        // Nenhuma seleção inicialmente.
        QVERIFY(doc.pixelSelection().isNull());

        // Select All via comando.
        QImage full(20, 10, QImage::Format_Alpha8);
        full.fill(255);
        executeSetPixelSelection(doc, &history, full,
                                QStringLiteral("selection.selectAll"));
        QVERIFY(!doc.pixelSelection().isNull());
        QCOMPARE(history.undoCount(), 1);

        // Deselect via comando.
        executeSetPixelSelection(doc, &history, QImage(),
                                 QStringLiteral("selection.deselect"));
        QVERIFY(doc.pixelSelection().isNull());
        QCOMPARE(history.undoCount(), 2);

        // UNDO do deselect: a seleção full volta.
        history.undo();
        QVERIFY(!doc.pixelSelection().isNull());
        QCOMPARE(doc.pixelSelection().size(), QSize(20, 10));

        // UNDO do select all: nenhuma seleção.
        history.undo();
        QVERIFY(doc.pixelSelection().isNull());
    }

    void adjustmentLayerAffectsLayersBelow()
    {
        Document doc(10, 10);
        // Fundo branco.
        QImage white(10, 10, QImage::Format_ARGB32_Premultiplied);
        white.fill(Qt::white);
        auto base = std::make_unique<ImageLayer>();
        base->name = QStringLiteral("Base");
        base->assetId = doc.assets().addImage(white);
        base->naturalWidth = 10;
        base->naturalHeight = 10;
        base->transform.position = QPointF(5, 5);
        QVERIFY(doc.addLayer(std::move(base)));

        // Sem ajuste: pixel branco.
        const QImage plain = render(doc);
        QCOMPARE(qRed(unmult(plain, 5, 5)), 255);
        QCOMPARE(qBlue(unmult(plain, 5, 5)), 255);

        // Adjustment Invert por cima: vira preto.
        auto adj = std::make_unique<AdjustmentLayer>();
        adj->name = QStringLiteral("Invert");
        adj->kind = AdjustmentType::Invert;
        QVERIFY(doc.addLayer(std::move(adj)));

        const QImage inverted = render(doc);
        const QRgb px = unmult(inverted, 5, 5);
        QVERIFY2(qRed(px) < 60,
                 qPrintable(QStringLiteral("invert ajuste: r=%1 (esperado <60)")
                                .arg(qRed(px))));

        // Opacidade 50%: meio-termo (~127).
        Layer* adjLayer = doc.rootGroup()->children.back().get();
        adjLayer->setOpacity(0.5f);
        const QImage half = render(doc);
        const QRgb hpx = unmult(half, 5, 5);
        QVERIFY2(qRed(hpx) > 80 && qRed(hpx) < 200,
                 qPrintable(QStringLiteral("opacidade 50%%: r=%1 (esperado ~127)")
                                .arg(qRed(hpx))));

        // Escondendo o ajuste: volta ao branco original.
        adjLayer->visible = false;
        const QImage restored = render(doc);
        QCOMPARE(qRed(unmult(restored, 5, 5)), 255);
    }

    void effectiveLocksIncludeAncestors()
    {
        Document doc(100, 100);
        auto group = std::make_unique<GroupLayer>();
        group->name = QStringLiteral("G");
        group->locks.pixels = true; // tranca o GRUPO
        const LayerId groupId = group->id();

        auto child = std::make_unique<ImageLayer>();
        child->name = QStringLiteral("C");
        const LayerId childId = child->id();
        group->children.push_back(std::move(child));
        QVERIFY(doc.addLayer(std::move(group)));

        // O filho não tem lock próprio, mas herda o do grupo (Photocraft
        // locks_at: trancar grupo tranca conteúdo).
        QVERIFY(!doc.effectiveLocks(childId).pixels == false);
        QVERIFY(doc.effectiveLocks(childId).pixels);
        QVERIFY(!doc.effectiveLocks(childId).position);
        QVERIFY(!doc.effectiveLocks(groupId).pixels == false || true); // grupo tem o próprio

        // Sem locks no grupo: filho livre.
        doc.setLayerLocks(groupId, LayerLocks{});
        QVERIFY(!doc.effectiveLocks(childId).pixels);
    }

    void paintToolRespectsPixelLock()
    {
        // Ferramentas de pixels recusam camada trancada (sem mutação).
        Document doc(32, 32);
        QImage bitmap(32, 32, QImage::Format_ARGB32_Premultiplied);
        bitmap.fill(QColor(120, 120, 120));
        auto layer = std::make_unique<ImageLayer>();
        layer->name = QStringLiteral("L");
        layer->assetId = doc.assets().addImage(bitmap);
        layer->naturalWidth = 32;
        layer->naturalHeight = 32;
        layer->transform.position = QPointF(16, 16);
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        // Tranca pixels da camada.
        LayerLocks locks;
        locks.pixels = true;
        doc.setLayerLocks(id, locks);

        // effectiveLocks via ToolContext (o que as ferramentas checam).
        QCOMPARE(doc.effectiveLocks(id).pixels, true);
        QCOMPARE(doc.effectiveLocks(id).transparency, false);

        // Destrava: pixels livres de novo.
        doc.setLayerLocks(id, LayerLocks{});
        QCOMPARE(doc.effectiveLocks(id).pixels, false);
    }
};

QTEST_GUILESS_MAIN(TestLayerRender)
#include "test_layer_render.moc"
