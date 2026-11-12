#include "core/Document.h"
#include "core/layers/Layer.h"
#include "rendering/CanvasRenderer.h"
#include "rendering/IRenderer.h"
#include "core/layers/Gradient.h"
#include "core/serialization/ProjectFile.h"

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


    void textOnPathRenders()
    {
        Document doc(120, 80);
        auto path = std::make_unique<VectorLayer>();
        VectorNode a; a.position = QPointF(10, 60);
        VectorNode b; b.position = QPointF(110, 60);
        path->nodes = {a, b};
        path->closed = false;
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

QTEST_GUILESS_MAIN(TestRenderer)
#include "test_renderer.moc"

