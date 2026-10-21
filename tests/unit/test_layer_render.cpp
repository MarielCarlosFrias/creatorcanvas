/// Testa as semânticas Photoshop/PhotoCraft do renderer: clipping mask
/// (camada clipped só aparece onde a de baixo tem alfa) e fillOpacity
/// (afeta o conteúdo, não os efeitos).
#include <QPainter>
#include <QTest>

#include "core/Document.h"
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
};

QTEST_GUILESS_MAIN(TestLayerRender)
#include "test_layer_render.moc"
