#include "core/Document.h"
#include "core/layers/Layer.h"
#include "rendering/CanvasRenderer.h"
#include "rendering/IRenderer.h"
#include "core/layers/Gradient.h"
#include "core/serialization/ProjectFile.h"
#include "core/history/CommandStack.h"
#include "core/commands/AdjustmentLayerCommands.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QtTest>

using namespace cc;

namespace {

void renderTo(QImage& image, const Document& doc, const RenderOptions& options = {})
{
    QPainter painter(&image);
    renderDocument(doc, &painter, QTransform(), options);
}

} // namespace

class TestRenderer final : public QObject
{
    Q_OBJECT

private slots:
    void backgroundFillsCanvas()
    {
        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(200, 30, 30);
        QVERIFY(doc.addLayer(std::move(bg)));

        QImage image(100, 100, QImage::Format_RGB32);
        image.fill(Qt::black);
        renderTo(image, doc);

        QCOMPARE(image.pixelColor(50, 50), QColor(200, 30, 30));
        QCOMPARE(image.pixelColor(2, 2), QColor(200, 30, 30));
    }

    void invisibleAndZeroOpacityLayersAreSkipped()
    {
        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(200, 30, 30);
        QVERIFY(doc.addLayer(std::move(bg)));

        auto hidden = std::make_unique<ShapeLayer>();
        hidden->visible = false;
        hidden->kind = ShapeKind::Rectangle;
        hidden->fill = QColor(0, 255, 0);
        hidden->points = QPolygonF({QPointF(0, 0), QPointF(100, 100)});
        QVERIFY(doc.addLayer(std::move(hidden)));

        auto transparent = std::make_unique<ShapeLayer>();
        transparent->setOpacity(0.0f);
        transparent->kind = ShapeKind::Rectangle;
        transparent->fill = QColor(0, 255, 0);
        transparent->points = QPolygonF({QPointF(0, 0), QPointF(100, 100)});
        QVERIFY(doc.addLayer(std::move(transparent)));

        QImage image(100, 100, QImage::Format_RGB32);
        image.fill(Qt::black);
        renderTo(image, doc);

        QCOMPARE(image.pixelColor(50, 50), QColor(200, 30, 30));
    }

    void shapeRendersWithOpacityBlend()
    {
        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(255, 0, 0);
        QVERIFY(doc.addLayer(std::move(bg)));

        auto shape = std::make_unique<ShapeLayer>();
        shape->kind = ShapeKind::Rectangle;
        shape->fill = QColor(0, 0, 255);
        shape->setOpacity(0.5f);
        shape->points = QPolygonF({QPointF(25, 25), QPointF(75, 75)});
        QVERIFY(doc.addLayer(std::move(shape)));

        QImage image(100, 100, QImage::Format_RGB32);
        image.fill(Qt::black);
        renderTo(image, doc);

         // With the transform system, the rect (points 25..75) renders
        // centered on its transform position (0,0): doc rect (-25..25)^2.
        const QColor inside = image.pixelColor(10, 10);
        QVERIFY(qAbs(inside.red() - 128) <= 1);
        QCOMPARE(inside.green(), 0);
        QVERIFY(qAbs(inside.blue() - 128) <= 1);

        // Outside the rect: pure red.
        QCOMPARE(image.pixelColor(50, 50), QColor(255, 0, 0));

        // Outside the rect: pure red.
        QCOMPARE(image.pixelColor(50, 50), QColor(255, 0, 0));
    }

    void checkerboardShowsThroughTransparentBackground()
    {
        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(0, 0, 0, 0); // fully transparent
        QVERIFY(doc.addLayer(std::move(bg)));

        QImage image(100, 100, QImage::Format_RGB32);
        image.fill(Qt::black);

        RenderOptions options;
        options.checkerSize = 10;
        options.checkerLight = QColor(255, 255, 255);
        options.checkerDark = QColor(0, 0, 0);
        renderTo(image, doc, options);

        QCOMPARE(image.pixelColor(5, 5), QColor(0, 0, 0));
        QCOMPARE(image.pixelColor(15, 5), QColor(255, 255, 255));
    }

    void imagePlaceholderRenders()
    {
        Document doc(100, 100);
        auto image = std::make_unique<ImageLayer>();
        image->naturalWidth = 40;
        image->naturalHeight = 30;
        QVERIFY(doc.addLayer(std::move(image)));

        QImage target(100, 100, QImage::Format_RGB32);
        target.fill(Qt::black);
        RenderOptions options;
        options.drawCheckerboard = false;
        renderTo(target, doc, options);

        QCOMPARE(target.pixelColor(10, 10), QColor(0x3a, 0x3b, 0x40));
    }

    void dropShadowCachingAndClearCaches()
    {
        Document doc(100, 100);
        QImage bitmap(40, 40, QImage::Format_ARGB32_Premultiplied);
        bitmap.fill(Qt::blue);
        const LayerId assetId = doc.assets().addImage(bitmap);

        auto image = std::make_unique<ImageLayer>();
        image->assetId = assetId;
        image->naturalWidth = 40;
        image->naturalHeight = 40;
        image->effects.dropShadow.enabled = true;
        image->effects.dropShadow.distance = 5.0;
        image->effects.dropShadow.blur = 4.0;
        image->effects.dropShadow.color = QColor(Qt::black);
        image->effects.dropShadow.opacity = 0.8;
        const LayerId imageId = image->id();
        QVERIFY(doc.addLayer(std::move(image)));

        QImage target1(100, 100, QImage::Format_ARGB32_Premultiplied);
        target1.fill(Qt::transparent);
        renderTo(target1, doc);

        // Subsequent render uses cached drop shadow and must produce identical output
        QImage target2(100, 100, QImage::Format_ARGB32_Premultiplied);
        target2.fill(Qt::transparent);
        renderTo(target2, doc);

        QCOMPARE(target1, target2);

        // Clearing caches does not break subsequent rendering
        clearRendererCaches();

        QImage target3(100, 100, QImage::Format_ARGB32_Premultiplied);
        target3.fill(Qt::transparent);
        renderTo(target3, doc);

        QCOMPARE(target1, target3);

        // Both invalidation scopes must be safe and preserve the rendered
        // result when the cache is rebuilt on the next frame.
        invalidateLayerCache(imageId);
        invalidateAllLayerCaches();
        QImage target4(100, 100, QImage::Format_ARGB32_Premultiplied);
        target4.fill(Qt::transparent);
        renderTo(target4, doc);
        QCOMPARE(target1, target4);
    }

