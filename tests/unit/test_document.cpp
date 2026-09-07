#include "core/Document.h"

#include <QSignalSpy>
#include <QtTest>

using namespace cc;

namespace {

std::unique_ptr<TextLayer> makeText(const QString& name)
{
    auto layer = std::make_unique<TextLayer>();
    layer->name = name;
    return layer;
}

std::unique_ptr<GroupLayer> makeGroup(const QString& name)
{
    auto layer = std::make_unique<GroupLayer>();
    layer->name = name;
    return layer;
}

std::unique_ptr<ShapeLayer> makeShape(const QString& name, ShapeKind kind = ShapeKind::Rectangle)
{
    auto layer = std::make_unique<ShapeLayer>();
    layer->name = name;
    layer->kind = kind;
    return layer;
}

} // namespace

class TestDocument final : public QObject
{
    Q_OBJECT

private slots:
    void constructorCreatesRootGroup()
    {
        Document doc(1280, 720, 96);
        QVERIFY(doc.rootGroup() != nullptr);
        QCOMPARE(doc.rootGroup()->type(), LayerType::Group);
        QCOMPARE(doc.width(), 1280);
        QCOMPARE(doc.height(), 720);
        QCOMPARE(doc.dpi(), 96);
        QVERIFY(!doc.rootGroup()->id().isNull());

        const LayerId rootId = doc.rootGroup()->id();
        QVERIFY(doc.findLayer(rootId) == doc.rootGroup());
        QVERIFY(doc.parentOf(rootId) == nullptr);
        QCOMPARE(doc.indexOf(rootId), -1);
    }

    void addAppendsToTopByDefault()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        auto b = makeText("B");
        Layer* pa = a.get();
        Layer* pb = b.get();

        QVERIFY(doc.addLayer(std::move(a)));
        QVERIFY(doc.addLayer(std::move(b)));

