#include "core/selection/SelectionEngine.h"

#include <QColor>
#include <QElapsedTimer>
#include <QtTest>

using namespace cc;

namespace {

/// Imagem de teste: quadrado vermelho sobre fundo branco.
QImage makeTestImage(int w, int h, const QRect& redRect)
{
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(Qt::white);
    // QRect::right()/bottom() são inclusivos; itera até <= para cobrir tudo.
    for (int y = redRect.top(); y <= redRect.bottom(); ++y)
        for (int x = redRect.left(); x <= redRect.right(); ++x)
            img.setPixel(x, y, qRgba(255, 0, 0, 255));
    return img;
}

/// Quantidade de pixels com cobertura > 0.
int selectedCount(const QImage& mask)
{
    if (mask.isNull())
        return 0;
    const QImage a = SelectionEngine::normalize(mask);
    int count = 0;
    for (int y = 0; y < a.height(); ++y) {
        const uchar* row = a.constScanLine(y);
        for (int x = 0; x < a.width(); ++x)
            if (row[x] > 0)
                ++count;
    }
    return count;
}

/// Cobre um quadrado central (para expand/contract/border).
/// Usa scanLine diretamente: setPixel() em Format_Alpha8 trata o valor
/// como índice de paleta, não como byte de cobertura.
QImage squareSelection(int w, int h, int x0, int y0, int x1, int y1)
{
    QImage mask(w, h, QImage::Format_Alpha8);
    mask.fill(0);
    for (int y = y0; y < y1; ++y) {
        uchar* row = mask.scanLine(y);
        for (int x = x0; x < x1; ++x)
            row[x] = 255;
    }
    return mask;
}

} // namespace

class TestSelectionEngine final : public QObject
{
    Q_OBJECT

private slots:
    void magicWandContiguousVsGlobal()
    {
        // Dois quadrados vermelhos separados em fundo branco.
        QImage img = makeTestImage(20, 10, QRect(2, 2, 4, 4));
        for (int y = 2; y < 6; ++y)
            for (int x = 12; x < 16; ++x)
                img.setPixel(x, y, qRgba(255, 0, 0, 255));

        const QRect area(0, 0, 20, 10);
        const auto contiguous = SelectionEngine::magicWand(
            img, area, 3, 3, 10.0f, true, false);
        const auto global = SelectionEngine::magicWand(
            img, area, 3, 3, 10.0f, false, false);

        QCOMPARE(selectedCount(contiguous), 16); // 4x4 quadrado do seed
        QCOMPARE(selectedCount(global), 32);    // ambos os quadrados vermelhos

        // Seed fora da área -> seleção vazia.
        const auto none = SelectionEngine::magicWand(
            img, area, 50, 3, 10.0f, true, false);
        QVERIFY(none.isNull() || selectedCount(none) == 0);
    }

    void magicWandAntiAlias()
    {
        QImage img = makeTestImage(20, 10, QRect(2, 2, 10, 5));
        const QRect area(0, 0, 20, 10);
        const auto aa = SelectionEngine::magicWand(
            img, area, 5, 5, 10.0f, true, true);
        // Anti-aliasing cria pixels com cobertura intermediária na borda.
        bool hasPartial = false;
        const QImage m = SelectionEngine::normalize(aa);
        for (int y = 0; y < m.height() && !hasPartial; ++y) {
            const uchar* row = m.constScanLine(y);
            for (int x = 0; x < m.width(); ++x) {
                if (row[x] > 0 && row[x] < 255) {
                    hasPartial = true;
                    break;
                }
            }
        }
        QVERIFY2(hasPartial, "anti-aliasing deve gerar cobertura parcial");
    }

    void colorRangeFalloff()
    {
        // 4 pixels: vermelho puro, vermelho levemente alterado, azul, transparente.
        QImage img(4, 1, QImage::Format_ARGB32);
        img.setPixel(0, 0, qRgba(255, 0, 0, 255));
        img.setPixel(1, 0, qRgba(230, 0, 0, 255)); // 25 de distância
        img.setPixel(2, 0, qRgba(0, 0, 255, 255)); // azul: fora
        img.setPixel(3, 0, qRgba(255, 0, 0, 0));   // transparente: fora

        const QRect area(0, 0, 4, 1);
        const auto mask = SelectionEngine::colorRange(
            img, area, QColor(255, 0, 0), 40.0f);
        const QImage m = SelectionEngine::normalize(mask);

        QCOMPARE(m.constScanLine(0)[0], uchar(255)); // vermelho puro: total
        QVERIFY2(m.constScanLine(0)[1] > 0 && m.constScanLine(0)[1] < 255,
                 "pixel próximo deve ter cobertura parcial");
        QCOMPARE(m.constScanLine(0)[2], uchar(0));    // azul: nada
        QCOMPARE(m.constScanLine(0)[3], uchar(0));    // transparente: nada
    }