    void checkerboardCachingAndClippingComposition()
    {
        Document doc(60, 60);
        RenderOptions opts;
        opts.drawCheckerboard = true;
        opts.checkerSize = 8;

        QImage img1(60, 60, QImage::Format_ARGB32_Premultiplied);
        img1.fill(Qt::transparent);
        renderTo(img1, doc, opts);

        // Second render should use cached brush and match pixel-for-pixel
        QImage img2(60, 60, QImage::Format_ARGB32_Premultiplied);
        img2.fill(Qt::transparent);
        renderTo(img2, doc, opts);
        QCOMPARE(img1, img2);

        // Add a base layer and a clipped layer to exercise composite composition with reused temp buffer
        auto base = std::make_unique<ShapeLayer>();
        base->kind = ShapeKind::Rectangle;
        base->fill = Qt::red;
        base->points = QPolygonF({QPointF(10, 10), QPointF(50, 50)});
        base->transform.position = QPointF(30, 30);
        QVERIFY(doc.addLayer(std::move(base)));

        auto clipped = std::make_unique<ShapeLayer>();
        clipped->kind = ShapeKind::Rectangle;
        clipped->fill = Qt::yellow;
        clipped->points = QPolygonF({QPointF(0, 0), QPointF(60, 60)});
        clipped->transform.position = QPointF(30, 30);
        clipped->clipped = true;
        QVERIFY(doc.addLayer(std::move(clipped)));

        QImage compImg(60, 60, QImage::Format_ARGB32_Premultiplied);
        compImg.fill(Qt::transparent);
        renderTo(compImg, doc, opts);

        // Outside base bounds (e.g. 5, 5), clipped layer should NOT show because alpha was 0
        // Inside base bounds (e.g. 30, 30), clipped layer should show (yellow)
        QCOMPARE(compImg.pixelColor(30, 30), QColor(Qt::yellow));

        clearRendererCaches();
    }
    void adjustmentLayerAppliesLevelsAndCurves()
    {
        // Fundo cinza médio + camada de ajuste por cima: o ajuste processa
        // o acumulado abaixo dela.
        Document doc(40, 20);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(128, 128, 128);
        QVERIFY(doc.addLayer(std::move(bg)));

        // Identidade primeira: render não muda.
        QImage base(40, 20, QImage::Format_ARGB32);
        renderTo(base, doc);
        const QColor identity = base.pixelColor(10, 10);
        QCOMPARE(identity, QColor(128, 128, 128));

        // Levels: ponto preto a 0.5 empurra os meios para baixo -> escurece.
        const LayerId levelsId = doc.createAdjustmentLayer(AdjustmentType::Levels);
        QVERIFY(!levelsId.isNull());
        QVERIFY(doc.setAdjustmentParameters(levelsId, {
            {QStringLiteral("levels.master.inBlack"), 0.5},
            {QStringLiteral("levels.master.inWhite"), 1.0},
            {QStringLiteral("levels.master.gamma"), 1.0},
        }));
        QImage withLevels(40, 20, QImage::Format_ARGB32);
        renderTo(withLevels, doc);
        const QColor levelsColor = withLevels.pixelColor(10, 10);
        QVERIFY2(levelsColor.red() < identity.red(),
                 qPrintable(QStringLiteral("levels deveria escurecer: %1")
                            .arg(levelsColor.red())));

        // Curves: S escurecendo o meio (0.5 -> 0.2).
        const LayerId curvesId = doc.createAdjustmentLayer(AdjustmentType::Curves);
        QVERIFY(!curvesId.isNull());
        QVERIFY(doc.setAdjustmentParameters(curvesId, {
            {QStringLiteral("curves.master.count"), 3.0},
            {QStringLiteral("curves.master.0.in"), 0.0},
            {QStringLiteral("curves.master.0.out"), 0.0},
            {QStringLiteral("curves.master.1.in"), 0.5},
            {QStringLiteral("curves.master.1.out"), 0.2},
            {QStringLiteral("curves.master.2.in"), 1.0},
            {QStringLiteral("curves.master.2.out"), 1.0},
        }));
        QImage withCurves(40, 20, QImage::Format_ARGB32);
        renderTo(withCurves, doc);
        const QColor curvesColor = withCurves.pixelColor(10, 10);
        QVERIFY2(curvesColor.red() < levelsColor.red(),
                 qPrintable(QStringLiteral("curvas deveriam escurecer mais: %1")
                            .arg(curvesColor.red())));

        // Parâmetros corrompidos/insuficientes contam como identidade:
        // sem pontos válidos a curva não quebra o render.
        QVERIFY(doc.setAdjustmentParameters(curvesId, {
            {QStringLiteral("curves.master.count"), 1.0},
            {QStringLiteral("curves.master.0.in"), 0.0},
            {QStringLiteral("curves.master.0.out"), 0.0},
        }));
        QImage backToIdentity(40, 20, QImage::Format_ARGB32);
        renderTo(backToIdentity, doc);
        QCOMPARE(backToIdentity.pixelColor(10, 10), levelsColor);

        // Channel mixer: ganho 2 no verde dobra o canal.
        const LayerId mixerId = doc.createAdjustmentLayer(AdjustmentType::ChannelMixer);
        QVERIFY(!mixerId.isNull());
        QVERIFY(doc.setAdjustmentParameters(mixerId, {
            {QStringLiteral("redGain"), 1.0},
            {QStringLiteral("greenGain"), 2.0},
            {QStringLiteral("blueGain"), 1.0},
        }));
        QImage withMixer(40, 20, QImage::Format_ARGB32);
        renderTo(withMixer, doc);
        const QColor mixerColor = withMixer.pixelColor(10, 10);
        // O ganho só mexe no verde (o vermelho fica igual ao passo anterior);
        // os valores absolutos já vinham escurecidos pelos níveis.
        QVERIFY2(mixerColor.green() > levelsColor.green(),
                 qPrintable(QStringLiteral("ganho 2 devia duplicar o verde: %1 -> %2")
                            .arg(levelsColor.green()).arg(mixerColor.green())));
        QCOMPARE(mixerColor.red(), levelsColor.red());

        clearRendererCaches();
    }