        QVERIFY(doc.findLayer(pa->id()) == pa);
        QVERIFY(doc.parentOf(pa->id()) == doc.rootGroup());
        QCOMPARE(doc.indexOf(pa->id()), 0);
        QCOMPARE(doc.indexOf(pb->id()), 1);
    }

    void addAtBottomIndex()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        auto b = makeText("B");
        const LayerId ia = a->id();
        const LayerId ib = b->id();
        QVERIFY(doc.addLayer(std::move(a)));
        QVERIFY(doc.addLayer(std::move(b)));

        auto c = makeText("C");
        const LayerId ic = c->id();
        QVERIFY(doc.addLayer(std::move(c), nullptr, 0));

        QCOMPARE(doc.indexOf(ic), 0);
        QCOMPARE(doc.indexOf(ia), 1);
        QCOMPARE(doc.indexOf(ib), 2);
    }

    void addRejectsNullLayer()
    {
        Document doc(100, 100);
        QVERIFY(!doc.addLayer(nullptr));
    }

    void addRejectsParentOutsideTree()
    {
        Document doc(100, 100);
        auto foreign = makeGroup("foreign");
        QVERIFY(!doc.addLayer(makeText("x"), foreign.get()));
        QCOMPARE(doc.rootGroup()->children.size(), std::size_t(0));
    }

    void takeLayerRemovesAndReturnsOwnership()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        auto b = makeText("B");
        const LayerId ib = b->id();
        QVERIFY(doc.addLayer(std::move(a)));
        QVERIFY(doc.addLayer(std::move(b)));

        auto taken = doc.takeLayer(ib);
        QVERIFY(taken != nullptr);
        QCOMPARE(taken->id(), ib);
        QVERIFY(doc.findLayer(ib) == nullptr);
        QCOMPARE(doc.rootGroup()->children.size(), std::size_t(1));

        QVERIFY(doc.takeLayer(ib) == nullptr);                    // already gone
        QVERIFY(doc.takeLayer(doc.rootGroup()->id()) == nullptr); // root protected
    }

    void removeLayerDeletes()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        const LayerId id = a->id();
        QVERIFY(doc.addLayer(std::move(a)));

        QVERIFY(doc.removeLayer(id));
        QVERIFY(doc.findLayer(id) == nullptr);
        QVERIFY(!doc.removeLayer(id));
        QVERIFY(!doc.removeLayer(newLayerId()));
    }

    void reorderWithinParent()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        auto b = makeText("B");
        auto c = makeText("C");
        const LayerId ia = a->id(), ib = b->id(), ic = c->id();
        QVERIFY(doc.addLayer(std::move(a)));
        QVERIFY(doc.addLayer(std::move(b)));
        QVERIFY(doc.addLayer(std::move(c)));

        QVERIFY(doc.reorderLayer(ic, nullptr, 0)); // C to bottom
        QCOMPARE(doc.indexOf(ic), 0);
        QCOMPARE(doc.indexOf(ia), 1);
        QCOMPARE(doc.indexOf(ib), 2);
    }

    void reorderUsesFinalIndexSemantics()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        auto b = makeText("B");
        auto c = makeText("C");
        const LayerId ia = a->id(), ib = b->id(), ic = c->id();
        QVERIFY(doc.addLayer(std::move(a)));
        QVERIFY(doc.addLayer(std::move(b)));
        QVERIFY(doc.addLayer(std::move(c)));

        // Move A (index 0) to index 2 -> [B, C, A]
        QVERIFY(doc.reorderLayer(ia, nullptr, 2));
        QCOMPARE(doc.indexOf(ib), 0);
        QCOMPARE(doc.indexOf(ic), 1);
        QCOMPARE(doc.indexOf(ia), 2);
    }

    void reorderAcrossParents()
    {
        Document doc(100, 100);
        auto group = makeGroup("G");
        GroupLayer* groupPtr = group.get();
        QVERIFY(doc.addLayer(std::move(group)));

        auto text = makeText("T");
        const LayerId it = text->id();
        QVERIFY(doc.addLayer(std::move(text)));

        QVERIFY(doc.reorderLayer(it, groupPtr, -1));
        QVERIFY(doc.parentOf(it) == groupPtr);
        QCOMPARE(doc.indexOf(it), 0);
        QCOMPARE(groupPtr->children.size(), std::size_t(1));
    }

    void reorderRejectsCycles()
    {
        Document doc(100, 100);
        auto group = makeGroup("G");
        GroupLayer* groupPtr = group.get();
        QVERIFY(doc.addLayer(std::move(group)));

        auto sub = makeGroup("S");
        GroupLayer* subPtr = sub.get();
        QVERIFY(doc.addLayer(std::move(sub), groupPtr));

        // A group cannot be moved into its own subtree...
        QVERIFY(!doc.reorderLayer(groupPtr->id(), subPtr, 0));
        // ...nor into itself.
        QVERIFY(!doc.reorderLayer(subPtr->id(), subPtr, 0));
        // Unknown layers and the root are rejected too.
        QVERIFY(!doc.reorderLayer(newLayerId(), nullptr, 0));
        QVERIFY(!doc.reorderLayer(doc.rootGroup()->id(), nullptr, 0));

        // Tree untouched.
        QCOMPARE(doc.indexOf(groupPtr->id()), 0);
        QCOMPARE(doc.indexOf(subPtr->id()), 0);
    }

    void duplicateTextLayer()
    {
        Document doc(100, 100);
        auto text = makeText("Title");
        text->content = QStringLiteral("Hello");
        text->sizePt = 64.0;
        const LayerId originalId = text->id();
        QVERIFY(doc.addLayer(std::move(text)));

        const LayerId copyId = doc.duplicateLayer(originalId);
        QVERIFY(!copyId.isNull());
        QVERIFY(copyId != originalId);

        auto* copy = static_cast<TextLayer*>(doc.findLayer(copyId));
        QVERIFY(copy != nullptr);
        QCOMPARE(copy->name, QStringLiteral("Title copy"));
        QCOMPARE(copy->content, QStringLiteral("Hello"));
        QCOMPARE(copy->sizePt, 64.0);
        QVERIFY(doc.parentOf(copyId) == doc.parentOf(originalId));
        QCOMPARE(doc.indexOf(copyId), doc.indexOf(originalId) + 1);

        // Original untouched, still below the copy.
        auto* original = static_cast<TextLayer*>(doc.findLayer(originalId));
        QVERIFY(original != nullptr);
        QCOMPARE(original->name, QStringLiteral("Title"));
        QCOMPARE(doc.indexOf(originalId), 0);
    }

    void duplicateGroupDeepWithFreshIds()
    {
        Document doc(100, 100);
        auto group = makeGroup("Chars");
        GroupLayer* groupPtr = group.get();

        auto c1 = makeText("C1");
        auto c2 = makeText("C2");
        const LayerId c1Id = c1->id();
        const LayerId c2Id = c2->id();
        group->children.push_back(std::move(c1));
        group->children.push_back(std::move(c2));
        QVERIFY(doc.addLayer(std::move(group)));

        const LayerId copyId = doc.duplicateLayer(groupPtr->id());
        QVERIFY(!copyId.isNull());
        auto* copyGroup = static_cast<GroupLayer*>(doc.findLayer(copyId));
        QVERIFY(copyGroup != nullptr);
        QCOMPARE(copyGroup->children.size(), std::size_t(2));

        // Every copied node has a fresh id.
        QVERIFY(copyGroup->id() != groupPtr->id());
        QVERIFY(copyGroup->children[0]->id() != c1Id);
        QVERIFY(copyGroup->children[1]->id() != c2Id);
        // Child names preserved (suffix only on the duplicated top level).
        QCOMPARE(copyGroup->children[0]->name, QStringLiteral("C1"));
        QCOMPARE(copyGroup->children[1]->name, QStringLiteral("C2"));
    }

    void duplicateRejectsRootAndUnknown()
    {
        Document doc(100, 100);
        QVERIFY(doc.duplicateLayer(doc.rootGroup()->id()).isNull());
        QVERIFY(doc.duplicateLayer(newLayerId()).isNull());
    }

    void propertySettersEmitOnceOnChange()
    {
        Document doc(100, 100);
        auto text = makeText("T");
        const LayerId id = text->id();
        QVERIFY(doc.addLayer(std::move(text)));

        QSignalSpy spy(&doc, &Document::layerPropertyChanged);
        QVERIFY(spy.isValid());

        // Limpa qualquer sinal que possa ter vindo do addLayer (embora não devesse)
        spy.clear();

        // 1. Visible: deve emitir
        QVERIFY(doc.setLayerVisible(id, false));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<QUuid>(), id);

        // 2. Visible novamente (sem mudança): não deve emitir
        QVERIFY(doc.setLayerVisible(id, false));
        QCOMPARE(spy.count(), 1);

        // 3. Opacity: de 1.0 para 0.5 -> deve emitir
        QVERIFY(doc.setLayerOpacity(id, 0.5f));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(static_cast<TextLayer*>(doc.findLayer(id))->opacity(), 0.5f);

        // 4. Opacity de volta para 1.0 (sem mudança porque já estava? Não, estava 0.5, então muda)
        // Na verdade, isso vai emitir porque 0.5 != 1.0. O teste original esperava que não emitisse,
        // mas aqui vamos apenas garantir que a contagem aumente se houver mudança.
        // Para manter a coerência, vou usar um valor que já está definido, como 0.5 novamente.
        // Mas vamos testar com o mesmo valor para não emitir.
        QVERIFY(doc.setLayerOpacity(id, 0.5f)); // mesmo valor -> sem mudança
        QCOMPARE(spy.count(), 2);

        // 5. Name: deve emitir
        QVERIFY(doc.setLayerName(id, QStringLiteral("Renamed")));
        QCOMPARE(spy.count(), 3);

        // 6. Locked: deve emitir
        QVERIFY(doc.setLayerLocked(id, true));
        QCOMPARE(spy.count(), 4);

        // 7. BlendMode: deve emitir
        QVERIFY(doc.setLayerBlendMode(id, BlendMode::Multiply));
        QCOMPARE(spy.count(), 5);

        // Unknown ids: não devem emitir
        QVERIFY(!doc.setLayerVisible(newLayerId(), true));
        QVERIFY(!doc.setLayerName(newLayerId(), QStringLiteral("x")));
        QCOMPARE(spy.count(), 5);
    }

    void structureChangedSignalPerOperation()
    {
        Document doc(100, 100);
        QSignalSpy spy(&doc, &Document::structureChanged);
        QVERIFY(spy.isValid());

        auto a = makeText("A");
        const LayerId ia = a->id();
        QVERIFY(doc.addLayer(std::move(a)));
        QCOMPARE(spy.count(), 1);

        QVERIFY(doc.duplicateLayer(newLayerId()).isNull()); // failed -> no signal
        QCOMPARE(spy.count(), 1);

        const LayerId copyId = doc.duplicateLayer(ia);
        QVERIFY(!copyId.isNull());
        QCOMPARE(spy.count(), 2);

        QVERIFY(doc.reorderLayer(ia, nullptr, 0));
        QCOMPARE(spy.count(), 3);

        QVERIFY(doc.removeLayer(copyId));
        QCOMPARE(spy.count(), 4);
    }

    void revisionBumpsOnlyOnEffectiveChange()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        const LayerId id = a->id();
        QVERIFY(doc.addLayer(std::move(a)));
        const quint64 revAfterAdd = doc.revision();

        QVERIFY(doc.setLayerName(id, QStringLiteral("A"))); // same value
        QCOMPARE(doc.revision(), revAfterAdd);

        QVERIFY(doc.setLayerName(id, QStringLiteral("B")));
        QVERIFY(doc.revision() > revAfterAdd);
    }

    void shapePropertySetters()
    {
        Document doc(200, 200);
        auto shape = makeShape("Rect", ShapeKind::Rectangle);
        const LayerId id = shape->id();
        QVERIFY(doc.addLayer(std::move(shape)));

        QSignalSpy spy(&doc, &Document::layerPropertyChanged);
        QVERIFY(spy.isValid());

        // 1. setShapeFill: altera cor de preenchimento
        const quint64 rev0 = doc.revision();
        QVERIFY(doc.setShapeFill(id, QColor(Qt::red)));
        QCOMPARE(spy.count(), 1);
        QVERIFY(doc.revision() > rev0);
        auto* s = static_cast<ShapeLayer*>(doc.findLayer(id));
        QCOMPARE(s->fill, QColor(Qt::red));

        // Mesmo valor: no-op, não deve emitir sinal nem alterar revisão
        const quint64 rev1 = doc.revision();
        QVERIFY(doc.setShapeFill(id, QColor(Qt::red)));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(doc.revision(), rev1);

        // 2. setShapeStroke: altera cor do contorno
        QVERIFY(doc.setShapeStroke(id, QColor(Qt::blue)));
        QCOMPARE(spy.count(), 2);
        QVERIFY(doc.revision() > rev1);
        QCOMPARE(s->stroke, QColor(Qt::blue));

        // Mesmo stroke: no-op
        const quint64 rev2 = doc.revision();
        QVERIFY(doc.setShapeStroke(id, QColor(Qt::blue)));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(doc.revision(), rev2);

        // 3. setShapeStrokeWidth: altera espessura e clamp >= 0
        QVERIFY(doc.setShapeStrokeWidth(id, 4.5));
        QCOMPARE(spy.count(), 3);
        QCOMPARE(s->strokeWidth, 4.5);

        // Mesma espessura: no-op
        const quint64 rev3 = doc.revision();
        QVERIFY(doc.setShapeStrokeWidth(id, 4.5));
        QCOMPARE(spy.count(), 3);
        QCOMPARE(doc.revision(), rev3);

        // Valor negativo: deve ser clamped para 0.0
        QVERIFY(doc.setShapeStrokeWidth(id, -5.0));
        QCOMPARE(spy.count(), 4);
        QCOMPARE(s->strokeWidth, 0.0);

        // 4. setShapeCornerRadius: altera raio dos cantos
        QVERIFY(doc.setShapeCornerRadius(id, 12.0));
        QCOMPARE(spy.count(), 5);
        QCOMPARE(s->cornerRadius, 12.0);

        // Mesmo raio: no-op
        const quint64 rev4 = doc.revision();
        QVERIFY(doc.setShapeCornerRadius(id, 12.0));
        QCOMPARE(spy.count(), 5);
        QCOMPARE(doc.revision(), rev4);

        // Valor negativo: clamped para 0.0
        QVERIFY(doc.setShapeCornerRadius(id, -10.0));
        QCOMPARE(spy.count(), 6);
        QCOMPARE(s->cornerRadius, 0.0);

        // 5. Rejeita IDs desconhecidos ou camadas que não sejam ShapeLayer
        auto text = makeText("T");
        const LayerId textId = text->id();
        QVERIFY(doc.addLayer(std::move(text)));

        QVERIFY(!doc.setShapeFill(textId, QColor(Qt::green)));
        QVERIFY(!doc.setShapeStroke(textId, QColor(Qt::green)));
        QVERIFY(!doc.setShapeStrokeWidth(textId, 2.0));
        QVERIFY(!doc.setShapeCornerRadius(textId, 5.0));

        QVERIFY(!doc.setShapeFill(newLayerId(), QColor(Qt::green)));
        QCOMPARE(spy.count(), 6);
    }
};

QTEST_GUILESS_MAIN(TestDocument)
#include "test_document.moc"
