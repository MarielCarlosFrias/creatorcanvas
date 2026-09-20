#include "core/Document.h"
#include "core/layers/Layer.h"
#include "core/serialization/ProjectFile.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace cc;

class TestEffects final : public QObject
{
    Q_OBJECT

private slots:
    void setLayerTextEffectsApplies()
    {
        Document doc(200, 200);
        auto layer = std::make_unique<TextLayer>();
        layer->content = QStringLiteral("Hello");
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        TextEffects fx;
        fx.outline.enabled = true;
        fx.outline.width = 5.0;
        fx.shadow.enabled = true;
        fx.shadow.offsetX = 6.0;
        fx.shadow.offsetY = 8.0;
        fx.shadow.blur = 10.0;

        QVERIFY(doc.setLayerTextEffects(id, fx));
        auto* text = static_cast<TextLayer*>(doc.findLayer(id));
        QVERIFY(text->effects == fx);
        QVERIFY(doc.setLayerTextEffects(id, fx));
    }

    void contentBoundsExpandForShadow()
    {
        Document doc(400, 400);
        auto layer = std::make_unique<TextLayer>();
        layer->content = QStringLiteral("Hello");
        layer->box = QSizeF(200, 60);
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        TextLayer* text = static_cast<TextLayer*>(doc.findLayer(id));
        const QRectF baseBounds = text->contentBounds();

        TextEffects fx;
        fx.shadow.enabled = true;
        fx.shadow.offsetX = 20.0;
        fx.shadow.offsetY = 20.0;
        fx.shadow.blur = 10.0;
        QVERIFY(doc.setLayerTextEffects(id, fx));

        const QRectF afterBounds = text->contentBounds();
        // Bounds stay stable so transform pivot and handles do not shift
        QCOMPARE(afterBounds, baseBounds);
    }

    void effectsRoundTripThroughSerialization()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("fx.creatorcanvas");

        Document doc(400, 400);
        auto layer = std::make_unique<TextLayer>();
        layer->content = QStringLiteral("Styled");
        TextEffects fx;
        fx.outline.enabled = true;
        fx.outline.color = QColor(255, 255, 255);
        fx.outline.width = 6.0;
        fx.shadow.enabled = true;
        fx.shadow.color = QColor(0, 0, 0, 170);
        fx.shadow.offsetX = 5.0;
        fx.shadow.offsetY = 7.0;
        fx.shadow.blur = 9.0;
        layer->effects = fx;
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        QString error;
        QVERIFY(saveDocument(doc, path, &error));
        auto loaded = loadDocument(path, &error);
        QVERIFY(loaded != nullptr);

        auto* loadedLayer = static_cast<TextLayer*>(loaded->findLayer(id));
        QVERIFY(loadedLayer != nullptr);
        QVERIFY(loadedLayer->effects == fx);
    }

    void imageEffectsAppliesAndSerializes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("img_fx.creatorcanvas");

        Document doc(500, 500);
        auto img = std::make_unique<ImageLayer>();
        img->naturalWidth = 200;
        img->naturalHeight = 200;

        ImageEffects fx;
        fx.outline.enabled = true;
        fx.outline.color = QColor(255, 230, 0);
        fx.outline.width = 12.0;
        fx.outline.blur = 4.0;
        fx.outline.opacity = 0.9;
        img->effects = fx;

        const LayerId id = img->id();
        QVERIFY(doc.addLayer(std::move(img)));

        // Teste de mutação via Document API
        ImageEffects newFx = fx;
        newFx.outline.width = 16.0;
        QVERIFY(doc.setLayerImageEffects(id, newFx));
        auto* modified = static_cast<ImageLayer*>(doc.findLayer(id));
        QVERIFY(modified != nullptr);
        QVERIFY(modified->effects == newFx);

        // Teste de Serialização Round-Trip
        QString error;
        QVERIFY(saveDocument(doc, path, &error));
        auto loaded = loadDocument(path, &error);
        QVERIFY(loaded != nullptr);

        auto* loadedImg = static_cast<ImageLayer*>(loaded->findLayer(id));
        QVERIFY(loadedImg != nullptr);
        QVERIFY(loadedImg->effects == newFx);
    }

    void newShapeKindsCreationAndRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("shapes.creatorcanvas");

        Document doc(600, 600);

        const QVector<ShapeKind> kinds = {
            ShapeKind::ArrowRight,
            ShapeKind::ArrowCurved,
            ShapeKind::Star,
            ShapeKind::Badge
        };

        QVector<LayerId> ids;
        for (ShapeKind k : kinds) {
            auto shape = std::make_unique<ShapeLayer>();
            shape->kind = k;
            shape->fill = QColor(255, 0, 100);
            shape->stroke = Qt::white;
            shape->strokeWidth = 2.0;
            shape->points = QPolygonF{ QPointF(0, 0), QPointF(100, 0), QPointF(100, 100), QPointF(0, 100) };
            ids.append(shape->id());
            QVERIFY(doc.addLayer(std::move(shape)));
        }

        QString error;
        QVERIFY(saveDocument(doc, path, &error));
        auto loaded = loadDocument(path, &error);
        QVERIFY(loaded != nullptr);

        for (int i = 0; i < kinds.size(); ++i) {
            auto* loadedShape = static_cast<ShapeLayer*>(loaded->findLayer(ids[i]));
            QVERIFY(loadedShape != nullptr);
            QCOMPARE(loadedShape->kind, kinds[i]);
        }
    }
};

QTEST_GUILESS_MAIN(TestEffects)
#include "test_effects.moc"