    void layerCacheKeepsEachLayerPosition()
    {
        // Regressão: com 2+ camadas cacheadas, o ponto de blit era membro
        // único do renderer — a segunda camada era colada na posição da
        // primeira (a imagem saltava de sítio e de tamanho).
        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = Qt::black;
        doc.addLayer(std::move(bg));

        // Duas camadas em pontos distantes: se a cache trocar as colagens,
        // uma aparece no lugar da outra.
        // (Shapes sem pontos têm bounds degeneradas — nada desenham.)
        auto s1 = std::make_unique<ShapeLayer>();
        s1->kind = ShapeKind::Rectangle;
        s1->fill = QColor(255, 0, 0);
        s1->points = QPolygonF({QPointF(0, 0), QPointF(20, 20)});
        s1->transform.position = QPointF(10, 10);
        doc.addLayer(std::move(s1));
        auto s2 = std::make_unique<ShapeLayer>();
        s2->kind = ShapeKind::Rectangle;
        s2->fill = QColor(0, 255, 0);
        s2->points = QPolygonF({QPointF(0, 0), QPointF(20, 20)});
        s2->transform.position = QPointF(70, 70);
        doc.addLayer(std::move(s2));
        // Camada de imagem (o cenário do relatório: foto importada).
        QImage photo(40, 40, QImage::Format_ARGB32);
        photo.fill(qRgba(0, 0, 255, 255));
        const LayerId asset = doc.assets().addImage(photo, QStringLiteral("png"));
        auto imgLayer = std::make_unique<ImageLayer>();
        imgLayer->assetId = asset;
        imgLayer->naturalWidth = 40;
        imgLayer->naturalHeight = 40;
        imgLayer->transform.position = QPointF(50, 50);
        doc.addLayer(std::move(imgLayer));

        // Referência: sempre sem cache.
        auto renderNoCache = [&doc]() {
            clearRendererCaches();
            QImage img(100, 100, QImage::Format_ARGB32);
            renderTo(img, doc);
            return img;
        };
        const QImage ref = renderNoCache();
        const QImage ref2 = renderNoCache();
        QCOMPARE(ref2, ref);   // determinismo

        // Com cache: aquecimento + pintura cacheada.
        clearRendererCaches();
        QImage warm(100, 100, QImage::Format_ARGB32);
        renderTo(warm, doc);
        QCOMPARE(warm, ref);
        QImage cached(100, 100, QImage::Format_ARGB32);
        renderTo(cached, doc);
        QCOMPARE(cached, ref);

        // Ambas as cores visíveis (nenhuma comida pela outra).
        int red = 0, green = 0, blue = 0;
        for (int y = 0; y < 100; ++y)
            for (int x = 0; x < 100; ++x) {
                const QColor c = cached.pixelColor(x, y);
                if (c == QColor(255, 0, 0)) ++red;
                if (c == QColor(0, 255, 0)) ++green;
                if (c == QColor(0, 0, 255)) ++blue;
            }
        QVERIFY2(red > 0 && green > 0 && blue > 0,
                 "as três camadas deviam ser visíveis");
        clearRendererCaches();
    }

