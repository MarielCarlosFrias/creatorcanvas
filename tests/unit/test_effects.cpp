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

        const QRectF expanded = text->contentBounds();
        QVERIFY(expanded.left() < baseBounds.left());
        QVERIFY(expanded.right() > baseBounds.right());
        QVERIFY(expanded.bottom() > baseBounds.bottom());
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
};

QTEST_GUILESS_MAIN(TestEffects)
#include "test_effects.moc"
