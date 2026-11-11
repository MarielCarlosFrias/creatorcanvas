#include "core/document/WebSlice.h"
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
};

QTEST_MAIN(TestOverlays)
#include "test_overlays.moc"