    void layerCacheIsPanInvariant()
    {
        // Regressão: a chave da cache usava a escala (m11) mas o pixmap era
        // renderizado COM o pan — ao arrastar a vista, a imagem em cache
        // ficava parada enquanto os handles se moviam.
        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = Qt::black;
        doc.addLayer(std::move(bg));
        QImage photo(40, 40, QImage::Format_ARGB32);
        photo.fill(qRgba(50, 200, 50, 255));
        const LayerId asset = doc.assets().addImage(photo, QStringLiteral("png"));
        auto layer = std::make_unique<ImageLayer>();
        layer->assetId = asset;
        layer->naturalWidth = 40;
        layer->naturalHeight = 40;
        layer->transform.position = QPointF(50, 50);
        doc.addLayer(std::move(layer));

        // Base: escala 1, sem translação (aquece a cache).
        clearRendererCaches();
        QImage base(100, 100, QImage::Format_ARGB32);
        base.fill(Qt::magenta);   // sentinela: regiono não pintado fica magenta
        {
            QPainter p(&base);
            cc::renderDocument(doc, &p, QTransform(), {});
        }

        // Agora com pan (+40, +25): a cache ACERTA e o resultado tem de ser
        // o mesmo que desenhar sem cache com essa transformação.
        QTransform panned;
        panned.translate(40, 25);
        QImage pannedCached(100, 100, QImage::Format_ARGB32);
        pannedCached.fill(Qt::magenta);
        {
            QPainter p(&pannedCached);
            cc::renderDocument(doc, &p, panned, {});
        }

        clearRendererCaches();
        QImage pannedRef(100, 100, QImage::Format_ARGB32);
        pannedRef.fill(Qt::magenta);
        {
            QPainter p(&pannedRef);
            cc::renderDocument(doc, &p, panned, {});
        }

        QCOMPARE(pannedCached.size(), pannedRef.size());
        int diff = 0;
        for (int y = 0; y < pannedCached.height(); ++y)
            for (int x = 0; x < pannedCached.width(); ++x)
                if (pannedCached.pixelColor(x, y) != pannedRef.pixelColor(x, y))
                    ++diff;
        QVERIFY2(diff == 0, qPrintable(QStringLiteral(
            "cache com pan devia igualar ao sem cache; %1 pixeis diferem").arg(diff)));
        clearRendererCaches();
    }
    void textOnPathRenders()
    {
        Document doc(120, 80);
        auto path = std::make_unique<VectorLayer>();
        VectorNode a; a.position = QPointF(10, 60);
        VectorNode b; b.position = QPointF(110, 60);
        path->nodes = {a, b};
        path->closed = false;
        // Como a caneta cria: conteúdo em espaço LOCAL centrado e a posição
        // da camada = centro das âncoras. Sem isto o caminho fica centrado
        // na origem do documento e o texto sai fora do ecrã.
        path->transform.position = QPointF(60, 60);
        const LayerId pathId = path->id();
        QVERIFY(doc.addLayer(std::move(path)));

        auto text = std::make_unique<TextLayer>();
        text->content = QStringLiteral("ARCO");
        text->sizePt = 24;
        text->color = QColor(255, 255, 255);
        const LayerId textId = text->id();
        QVERIFY(doc.addLayer(std::move(text)));

        // Sem vínculo: texto recto, alinhado ao topo da caixa.
        QImage straight(120, 80, QImage::Format_ARGB32);
        renderTo(straight, doc);

        // Com vínculo: o texto segue a linha — pixmap diferente.
        QVERIFY(doc.setTextPathLayerId(textId, pathId));
        QImage curved(120, 80, QImage::Format_ARGB32);
        renderTo(curved, doc);
        QVERIFY2(curved != straight, "texto em caminho devia mudar o render");

        // Offset diferente desloca os glifos.
        QVERIFY(doc.setTextPathOffset(textId, 0.5));
        QImage shifted(120, 80, QImage::Format_ARGB32);
        shifted.fill(Qt::transparent);
        renderTo(shifted, doc);
        QVERIFY2(shifted != curved, "offset devia deslocar o texto");

        // Round-trip: o vínculo e o offset sobrevivem a guardar/abrir.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path2 = dir.filePath(QStringLiteral("textpath.creatorcanvas"));
        QString error;
        QVERIFY(cc::saveDocument(doc, path2, &error));
        auto loaded = cc::loadDocument(path2, &error);
        QVERIFY(loaded != nullptr);
        const Layer* loadedText = nullptr;
        for (const auto& child : loaded->rootGroup()->children) {
            if (child && child->type() == LayerType::Text)
                loadedText = child.get();
        }
        QVERIFY(loadedText != nullptr);
        const auto* lt = dynamic_cast<const TextLayer*>(loadedText);
        QVERIFY(lt->pathLayerId.has_value());
        QCOMPARE(lt->pathOffsetPct, 0.5);

        clearRendererCaches();
    }

    void linkedAdjustmentRendersAndInvalidatesOnParamChange()
    {
        Document doc(64, 64);
        QImage gray(64, 64, QImage::Format_ARGB32);
        gray.fill(QColor(100, 100, 100, 255));
        auto img = std::make_unique<ImageLayer>();
        img->assetId = doc.assets().addImage(gray);
        img->naturalWidth = 64;
        img->naturalHeight = 64;
        img->transform.position = QPointF(32, 32);
        const LayerId imgId = img->id();
        QVERIFY(doc.addLayer(std::move(img)));

        // Liga uma adjustment layer de brilho/contraste.
        const LayerId adjId = doc.createAdjustmentLayer(
            AdjustmentType::BrightnessContrast, imgId);
        QVERIFY(!adjId.isNull());
        QVERIFY(doc.setLayerAdjustmentLayerId(imgId, adjId));
        QVERIFY(doc.setAdjustmentParameters(adjId, {{QStringLiteral("brightness"), 0.0},
                                                     {QStringLiteral("contrast"), 0.0}}));

        QImage out1(64, 64, QImage::Format_RGB32);
        out1.fill(Qt::black);
        renderTo(out1, doc);
        const QColor base = out1.pixelColor(32, 32);
        QVERIFY(qAbs(base.red() - 100) <= 2); // brilho 0 ≈ inalterado

        // Render repetido com os mesmos parâmetros = resultado idêntico
        // (a cache do ajuste está correta e não corrompe o pixmap).
        QImage out1b(64, 64, QImage::Format_RGB32);
        out1b.fill(Qt::black);
        renderTo(out1b, doc);
        QCOMPARE(out1b.pixelColor(32, 32), base);

        // Mudar parâmetros invalida a cache: o render seguinte reflete o novo
        // valor (não fica preso ao resultado antigo).
        QVERIFY(doc.setAdjustmentParameters(adjId, {{QStringLiteral("brightness"), 50.0},
                                                     {QStringLiteral("contrast"), 0.0}}));
        QImage out2(64, 64, QImage::Format_RGB32);
        out2.fill(Qt::black);
        renderTo(out2, doc);
        const QColor brighter = out2.pixelColor(32, 32);
        // brilho=50 em cinza 100 → ~228 com UMA aplicação. Se o ajuste fosse
        // aplicado 2× (nível da imagem + nível da árvore), daria 255 (clamp).
        QVERIFY(brighter.red() > base.red() + 100);
        QVERIFY2(brighter.red() < 250, "ajuste linkado aplicado mais de uma vez?");

        // De novo sem mudar nada: idêntico ao anterior (cache válida).
        QImage out2b(64, 64, QImage::Format_RGB32);
        out2b.fill(Qt::black);
        renderTo(out2b, doc);
        QCOMPARE(out2b.pixelColor(32, 32), brighter);

        clearRendererCaches();
    }

