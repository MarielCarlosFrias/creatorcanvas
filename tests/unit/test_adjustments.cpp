#include "core/Document.h"
#include "core/assets/AssetStore.h"
#include "core/commands/AdjustmentLayerCommands.h"
#include "core/history/CommandStack.h"
#include "core/image/AdjustmentEngine.h"
#include "core/image/ImageProcessing.h"
#include "core/image/ImageHistogram.h"
#include "core/layers/Layer.h"

#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using namespace cc;

class TestAdjustments final : public QObject
{
    Q_OBJECT

private slots:
    void engineAppliesLevelsAndCurves()
    {
        QImage img(8, 8, QImage::Format_ARGB32);
        img.fill(qRgba(128, 128, 128, 255));

        // Identidade: devolve a própria imagem (sem custo de cópia).
        const AdjustmentParams identity = encodeLevels({}, {}, {}, {});
        const QImage same = applyAdjustmentParameters(img, AdjustmentType::Levels, identity);
        QVERIFY(same.isNull() || same == img);

        // Níveis a escurecer (ponto preto a 0.5) + curva invertida escurecem mais.
        const AdjustmentParams levels = encodeLevels(
            LevelsChannel{0.5f, 1.0f, 1.0f, 0.0f, 1.0f}, {}, {}, {});
        const QImage afterLevels = applyAdjustmentParameters(img, AdjustmentType::Levels, levels);
        QVERIFY(!afterLevels.isNull());
        const int levelsPixel = qRed(afterLevels.pixel(4, 4));
        QVERIFY2(levelsPixel < 128, qPrintable(QStringLiteral("níveis deviam escurecer: %1")
                                                   .arg(levelsPixel)));

        // Curva que mente para baixo (saída máxima 0.1) só pode escurecer
        // mais — ou empatar quando ambos já bateram no piso.
        const AdjustmentParams curves = encodeCurves(
            {CurvePoint{0.0f, 0.0f}, CurvePoint{1.0f, 0.1f}}, {}, {}, {});
        // Encadeado, como duas camadas de ajuste na pilha do documento.
        const QImage afterCurves = applyAdjustmentParameters(afterLevels, AdjustmentType::Curves, curves);
        const int curvesPixel = qRed(afterCurves.pixel(4, 4));
        QVERIFY2(curvesPixel <= levelsPixel,
                 qPrintable(QStringLiteral("curva devia escurecer mais ou igualar: %1 -> %2")
                                .arg(levelsPixel).arg(curvesPixel)));

        // Força 0 = original; força 1 = ajusto total.
        const QImage none = applyAdjustmentParameters(img, AdjustmentType::Levels, levels, 0.0f);
        QCOMPARE(qRed(none.pixel(4, 4)), 128);
    }

    void roundTripOfParams()
    {
        // O que o UI codifica, o motor relê de forma equivalente.
        const auto curve = std::vector<CurvePoint>{
            CurvePoint{0.0f, 0.0f}, CurvePoint{0.25f, 0.6f}, CurvePoint{1.0f, 1.0f}};
        const AdjustmentParams params = encodeCurves(curve, curve, curve, curve);
        const std::vector<CurvePoint> back = curveFromParams(params, QStringLiteral("curves.master"));
        QCOMPARE(back.size(), curve.size());
        for (size_t i = 0; i < curve.size(); ++i) {
            QCOMPARE(back[i].input, curve[i].input);
            QCOMPARE(back[i].output, curve[i].output);
        }

        // Parâmetros corrompidos não estouram: caem para identidade.
        const AdjustmentParams broken = {
            {QStringLiteral("curves.master.count"), 2.0},
            {QStringLiteral("curves.master.0.in"), 0.0},
            {QStringLiteral("curves.master.0.out"), 5.0}, // fora de 0..1
            {QStringLiteral("curves.master.1.in"), 1.0},
            {QStringLiteral("curves.master.1.out"), 1.0},
        };
        QVERIFY(curveFromParams(broken, QStringLiteral("curves.master")).empty());
    }