    void tonalRanges()
    {
        // Escala de cinza: 0, 60, 128, 200, 255.
        QImage img(5, 1, QImage::Format_ARGB32);
        const int values[] = {0, 60, 128, 200, 255};
        for (int i = 0; i < 5; ++i) {
            const int v = values[i];
            img.setPixel(i, 0, qRgba(v, v, v, 255));
        }
        const QRect area(0, 0, 5, 1);

        // Sombras até 65 com feather 20: 0 e 60 selecionados.
        const auto shadows = SelectionEngine::tonalShadows(img, area, 65.0f, 20.0f);
        const QImage sh = SelectionEngine::normalize(shadows);
        QVERIFY(sh.constScanLine(0)[0] == 255);
        QVERIFY(sh.constScanLine(0)[1] == 255);
        QVERIFY(sh.constScanLine(0)[2] == 0);
        QVERIFY(sh.constScanLine(0)[4] == 0);

        // Altas luzes a partir de 190 com feather 5: 200 e 255.
        const auto highlights = SelectionEngine::tonalHighlights(img, area, 190.0f, 5.0f);
        const QImage hi = SelectionEngine::normalize(highlights);
        QVERIFY(hi.constScanLine(0)[0] == 0);
        QVERIFY(hi.constScanLine(0)[3] == 255);
        QVERIFY(hi.constScanLine(0)[4] == 255);

        // Meios-tons 105..150: apenas 128.
        const auto midtones = SelectionEngine::tonalMidtones(img, area, 105.0f, 150.0f, 20.0f);
        const QImage mid = SelectionEngine::normalize(midtones);
        QVERIFY(mid.constScanLine(0)[2] == 255);
        QVERIFY(mid.constScanLine(0)[0] == 0);
        QVERIFY(mid.constScanLine(0)[4] == 0);
    }

    void expandContractBorder()
    {
        // Quadrado 10x10 em máscara 20x20.
        const auto square = squareSelection(20, 20, 5, 5, 15, 15);

        const auto expanded = SelectionEngine::expand(square, 2.0f);
        const QImage e = SelectionEngine::normalize(expanded);
        // Pixel a 2 do quadrado entra; a 3 não.
        QCOMPARE(e.constScanLine(10)[3], uchar(255));
        QCOMPARE(e.constScanLine(10)[2], uchar(0));

        const auto contracted = SelectionEngine::contract(square, 2.0f);
        const QImage c = SelectionEngine::normalize(contracted);
        // Pixel a 2 da borda interna sai; a 3 fica.
        QCOMPARE(c.constScanLine(10)[6], uchar(0));
        QCOMPARE(c.constScanLine(10)[7], uchar(255));

        const auto bordered = SelectionEngine::border(square, 4.0f);
        const QImage b = SelectionEngine::normalize(bordered);
        // Centro não faz parte da borda.
        QVERIFY(b.constScanLine(10)[10] == 0);
        // Borda interna/externa fazem.
        QVERIFY(b.constScanLine(10)[5] > 128);
    }

    void smoothAndFeather()
    {
        // Quadrado com um pixel isolado (speck).
        auto square = squareSelection(20, 20, 5, 5, 15, 15);
        square.scanLine(2)[2] = 255;

        const auto smoothed = SelectionEngine::smooth(square, 1.0f);
        const QImage s = SelectionEngine::normalize(smoothed);
        // Speck removido, quadrado mantido.
        QCOMPARE(s.constScanLine(2)[2], uchar(0));
        QCOMPARE(s.constScanLine(10)[10], uchar(255));

        const auto feathered = SelectionEngine::feather(squareSelection(20, 20, 5, 5, 15, 15), 4.0f);
        const QImage f = SelectionEngine::normalize(feathered);
        // Borda do feather tem cobertura parcial.
        const uchar edge = f.constScanLine(10)[5];
        QVERIFY2(edge > 76 && edge < 204,
                 qPrintable(QStringLiteral("borda feathered=%1 (esperado 76..204)")
                                .arg(edge)));
        // Centro permanece majoritariamente sólido (gaussiana espalha
        // cobertura, mas o miolo de um quadrado 10x10 com sigma 1 fica alto).
        QVERIFY(f.constScanLine(10)[10] > 150);
    }

    void combineModes()
    {
        const QImage a = squareSelection(4, 1, 0, 0, 2, 1); // [1 1 0 0]
        const QImage b = squareSelection(4, 1, 1, 0, 3, 1); // [0 1 1 0]
        const QRect area(0, 0, 4, 1);

        const auto replace = SelectionEngine::combine(a, b, area, SelectionMode::Replace);
        const QImage r = SelectionEngine::normalize(replace);
        QCOMPARE(r.constScanLine(0)[0], uchar(0));
        QCOMPARE(r.constScanLine(0)[1], uchar(255));
        QCOMPARE(r.constScanLine(0)[2], uchar(255));

        const auto add = SelectionEngine::combine(a, b, area, SelectionMode::Add);
        const QImage ad = SelectionEngine::normalize(add);
        QCOMPARE(ad.constScanLine(0)[0], uchar(255));
        QCOMPARE(ad.constScanLine(0)[1], uchar(255));
        QCOMPARE(ad.constScanLine(0)[2], uchar(255));

        const auto subtract = SelectionEngine::combine(a, b, area, SelectionMode::Subtract);
        const QImage su = SelectionEngine::normalize(subtract);
        QCOMPARE(su.constScanLine(0)[0], uchar(255));
        QCOMPARE(su.constScanLine(0)[1], uchar(0));
        QCOMPARE(su.constScanLine(0)[2], uchar(0));

        const auto intersect = SelectionEngine::combine(a, b, area, SelectionMode::Intersect);
        const QImage in = SelectionEngine::normalize(intersect);
        QCOMPARE(in.constScanLine(0)[0], uchar(0));
        QCOMPARE(in.constScanLine(0)[1], uchar(255));
        QCOMPARE(in.constScanLine(0)[2], uchar(0));
    }