    void linkedAdjustmentAffectsOnlyItsImage()
    {
        // Duas imagens sólidas lado a lado; ajuste linkado só à de cima.
        Document doc(220, 100);
        QImage redImg(100, 100, QImage::Format_ARGB32);
        redImg.fill(QColor(200, 30, 30));
        auto red = std::make_unique<ImageLayer>();
        red->assetId = doc.assets().addImage(redImg);
        red->naturalWidth = 100;
        red->naturalHeight = 100;
        red->transform.position = QPointF(50, 50);
        QVERIFY(doc.addLayer(std::move(red)));

        QImage blueImg(100, 100, QImage::Format_ARGB32);
        blueImg.fill(QColor(30, 30, 200));
        auto blue = std::make_unique<ImageLayer>();
        blue->assetId = doc.assets().addImage(blueImg);
        blue->naturalWidth = 100;
        blue->naturalHeight = 100;
        blue->transform.position = QPointF(170, 50);
        const LayerId blueId = blue->id();
        QVERIFY(doc.addLayer(std::move(blue)));

        const LayerId adjId = doc.createAdjustmentLayer(
            AdjustmentType::BrightnessContrast, blueId);
        QVERIFY(doc.setLayerAdjustmentLayerId(blueId, adjId));
        QVERIFY(doc.setAdjustmentParameters(adjId, {{QStringLiteral("brightness"), 50.0},
                                                     {QStringLiteral("contrast"), 0.0}}));

        QImage out(220, 100, QImage::Format_RGB32);
        out.fill(Qt::black);
        renderTo(out, doc);
        // a vermelha (sem ajuste) NÃO muda
        QCOMPARE(out.pixelColor(50, 50).red(), 200);
        // a azul muda exatamente 1× (não 2×, não vazamento)
        const int blueRed = out.pixelColor(170, 50).red();
        QVERIFY2(blueRed > 30 + 100, "ajuste não aplicado?");
        QVERIFY2(blueRed < 250, "ajuste aplicado 2×?");

        // Esconder a camada de ajuste remove o efeito (só na azul).
        QVERIFY(doc.setLayerVisible(adjId, false));
        QImage out2(220, 100, QImage::Format_RGB32);
        out2.fill(Qt::black);
        renderTo(out2, doc);
        QCOMPARE(out2.pixelColor(170, 50).red(), 30);
        QCOMPARE(out2.pixelColor(50, 50).red(), 200);

        clearRendererCaches();
    }

    void unlinkedAdjustmentAffectsAllBelow()
    {
        // Ajuste SOLTO na pilha (sem sourceLayerId) afeta tudo abaixo —
        // comportamento Photoshop intencional para adjustment layers livres.
        Document doc(220, 100);
        QImage redImg(100, 100, QImage::Format_ARGB32);
        redImg.fill(QColor(200, 30, 30));
        auto red = std::make_unique<ImageLayer>();
        red->assetId = doc.assets().addImage(redImg);
        red->naturalWidth = 100;
        red->naturalHeight = 100;
        red->transform.position = QPointF(50, 50);
        QVERIFY(doc.addLayer(std::move(red)));

        auto adj = std::make_unique<AdjustmentLayer>();
        adj->kind = AdjustmentType::BrightnessContrast;
        adj->parameters = {{QStringLiteral("brightness"), 50.0},
                           {QStringLiteral("contrast"), 0.0}};
        QVERIFY(doc.addLayer(std::move(adj)));

        QImage out(220, 100, QImage::Format_RGB32);
        out.fill(Qt::black);
        renderTo(out, doc);
        // brilho=50 em 200 → 327 → clamp 255: claramente afetada (>240)
        QVERIFY(out.pixelColor(50, 50).red() > 240);

        clearRendererCaches();
    }

    void layerCacheReappliesBlendModeOnEveryBlit()
    {
        Document doc(16, 16);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(40, 100, 160);
        QVERIFY(doc.addLayer(std::move(bg)));

        QImage pixels(16, 16, QImage::Format_ARGB32_Premultiplied);
        pixels.fill(QColor(200, 80, 40, 255));
        auto image = std::make_unique<ImageLayer>();
        image->assetId = doc.assets().addImage(pixels);
        image->naturalWidth = 16;
        image->naturalHeight = 16;
        image->transform.position = QPointF(8, 8);
        image->blendMode = BlendMode::Multiply;
        const LayerId imageId = image->id();
        QVERIFY(doc.addLayer(std::move(image)));

        RenderOptions options;
        options.drawCheckerboard = false;
        options.canvasBorder = QColor(0, 0, 0, 0);
        auto render = [&](IRenderer& renderer) {
            QImage out(16, 16, QImage::Format_ARGB32_Premultiplied);
            out.fill(Qt::transparent);
            QPainter painter(&out);
            renderer.renderDocument(doc, &painter, QTransform(), options);
            painter.end();
            return out;
        };

        SoftwareRenderer plain;
        const QImage reference = render(plain);
        CachingRenderer caching;
        const QImage warm = render(caching);
        QCOMPARE(warm, reference);
        const QImage cached = render(caching);
        QCOMPARE(cached, reference);
        QCOMPARE(caching.cacheHits(), 1);
        QVERIFY(caching.cacheBytes() > 0);

        // dropLayer tem de descontar exatamente a entrada removida; clear
        // reinicia tanto os bytes como as estatísticas das duas caches.
        caching.invalidateLayer(imageId);
        QCOMPARE(caching.cacheBytes(), qint64(0));
        caching.clearCaches();
        QCOMPARE(caching.cacheBytes(), qint64(0));
        QCOMPARE(caching.cacheHits(), 0);
        QCOMPARE(caching.cacheMisses(), 0);
    }