    void nonDestructiveApplyIsUndoable()
    {
        Document doc(32, 24);
        QImage img(32, 24, QImage::Format_ARGB32);
        img.fill(qRgba(100, 120, 140, 255));
        const LayerId asset = doc.assets().addImage(img, QStringLiteral("png"));
        auto layer = std::make_unique<ImageLayer>();
        layer->assetId = asset;
        layer->naturalWidth = 32;
        layer->naturalHeight = 24;
        const LayerId imageId = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        CommandStack history;
        const AdjustmentParams params = {
            {QStringLiteral("brightness"), 40.0},
            {QStringLiteral("contrast"), 10.0},
        };
        QVERIFY(executeApplyAdjustment(doc, &history, imageId,
                                       AdjustmentType::BrightnessContrast, params, true));

        // Camada de ajuste criada, ligada e com parâmetros.
        const auto* imgLayer = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        QVERIFY(imgLayer);
        QVERIFY(imgLayer->adjustmentLayerId.has_value());
        const AdjustmentLayer* adj = doc.findAdjustmentLayer(*imgLayer->adjustmentLayerId);
        QVERIFY(adj);
        QCOMPARE(adj->kind, AdjustmentType::BrightnessContrast);
        QCOMPARE(adj->parameters.size(), params.size());

        // O asset original ficou intacto (nada foi cozido).
        QCOMPARE(imgLayer->assetId, asset);

        // Undo remove a camada e o vínculo.
        QVERIFY(history.canUndo());
        history.undo();
        const auto* restored = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        QVERIFY(restored);
        QVERIFY(!restored->adjustmentLayerId.has_value());
        history.redo();
        const auto* redone = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        QVERIFY(redone && redone->adjustmentLayerId.has_value());
    }

    void reusingLinkedLayerIsOneUndoStep()
    {
        Document doc(32, 24);
        QImage img(32, 24, QImage::Format_ARGB32);
        img.fill(qRgba(100, 120, 140, 255));
        const LayerId asset = doc.assets().addImage(img, QStringLiteral("png"));
        auto layer = std::make_unique<ImageLayer>();
        layer->assetId = asset;
        layer->naturalWidth = 32;
        layer->naturalHeight = 24;
        const LayerId imageId = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        CommandStack history;
        executeApplyAdjustment(doc, &history, imageId, AdjustmentType::Levels,
                               encodeLevels(LevelsChannel{0.0f, 0.5f, 1.0f, 0.0f, 1.0f}, {}, {}, {}),
                               true);
        const int depthAfterFirst = history.undoCount();
        // Segunda aplicação reutiliza a camada ligada em UM passo.
        executeApplyAdjustment(doc, &history, imageId, AdjustmentType::Curves,
                               encodeCurves({CurvePoint{0.0f, 0.0f}, CurvePoint{1.0f, 0.2f}}, {}, {}, {}),
                               true);
        QCOMPARE(history.undoCount(), depthAfterFirst + 1);

        const auto* imgLayer = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        const AdjustmentLayer* adj = doc.findAdjustmentLayer(*imgLayer->adjustmentLayerId);
        QCOMPARE(adj->kind, AdjustmentType::Curves);
    }

    void destructiveApplyBakesNewAsset()
    {
        Document doc(32, 24);
        QImage img(32, 24, QImage::Format_ARGB32);
        img.fill(qRgba(100, 120, 140, 255));
        const LayerId asset = doc.assets().addImage(img, QStringLiteral("png"));
        auto layer = std::make_unique<ImageLayer>();
        layer->assetId = asset;
        layer->naturalWidth = 32;
        layer->naturalHeight = 24;
        const LayerId imageId = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        CommandStack history;
        QVERIFY(executeApplyAdjustment(
            doc, &history, imageId, AdjustmentType::BrightnessContrast,
            {{QStringLiteral("brightness"), 40.0}, {QStringLiteral("contrast"), 10.0}}, false));

        const auto* imgLayer = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        QVERIFY(imgLayer->assetId != asset); // asset novo
        QVERIFY(!imgLayer->adjustmentLayerId.has_value()); // sem camada de ajuste

        history.undo();
        const auto* restored = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        QCOMPARE(restored->assetId, asset);
    }

    void rejectsNonImageLayers()
    {
        Document doc(32, 24);
        auto text = std::make_unique<TextLayer>();
        const LayerId textId = text->id();
        QVERIFY(doc.addLayer(std::move(text)));
        CommandStack history;
        QVERIFY(!executeApplyAdjustment(doc, &history, textId,
                                        AdjustmentType::Levels, {}, true));
        QVERIFY(history.canUndo() == false);
    }

