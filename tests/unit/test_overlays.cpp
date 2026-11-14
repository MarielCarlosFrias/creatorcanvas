#include "core/document/WebSlice.h"
#include "core/Document.h"
#include "core/layers/Layer.h"
#include "ui/imageinspector.h"
#include "ui/canvasoverlayrenderer.h"

#include <QImage>
#include <QPainter>
#include <QTest>

using namespace cc;

namespace {

/// Conta pixels não transparentes de uma imagem (marca do que foi desenhado).
int opaquePixels(const QImage& img)
{
    int count = 0;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x)
            if (qAlpha(line[x]) != 0)
                ++count;
    }
    return count;
}

QImage renderWithSlices(const std::vector<WebSlice>& slices, int w = 240, int h = 180)
{
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    QTransform toDevice;
    toDevice.scale(0.5, 0.5);   // documento 480x360 → dispositivo 240x180
    CanvasOverlayRenderer::drawWebSlices(&p, slices, toDevice);
    p.end();
    return img;
}

} // namespace

class TestOverlays final : public QObject
{
    Q_OBJECT

private slots:
    void webSlicesDrawNothingWhenEmpty()
    {
        const QImage empty = renderWithSlices({});
        QCOMPARE(opaquePixels(empty), 0);
    }

    void webSlicesDrawRectAndLabel()
    {
        WebSlice slice;
        slice.name = QStringLiteral("thumb_yt");
        slice.rect = QRectF(0, 0, 480, 270);
        slice.exportFormat = QStringLiteral("png");

        const QImage img = renderWithSlices({slice});
        const int drawn = opaquePixels(img);
        QVERIFY2(drawn > 50, qPrintable(QStringLiteral("esperava contorno+etiqueta desenhados: %1")
                                            .arg(drawn)));

        // A etiqueta fica dentro da vista (não escapa por cima).
        const QImage clipped = renderWithSlices({slice}, 120, 90);
        QVERIFY(opaquePixels(clipped) > 0);

        // Sem fatias nada é desenhado — regressão de "não vem nada".
        const std::vector<WebSlice> none;
        QCOMPARE(opaquePixels(renderWithSlices(none)), 0);
    }

    void webSlicesCycleColorsForMultipleSlices()
    {
        std::vector<WebSlice> slices;
        for (int i = 0; i < 3; ++i) {
            WebSlice s;
            s.name = QStringLiteral("s%1").arg(i);
            s.rect = QRectF(0, 0, 200 + i * 40, 150);
            s.exportFormat = QStringLiteral("png");
            slices.push_back(s);
        }
        const QImage img = renderWithSlices(slices);
        // Contornos de 3 rects aninhados + 3 etiquetas.
        QVERIFY(opaquePixels(img) > 100);

        // Rect inválido é ignorado sem estoirar.
        WebSlice bad;
        bad.name = QStringLiteral("vazio");
        bad.rect = QRectF();
        slices.push_back(bad);
        const QImage withBad = renderWithSlices(slices);
        QVERIFY(withBad.size() == img.size());
    }
    void pixelSelectionOverlayTintsAreaAndAntsEdges()
    {
        using namespace cc;
        Document doc(20, 20);
        QImage img(20, 20, QImage::Format_ARGB32);
        img.fill(qRgba(255, 255, 255, 255));
        const LayerId asset = doc.assets().addImage(img, QStringLiteral("png"));
        auto layer = std::make_unique<ImageLayer>();
        layer->assetId = asset;
        layer->naturalWidth = 20;
        layer->naturalHeight = 20;
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        // Selecção: quadrado 5x5 no canto.
        QImage sel(20, 20, QImage::Format_Alpha8);
        sel.fill(0);
        for (int y = 5; y < 10; ++y)
            for (int x = 5; x < 10; ++x)
                sel.scanLine(y)[x] = 255;
        doc.setPixelSelection(sel);

        // Constrói o overlay (caminho lento, sem cache do caller). O painter
        // tem de ser válido — a função também desenha o resultado.
        QImage target(20, 20, QImage::Format_ARGB32);
        target.fill(Qt::transparent);
        QPainter painter(&target);
        QImage overlay;
        CanvasOverlayRenderer::drawPixelSelectionOutline(
            &painter, static_cast<ImageLayer*>(doc.findLayer(id)), sel, img,
            QTransform(), &overlay);

        QVERIFY(!overlay.isNull());
        QCOMPARE(overlay.size(), QSize(20, 20));

        // Área selecionada: véu accent semi-transparente.
        const QRgb inside = overlay.pixel(7, 7);
        QCOMPARE(qRed(inside), 0x43);
        QCOMPARE(qGreen(inside), 0x78);
        QCOMPARE(qBlue(inside), 0xff);
        QVERIFY(qAlpha(inside) > 0 && qAlpha(inside) < 255);

        // Fora da selecção: transparente.
        QCOMPARE(qAlpha(overlay.pixel(15, 15)), 0);

        // Fronteira: antenas preto/branco opacas.
        bool hasWhite = false, hasBlack = false;
        for (int y = 4; y <= 10; ++y) {
            for (int x = 4; x <= 10; ++x) {
                const QRgb p = overlay.pixel(x, y);
                if (p == qRgba(255, 255, 255, 255)) hasWhite = true;
                if (p == qRgba(0, 0, 0, 255)) hasBlack = true;
            }
        }
        QVERIFY2(hasWhite && hasBlack, "a fronteira devia ter antenas preto/branco");
    }
};



QTEST_MAIN(TestOverlays)
#include "test_overlays.moc"