    void imageEffectsUseMaskedPixelsAndSourceAwareKeys()
    {
        clearRendererCaches();
        Document doc(8, 8);
        QImage bitmap(8, 8, QImage::Format_ARGB32_Premultiplied);
        bitmap.fill(QColor(30, 80, 200, 255));

        auto image = std::make_unique<ImageLayer>();
        image->assetId = doc.assets().addImage(bitmap);
        image->naturalWidth = 8;
        image->naturalHeight = 8;
        image->transform.position = QPointF(4, 4);
        image->setFillOpacity(0.0f); // isola os dois efeitos do conteúdo
        image->effects.dropShadow.enabled = true;
        image->effects.dropShadow.distance = 0.0;
        image->effects.dropShadow.blur = 0.0;
        image->effects.dropShadow.opacity = 1.0;
        image->effects.dropShadow.color = QColor(255, 0, 0, 255);
        image->effects.outline.enabled = true;
        image->effects.outline.width = 1.0;
        image->effects.outline.blur = 0.0;
        image->effects.outline.opacity = 1.0;
        image->effects.outline.color = QColor(0, 255, 0, 255);
        const LayerId imageId = image->id();
        QVERIFY(doc.addLayer(std::move(image)));

        const auto halfMask = [](bool left) {
            QImage mask(8, 8, QImage::Format_Alpha8);
            for (int y = 0; y < 8; ++y) {
                uchar* row = mask.scanLine(y);
                for (int x = 0; x < 8; ++x)
                    row[x] = ((x < 4) == left) ? 255 : 0;
            }
            return mask;
        };
        QVERIFY(doc.setLayerAlphaMask(imageId, halfMask(true)));

        RenderOptions options;
        options.drawCheckerboard = false;
        options.canvasBorder = QColor(0, 0, 0, 0);
        QImage first(8, 8, QImage::Format_ARGB32_Premultiplied);
        first.fill(Qt::transparent);
        renderTo(first, doc, options);
        QVERIFY(first.pixelColor(0, 4).alpha() > 0);
        QCOMPARE(first.pixelColor(7, 4).alpha(), 0);

        // Não limpar caches: a nova máscara tem de produzir uma nova chave.
        // Caso contrário a segunda pintura reutiliza a silhueta da esquerda.
        QVERIFY(doc.setLayerAlphaMask(imageId, halfMask(false)));
        QImage second(8, 8, QImage::Format_ARGB32_Premultiplied);
        second.fill(Qt::transparent);
        renderTo(second, doc, options);
        QCOMPARE(second.pixelColor(0, 4).alpha(), 0);
        QVERIFY(second.pixelColor(7, 4).alpha() > 0);
        clearRendererCaches();
    }

    void siblingClippingUsesImmediateVisibleBase()
    {
        Document doc(16, 16);
        const auto solidLayer = [&doc](const QColor& color, const QSize& size,
                                       const QPointF& position) {
            QImage pixels(size, QImage::Format_ARGB32_Premultiplied);
            pixels.fill(color);
            auto layer = std::make_unique<ImageLayer>();
            layer->assetId = doc.assets().addImage(pixels);
            layer->naturalWidth = size.width();
            layer->naturalHeight = size.height();
            layer->transform.position = position;
            return layer;
        };

        auto bottom = solidLayer(Qt::red, QSize(4, 4), QPointF(2, 2));
        QVERIFY(doc.addLayer(std::move(bottom)));
        auto immediate = solidLayer(Qt::green, QSize(4, 4), QPointF(10, 2));
        QVERIFY(doc.addLayer(std::move(immediate)));
        auto clipped = solidLayer(Qt::yellow, QSize(16, 16), QPointF(8, 8));
        clipped->clipped = true;
        const LayerId clippedId = clipped->id();
        QVERIFY(doc.addLayer(std::move(clipped)));

        QImage out(16, 16, QImage::Format_ARGB32_Premultiplied);
        out.fill(Qt::transparent);
        QPainter painter(&out);
        renderSiblingLayers(doc, QList<QUuid>{clippedId}, &painter);
        painter.end();

        // A base é a camada imediatamente abaixo (índice 1), não a camada 0.
        QCOMPARE(out.pixelColor(10, 2), QColor(Qt::yellow));
        QCOMPARE(out.pixelColor(2, 2).alpha(), 0);
    }