    void newAdjustmentTypesChangePixels()
    {
        // Imagem escura e imagem clara: os pesos do motor dependem da
        // luminância (shadowWeight = 1 - 2L, highlightWeight = 2L - 0.5).
        QImage dark(48, 48, QImage::Format_ARGB32);
        dark.fill(qRgba(40, 40, 40, 255));
        QImage bright(48, 48, QImage::Format_ARGB32);
        bright.fill(qRgba(210, 210, 210, 255));

        // Sombras a +40 clareiam os pixéis escuros e não tocam nos claros.
        const QImage shadowsOnDark = applyAdjustmentParameters(
            dark, AdjustmentType::ShadowsHighlights,
            {{QStringLiteral("shadows"), 40.0}, {QStringLiteral("highlights"), 0.0}});
        QVERIFY2(qRed(shadowsOnDark.pixel(24, 24)) > 40,
                 qPrintable(QStringLiteral("sombras deviam clarear o escuro: %1")
                            .arg(qRed(shadowsOnDark.pixel(24, 24)))));

        const QImage shadowsOnBright = applyAdjustmentParameters(
            bright, AdjustmentType::ShadowsHighlights,
            {{QStringLiteral("shadows"), 40.0}, {QStringLiteral("highlights"), 0.0}});
        QCOMPARE(qRed(shadowsOnBright.pixel(24, 24)), 210);

        // Realces a +40 clareiam os pixéis claros (até ao tecto).
        const QImage highlightsOnBright = applyAdjustmentParameters(
            bright, AdjustmentType::ShadowsHighlights,
            {{QStringLiteral("shadows"), 0.0}, {QStringLiteral("highlights"), 40.0}});
        QVERIFY2(qRed(highlightsOnBright.pixel(24, 24)) > 210,
                 qPrintable(QStringLiteral("realces deviam clarear o claro: %1")
                            .arg(qRed(highlightsOnBright.pixel(24, 24)))));

        // Vinheta: positiva escurece as bordas e mantém o centro.
        const QImage vig = applyAdjustmentParameters(
            bright, AdjustmentType::Vignette, {{QStringLiteral("vignette"), 100.0}});
        const int center = qRed(vig.pixel(24, 24));
        const int corner = qRed(vig.pixel(1, 1));
        QVERIFY2(corner < center,
                 qPrintable(QStringLiteral("vinheta devia escurecer o canto: %1 vs %2")
                            .arg(corner).arg(center)));

        // Channel Mixer: ganho 2 no verde duplica o canal.
        QImage tinted(48, 48, QImage::Format_ARGB32);
        tinted.fill(qRgba(120, 130, 140, 255));
        const QImage mixed = applyAdjustmentParameters(
            tinted, AdjustmentType::ChannelMixer,
            {{QStringLiteral("redGain"), 1.0},
             {QStringLiteral("greenGain"), 2.0},
             {QStringLiteral("blueGain"), 1.0}});
        QCOMPARE(qGreen(mixed.pixel(24, 24)), 255);
        QCOMPARE(qRed(mixed.pixel(24, 24)), qRed(tinted.pixel(24, 24)));
    }


    void histogramCountsChannelsAndIgnoresTransparent()
    {
        QImage img(4, 2, QImage::Format_ARGB32);
        img.fill(qRgba(0, 0, 0, 0));                       // tudo transparente
        img.setPixel(0, 0, qRgba(255, 0, 0, 255));         // vermelho puro
        img.setPixel(1, 0, qRgba(255, 0, 0, 255));         // vermelho puro
        img.setPixel(2, 0, qRgba(0, 255, 0, 255));         // verde puro
        img.setPixel(3, 0, qRgba(0, 0, 255, 128));         // azul semi

        const ImageHistogram h = computeHistogram(img);
        // Os 4 pixéis transparentes NÃO contam: só os 4 opacos existem.
        QCOMPARE(h.luminance[0], 0u);          // nenhum opaco é preto puro
        QCOMPARE(h.red[0], 2u);                // verde e azul têm canal R a 0
        QCOMPARE(h.green[0], 3u);              // vermelho, vermelho e azul

        // Imagem toda transparente: histograma a zeros.
        QImage nothing(4, 2, QImage::Format_ARGB32);
        nothing.fill(qRgba(0, 0, 0, 0));
        QCOMPARE(computeHistogram(nothing).maxCount(), 0u);

        // Dois vermelhos, um verde, um azul.
        QCOMPARE(h.red[255], 2u);
        QCOMPARE(h.green[255], 1u);
        QCOMPARE(h.blue[255], 1u);   // o semi-transparente conta (alfa > 0)

        // Luminância Rec.601: vermelho ~76, verde ~150, azul ~29.
        QVERIFY(h.luminance[76] >= 2u);
        QVERIFY(h.luminance[150] >= 1u);
        QVERIFY(h.luminance[29] >= 1u);

        // Pico para normalização do desenho: o verde a 0 aparece 3 vezes
        // (vermelelho, vermelho e azul), mais que o vermelho a 255.
        QCOMPARE(h.maxCount(), 3u);

        // Imagem nula não estoura.
        const ImageHistogram empty = computeHistogram(QImage());
        QCOMPARE(empty.maxCount(), 0u);
    }

};

QTEST_GUILESS_MAIN(TestAdjustments)
#include "test_adjustments.moc"
