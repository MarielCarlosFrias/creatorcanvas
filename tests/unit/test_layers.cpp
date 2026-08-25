#include "core/layers/Layer.h"

#include <QtTest>

using namespace cc;

class TestLayers final : public QObject
{
    Q_OBJECT

private slots:
    void factoryCreatesAllTypes()
    {
        const LayerType types[] = {
            LayerType::Group, LayerType::Image, LayerType::Text,
            LayerType::Shape, LayerType::Background
        };
        for (LayerType type : types) {
            auto layer = makeLayer(type);
            QVERIFY(layer != nullptr);
            QCOMPARE(static_cast<int>(layer->type()), static_cast<int>(type));
            QVERIFY(!layer->id().isNull());
        }
    }

    void freshLayersHaveUniqueIds()
    {
        auto a = makeLayer(LayerType::Text);
        auto b = makeLayer(LayerType::Text);
        QVERIFY(a->id() != b->id());
    }

    void opacityIsClamped()
    {
        auto layer = makeLayer(LayerType::Text);
        layer->setOpacity(1.7f);
        QCOMPARE(layer->opacity(), 1.0f);
        layer->setOpacity(-0.25f);
        QCOMPARE(layer->opacity(), 0.0f);
        layer->setOpacity(0.35f);
        QCOMPARE(layer->opacity(), 0.35f);
    }

    void deepCopyOfTextCopiesPayloadWithFreshId()
    {
        TextLayer original;
        original.name = QStringLiteral("Main Title");
        original.visible = false;
        original.locked = true;
        original.setOpacity(0.4f);
        original.blendMode = BlendMode::Multiply;
        original.content = QStringLiteral("HELLO");
        original.fontFamily = QStringLiteral("Impact");
        original.sizePt = 72.0;
        original.bold = true;
        original.italic = true;
        original.underline = true;
        original.color = QColor(255, 128, 0);
        original.letterSpacingPx = 2.5;
        original.lineHeightMult = 1.2;
        original.align = TextAlignment::Right;

        auto copy = original.deepCopy();
        QVERIFY(copy != nullptr);
        QCOMPARE(static_cast<int>(copy->type()), static_cast<int>(LayerType::Text));
        QVERIFY(copy->id() != original.id());

        auto* text = static_cast<TextLayer*>(copy.get());
        QCOMPARE(text->name, original.name);
        QCOMPARE(text->visible, original.visible);
        QCOMPARE(text->locked, original.locked);
        QCOMPARE(text->opacity(), original.opacity());
        QCOMPARE(static_cast<int>(text->blendMode),
                 static_cast<int>(original.blendMode));
        QCOMPARE(text->content, original.content);
        QCOMPARE(text->fontFamily, original.fontFamily);
        QCOMPARE(text->sizePt, original.sizePt);
        QCOMPARE(text->bold, original.bold);
        QCOMPARE(text->italic, original.italic);
        QCOMPARE(text->underline, original.underline);
        QCOMPARE(text->color, original.color);
        QCOMPARE(text->letterSpacingPx, original.letterSpacingPx);
        QCOMPARE(text->lineHeightMult, original.lineHeightMult);
        QCOMPARE(static_cast<int>(text->align), static_cast<int>(original.align));
    }

    void deepCopyOfGroupIsRecursiveWithFreshIds()
    {
        auto original = std::make_unique<GroupLayer>();
        original->name = QStringLiteral("Characters");
        original->visible = false;
        original->setOpacity(0.5f);

        auto text = std::make_unique<TextLayer>();
        text->name = QStringLiteral("A");
        text->content = QStringLiteral("Hello");
        const LayerId textId = text->id();

        auto shape = std::make_unique<ShapeLayer>();
        shape->name = QStringLiteral("B");
        shape->kind = ShapeKind::Ellipse;
        shape->fill = QColor(200, 10, 10);
        const LayerId shapeId = shape->id();

        original->children.push_back(std::move(text));
        original->children.push_back(std::move(shape));

        auto copy = original->deepCopy();
        auto* copyGroup = static_cast<GroupLayer*>(copy.get());
        QVERIFY(copyGroup->id() != original->id());
        QCOMPARE(copyGroup->children.size(), std::size_t(2));
        QCOMPARE(copyGroup->name, QStringLiteral("Characters"));
        QCOMPARE(copyGroup->visible, false);
        QCOMPARE(copyGroup->opacity(), 0.5f);

        const Layer& copiedText = *copyGroup->children[0];
        const Layer& copiedShape = *copyGroup->children[1];
        QVERIFY(copiedText.id() != textId);
        QVERIFY(copiedShape.id() != shapeId);
        QVERIFY(copiedText.id() != copiedShape.id());

        QCOMPARE(static_cast<int>(copiedText.type()),
                 static_cast<int>(LayerType::Text));
        QCOMPARE(static_cast<int>(copiedShape.type()),
                 static_cast<int>(LayerType::Shape));
        QCOMPARE(static_cast<const TextLayer&>(copiedText).content,
                 QStringLiteral("Hello"));
        QCOMPARE(static_cast<int>(static_cast<const ShapeLayer&>(copiedShape).kind),
                 static_cast<int>(ShapeKind::Ellipse));
        QCOMPARE(static_cast<const ShapeLayer&>(copiedShape).fill,
                 QColor(200, 10, 10));
        // Child names preserved (suffix only on the duplicated top level).
        QCOMPARE(copiedText.name, QStringLiteral("A"));
    }

    void deepCopyOfShapeAndImageCopiesPayload()
    {
        ShapeLayer shape;
        shape.kind = ShapeKind::Polygon;
        shape.stroke = QColor(0, 255, 0);
        shape.strokeWidth = 3.0;
        shape.cornerRadius = 4.0;
        shape.points = QPolygonF({ QPointF(0, 0), QPointF(10, 0), QPointF(5, 8) });

        auto shapeCopy = shape.deepCopy();
        auto* s = static_cast<ShapeLayer*>(shapeCopy.get());
        QCOMPARE(static_cast<int>(s->kind), static_cast<int>(ShapeKind::Polygon));
        QCOMPARE(s->stroke, QColor(0, 255, 0));
        QCOMPARE(s->strokeWidth, 3.0);
        QCOMPARE(s->cornerRadius, 4.0);
        QCOMPARE(s->points, shape.points);

        ImageLayer image;
        image.assetId = newLayerId();
        image.naturalWidth = 640;
        image.naturalHeight = 480;
        auto imageCopy = image.deepCopy();
        auto* im = static_cast<ImageLayer*>(imageCopy.get());
        QCOMPARE(im->assetId, image.assetId);
        QCOMPARE(im->naturalWidth, 640);
        QCOMPARE(im->naturalHeight, 480);
    }
};

QTEST_GUILESS_MAIN(TestLayers)
#include "test_layers.moc"