    void compositeCacheAccountingReplacementBudgetAndClear()
    {
        CachingRenderer renderer;
        Document doc(8, 8);
        const QSize docSize(8, 8);
        QImage small(docSize, QImage::Format_ARGB32_Premultiplied);
        small.fill(Qt::red);
        const qint64 smallBytes = small.sizeInBytes();

        renderer.storeComposite(small, doc, docSize, QTransform(), {});
        QCOMPARE(renderer.cacheBytes(), qint64(0)); // caches independentes
        QCOMPARE(renderer.m_compositeBytes, smallBytes);
        QCOMPARE(renderer.m_compositeCache.size(), 1);

        // Mesma revisão/fingerprint = substituição, nunca dupla contagem.
        renderer.storeComposite(small, doc, docSize, QTransform(), {});
        QCOMPARE(renderer.m_compositeBytes, smallBytes);
        QCOMPARE(renderer.m_compositeCache.size(), 1);

        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = Qt::black;
        const LayerId bgId = bg->id();
        QVERIFY(doc.addLayer(std::move(bg))); // nova revisão = nova chave
        renderer.storeComposite(small, doc, docSize, QTransform(), {});
        QCOMPARE(renderer.m_compositeBytes, smallBytes * 2);
        QCOMPARE(renderer.m_compositeCache.size(), 2);

        const auto storedBytes = [&renderer] {
            qint64 total = 0;
            for (auto it = renderer.m_compositeCache.constBegin();
                 it != renderer.m_compositeCache.constEnd(); ++it)
                total += it.value().sizeInBytes();
            return total;
        };
        QCOMPARE(renderer.m_compositeBytes, storedBytes());

        // Duas entradas de 64 MiB excedem o orçamento de 96 MiB. A cache deve
        // despejar até caber, mantendo o contador igual à soma real das QImages.
        QImage large(4096, 4096, QImage::Format_ARGB32_Premultiplied);
        QVERIFY(!large.isNull());
        large.fill(Qt::transparent);
        QVERIFY(doc.setLayerVisible(bgId, false));
        renderer.storeComposite(large, doc, docSize, QTransform(), {});
        QVERIFY(doc.setLayerVisible(bgId, true));
        renderer.storeComposite(large, doc, docSize, QTransform(), {});
        constexpr qint64 budget = qint64(96) * 1024 * 1024;
        QVERIFY(renderer.m_compositeBytes > 0);
        QVERIFY(renderer.m_compositeBytes <= budget);
        QCOMPARE(renderer.m_compositeBytes, storedBytes());
        QCOMPARE(renderer.cacheBytes(), qint64(0));

        renderer.clearCaches();
        QCOMPARE(renderer.m_compositeBytes, qint64(0));
        QCOMPARE(renderer.cacheBytes(), qint64(0));
        QVERIFY(renderer.m_compositeCache.isEmpty());
        QCOMPARE(renderer.compositeHits(), 0);
        QCOMPARE(renderer.compositeMisses(), 0);
    }