    void polygonAntiAlias()
    {
        const QRect area(0, 0, 20, 20);

        // Quadrado 10x10.
        const std::vector<QPointF> square = {
            {2.0, 2.0}, {12.0, 2.0}, {12.0, 12.0}, {2.0, 12.0}};
        const auto sq = SelectionEngine::polygon(square, area, true);
        const QImage s = SelectionEngine::normalize(sq);
        // Área aproximada do quadrado (100 px, com AA até ±10%).
        const int count = selectedCount(s);
        QVERIFY2(count > 80 && count < 130,
                 qPrintable(QStringLiteral("area quadrado=%1 (esperado ~100)")
                                .arg(count)));

        // Triângulo com AA deve ter pixels parciais.
        const std::vector<QPointF> tri = {
            {0.0, 0.0}, {10.0, 0.0}, {0.0, 10.0}};
        const auto triangle = SelectionEngine::polygon(tri, area, true);
        const QImage t = SelectionEngine::normalize(triangle);
        bool hasPartial = false;
        for (int y = 0; y < t.height() && !hasPartial; ++y) {
            const uchar* row = t.constScanLine(y);
            for (int x = 0; x < t.width(); ++x)
                if (row[x] > 0 && row[x] < 255) { hasPartial = true; break; }
        }
        QVERIFY2(hasPartial, "triângulo AA deve ter cobertura parcial");

        // Sem AA: apenas valores 0 ou 255.
        const auto hard = SelectionEngine::polygon(tri, area, false);
        const QImage h = SelectionEngine::normalize(hard);
        for (int y = 0; y < h.height(); ++y) {
            const uchar* row = h.constScanLine(y);
            for (int x = 0; x < h.width(); ++x)
                QVERIFY(row[x] == 0 || row[x] == 255);
        }

        // Menos de 3 pontos -> máscara vazia.
        QVERIFY(SelectionEngine::polygon({{0, 0}, {1, 1}}, area, true).isNull());
    }

    void helpers()
    {
        // boundingBox e isEmpty
        QVERIFY(SelectionEngine::isEmpty(QImage()));
        QVERIFY(SelectionEngine::isEmpty(squareSelection(10, 10, 0, 0, 0, 0)));

        const auto mask = squareSelection(20, 20, 5, 6, 12, 14);
        QVERIFY(!SelectionEngine::isEmpty(mask));
        const QRect bb = SelectionEngine::boundingBox(mask);
        QCOMPARE(bb, QRect(5, 6, 7, 8));

        // normalize: máscara em outro formato vira Alpha8.
        QImage rgb(2, 2, QImage::Format_ARGB32);
        rgb.fill(Qt::white);
        const QImage norm = SelectionEngine::normalize(rgb);
        QCOMPARE(norm.format(), QImage::Format_Alpha8);
    }

    void morphologyIsFastForLargeRadius()
    {
        // Regressão do congelamento: a versão antiga era O(w·h·r²) e
        // travava o app por minutos (raio 200 ≈ 320 bilhões de operações),
        // fazendo o gerenciador de janelas escurecer a janela ("cinza").
        // Com van Herk separável é O(w·h): deve terminar em < 2 segundos.
        QImage big = squareSelection(1920, 1080, 400, 200, 1200, 800);

        QElapsedTimer timer;
        timer.start();
        const QImage expanded = SelectionEngine::expand(big, 200.0f);
        const qint64 ms = timer.elapsed();

        QVERIFY(!expanded.isNull());
        // Limite generoso: a versão O(r²) antiga levava MINUTOS aqui
        // (320G de operações); o ASan instrumentado adiciona ~3-4x.
        QVERIFY2(ms < 30000,
                 qPrintable(QStringLiteral("expand r=200 levou %1 ms (max 30000)")
                                .arg(ms)));

        // Corretude com raio grande: quadrado x=400..1199 expandido por 200
        // tem borda esquerda em x=200 — dentro é 255, fora é 0.
        const QImage e = SelectionEngine::normalize(expanded);
        QVERIFY(e.constScanLine(500)[201] == uchar(255)); // dentro da nova borda
        QVERIFY(e.constScanLine(500)[199] == uchar(0));   // fora da nova borda
    }
};

QTEST_GUILESS_MAIN(TestSelectionEngine)
#include "test_selection_engine.moc"