    void adjustmentSliderStress()
    {
        // Imita o fluxo da aba Imagem: criar adjustment linkada e depois uma
        // sequência de updates (arrastar slider) com render entre cada um,
        // seguidos de undo/redo — exercita a cache do ajuste e os comandos.
        Document doc(96, 96);
        CommandStack history;
        QImage img(96, 96, QImage::Format_ARGB32);
        img.fill(QColor(90, 120, 160, 255));
        auto layer = std::make_unique<ImageLayer>();
        layer->assetId = doc.assets().addImage(img);
        layer->naturalWidth = 96;
        layer->naturalHeight = 96;
        layer->transform.position = QPointF(48, 48);
        const LayerId imgId = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        auto create = std::make_unique<CreateAdjustmentLayerCommand>(
            doc, AdjustmentType::BrightnessContrast,
            AdjustmentParams{{QStringLiteral("brightness"), 0.0},
                             {QStringLiteral("contrast"), 0.0}},
            imgId, QStringLiteral("adjustment.create"), 1.0f);
        const LayerId adjId = [&] {
            auto* raw = create.get();
            history.execute(std::move(create));
            return raw->createdId();
        }();
        QVERIFY(!adjId.isNull());

        QImage out(96, 96, QImage::Format_RGB32);
        QColor last(0, 0, 0);
        for (int i = 0; i <= 20; ++i) {
            const double b = -100.0 + i * 10.0; // arrasto do slider
            const auto* adj = doc.findAdjustmentLayer(adjId);
            history.execute(std::make_unique<UpdateAdjustmentLayerCommand>(
                doc, adjId, adj->kind, adj->kind, adj->parameters,
                AdjustmentParams{{QStringLiteral("brightness"), b},
                                 {QStringLiteral("contrast"), 0.0}},
                QStringLiteral("adjustment.update")));
            out.fill(Qt::black);
            renderTo(out, doc);
            last = out.pixelColor(48, 48);
        }
        // brilho máximo → píxel mais claro que o original
        QVERIFY(last.red() > 90 + 40);

        // undo de todos os updates + criação → volta ao original
        for (int i = 0; i < 21; ++i)
            history.undo();
        QVERIFY(doc.findAdjustmentLayer(adjId) == nullptr);
        out.fill(Qt::black);
        renderTo(out, doc);
        const QColor restored = out.pixelColor(48, 48);
        QVERIFY(qAbs(restored.red() - 90) <= 2);

        // redo tudo → o vínculo volta a existir (o id regenera no redo do
        // comando de criação — comportamento pré-existente; procuramos pelo
        // link na camada de imagem, não pelo id antigo).
        for (int i = 0; i < 21; ++i)
            history.redo();
        const auto* imgLayer = static_cast<const ImageLayer*>(doc.findLayer(imgId));
        QVERIFY(imgLayer != nullptr);
        QVERIFY(imgLayer->adjustmentLayerId.has_value());
        QVERIFY(doc.findAdjustmentLayer(*imgLayer->adjustmentLayerId) != nullptr);
        out.fill(Qt::black);
        renderTo(out, doc); // não pode crashar
        QVERIFY(out.pixelColor(48, 48).red() >= 0);

        clearRendererCaches();
    }
};


    void layerCacheAndRendererInterface()
    {
        // Documento com texto (camada cacheável) + fundo (não cacheável).
        Document doc(60, 40);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(240, 240, 240);
        QVERIFY(doc.addLayer(std::move(bg)));
        auto text = std::make_unique<TextLayer>();
        text->content = QStringLiteral("ABC");
        text->sizePt = 24;
        text->transform.position = QPointF(10, 8);
        const LayerId textId = text->id();
        QVERIFY(doc.addLayer(std::move(text)));

        auto& renderer = defaultRenderer();
        QCOMPARE(QString::fromLatin1(renderer.name()), QStringLiteral("software+cache"));
        renderer.clearCaches();

        QImage first(60, 40, QImage::Format_ARGB32);
        renderTo(first, doc);

        // Segunda pintura na MESMA revisão: o cache acerta e o resultado é
        // idêntico (pixmap colado, não re-rasterizado).
        QImage second(60, 40, QImage::Format_ARGB32);
        renderTo(second, doc);
        QCOMPARE(second, first);
        QVERIFY2(dynamic_cast<CachingRenderer*>(&renderer)->cacheHits() > 0,
                 "o cache por camada devia ter acertado na segunda pintura");

        // Editar o texto muda a revisão: o cache invalida e o novo texto
        // aparece — sem pixmap velho.
        doc.setLayerTextContent(textId, QStringLiteral("XYZ"));
        doc.setLayerTextColor(textId, QColor(200, 0, 0));
        QImage edited(60, 40, QImage::Format_ARGB32);
        renderTo(edited, doc);
        QVERIFY(edited != first);

        // Invalidar à mão e repintar dá o mesmo resultado.
        clearRendererCaches();
        QImage afterClear(60, 40, QImage::Format_ARGB32);
        renderTo(afterClear, doc);
        QCOMPARE(afterClear, edited);

        // A interface aceita os dois renderers e ambos produzem o mesmo.
        SoftwareRenderer plain;
        plain.clearCaches();
        QImage viaSoftware(60, 40, QImage::Format_ARGB32);
        {
            QPainter p(&viaSoftware);
            plain.renderDocument(doc, &p, QTransform(), {});
        }
        QCOMPARE(viaSoftware, afterClear);

        CachingRenderer local;
        QImage viaCaching(60, 40, QImage::Format_ARGB32);
        {
            QPainter p(&viaCaching);
            local.renderDocument(doc, &p, QTransform(), {});
            local.renderDocument(doc, &p, QTransform(), {});
        }
        QCOMPARE(viaCaching, afterClear);
        QVERIFY(local.cacheHits() > 0);
        local.clearCaches();
        QCOMPARE(local.cacheHits(), 0);

        // invalidateLayer força re-renderização sem quebrar o resultado.
        auto* caching = dynamic_cast<CachingRenderer*>(&renderer);
        QVERIFY(caching != nullptr);
        caching->invalidateLayer(textId);
        QImage afterInvalidate(60, 40, QImage::Format_ARGB32);
        renderTo(afterInvalidate, doc);
        QCOMPARE(afterInvalidate, edited);

        clearRendererCaches();
    }


    void conicalAndMultiStopGradients()
    {
        Document doc(40, 30);
        auto shape = std::make_unique<ShapeLayer>();
        shape->kind = ShapeKind::Rectangle;
        shape->fill = Qt::red;
        shape->transform.position = QPointF(0, 0);
        const LayerId id = shape->id();
        QVERIFY(doc.addLayer(std::move(shape)));

        auto* layer = dynamic_cast<ShapeLayer*>(doc.findLayer(id));
        QVERIFY(layer);

        // Baseline: cor sólida.
        QImage solid(40, 30, QImage::Format_ARGB32);
        renderTo(solid, doc);
        const QColor solidColor = solid.pixelColor(20, 15);

        // 1) Multi-stop linear: preto -> branco -> preto (3 paragens).
        layer->gradient.enabled = true;
        layer->gradient.kind = GradientKind::Linear;
        layer->gradient.angleDeg = 0;
        layer->gradient.stops = {{0.0f, Qt::black}, {0.5f, Qt::white}, {1.0f, Qt::black}};
        QImage multi(40, 30, QImage::Format_ARGB32);
        renderTo(multi, doc);
        QVERIFY(multi.pixelColor(20, 15) != solidColor);
        // O meio é branco e as pontas escuras — assinatura do multi-stop.
        QVERIFY2(multi.pixelColor(20, 15).red() > 200,
                 qPrintable(QStringLiteral("meio devia ser claro: %1")
                            .arg(multi.pixelColor(20, 15).red())));
        QVERIFY2(multi.pixelColor(2, 15).red() < 60,
                 qPrintable(QStringLiteral("ponta devia ser escura: %1")
                            .arg(multi.pixelColor(2, 15).red())));

        // 2) Cónico: duas zonas de cor distintas a 0.5 do ângulo.
        layer->gradient.kind = GradientKind::Conical;
        layer->gradient.angleDeg = 0;
        layer->gradient.stops = {{0.0f, Qt::black}, {0.5f, Qt::white}, {1.0f, Qt::black}};
        QImage conical(40, 30, QImage::Format_ARGB32);
        renderTo(conical, doc);
        QVERIFY2(conical.pixelColor(38, 15) != conical.pixelColor(20, 15),
                 "cónico devia variar ao longo do ângulo");

        // 3) Série de paragens não estoura o render (32 paragens).
        layer->gradient.stops.clear();
        for (int i = 0; i < 32; ++i) {
            const double t2 = static_cast<double>(i) / 31.0;
            layer->gradient.stops.push_back(
                {static_cast<float>(t2), QColor::fromHsvF(t2, 1.0, 1.0)});
        }
        QImage many(40, 30, QImage::Format_ARGB32);
        renderTo(many, doc);
        QVERIFY(many.pixelColor(20, 15) != QColor(0, 0, 0, 0));

        clearRendererCaches();
    }



QTEST_MAIN(TestRenderer)
#include "test_renderer.moc"
