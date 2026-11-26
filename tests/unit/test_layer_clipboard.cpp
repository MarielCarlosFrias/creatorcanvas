#include <QtTest>

#include "core/history/CommandStack.h"
#include "services/LayerClipboard.h"

using namespace cc;

class TestLayerClipboard final : public QObject
{
    Q_OBJECT

private slots:
    void emptyCaptureAndPaste();
    void pasteMultipleLayersAndAssetsAcrossDocuments();
    void capturesLinkedAdjustmentAutomaticallyWithoutDuplicates();
    void preservesGroupAndRemapsAdjustmentLinks();
    void groupSelectionDoesNotDuplicateChildren();
    void sameDocumentPasteRemapsIdsAssetsAndUndoRedo();
    void undoRedoPaste();
};

static LayerId addImage(Document& doc, const QColor& color, const QString& name)
{
    QImage bitmap(12, 10, QImage::Format_ARGB32);
    bitmap.fill(color);
    auto layer = std::make_unique<ImageLayer>();
    layer->name = name;
    layer->assetId = doc.assets().addImage(bitmap);
    layer->naturalWidth = bitmap.width();
    layer->naturalHeight = bitmap.height();
    layer->transform.position = QPointF(50, 40);
    const LayerId id = layer->id();
    doc.addLayer(std::move(layer));
    return id;
}

void TestLayerClipboard::emptyCaptureAndPaste()
{
    Document source(100, 100);
    LayerClipboard clipboard;
    QVERIFY(!clipboard.capture(source, {}));
    QVERIFY(clipboard.isEmpty());
    Document target(100, 100);
    CommandStack history;
    QVERIFY(clipboard.paste(target, &history).isEmpty());
    QVERIFY(target.rootGroup()->children.empty());
}

void TestLayerClipboard::pasteMultipleLayersAndAssetsAcrossDocuments()
{
    Document source(200, 150);
    const LayerId redId = addImage(source, QColor(200, 20, 20), QStringLiteral("red"));
    const LayerId blueId = addImage(source, QColor(20, 20, 200), QStringLiteral("blue"));
    const LayerId oldAsset = static_cast<ImageLayer*>(source.findLayer(redId))->assetId;

    LayerClipboard clipboard;
    QVERIFY(clipboard.capture(source, {redId, blueId}));
    QCOMPARE(clipboard.layerCount(), 2);

    Document target(200, 150);
    CommandStack history;
    const QList<LayerId> pasted = clipboard.paste(target, &history);
    QCOMPARE(pasted.size(), 2);
    QCOMPARE(target.rootGroup()->children.size(), size_t(2));

    const auto* red = dynamic_cast<const ImageLayer*>(target.findLayer(pasted.first()));
    QVERIFY(red != nullptr);
    QVERIFY(!red->assetId.isNull());
    QVERIFY(red->assetId != oldAsset); // remapeado, não pendurado no source
    QCOMPARE(target.assets().decodedImage(red->assetId).pixelColor(2, 2),
             QColor(200, 20, 20));
    QCOMPARE(red->name, QStringLiteral("red copy"));
}

void TestLayerClipboard::capturesLinkedAdjustmentAutomaticallyWithoutDuplicates()
{
    Document source(100, 100);
    const LayerId imageId = addImage(source, QColor(30, 90, 150), QStringLiteral("image"));
    const LayerId oldAsset = static_cast<ImageLayer*>(source.findLayer(imageId))->assetId;
    const LayerId adjId = source.createAdjustmentLayer(AdjustmentType::Invert, imageId);
    QVERIFY(source.setLayerAdjustmentLayerId(imageId, adjId));

    LayerClipboard clipboard;
    QVERIFY(clipboard.capture(source, {imageId}));
    QCOMPARE(clipboard.layerCount(), 2); // imagem + ajuste incluído automaticamente

    Document target(100, 100);
    const QList<LayerId> automatic = clipboard.paste(target, nullptr);
    QCOMPARE(automatic.size(), 2);
    QCOMPARE(target.rootGroup()->children.size(), size_t(2));

    const auto* firstImage = dynamic_cast<const ImageLayer*>(target.findLayer(automatic.at(0)));
    const auto* firstAdj = dynamic_cast<const AdjustmentLayer*>(target.findLayer(automatic.at(1)));
    QVERIFY(firstImage != nullptr);
    QVERIFY(firstAdj != nullptr);
    QVERIFY(firstImage->id() != imageId);
    QVERIFY(firstAdj->id() != adjId);
    QVERIFY(firstImage->assetId != oldAsset);
    QVERIFY(!target.assets().decodedImage(firstImage->assetId).isNull());
    QVERIFY(firstImage->adjustmentLayerId.has_value());
    QCOMPARE(*firstImage->adjustmentLayerId, firstAdj->id());
    QVERIFY(firstAdj->sourceLayerId.has_value());
    QCOMPARE(*firstAdj->sourceLayerId, firstImage->id());

    // Selecionar explicitamente o ajuste não pode criar uma segunda cópia.
    QVERIFY(clipboard.capture(source, {imageId, adjId}));
    QCOMPARE(clipboard.layerCount(), 2);
    const QList<LayerId> explicitSelection = clipboard.paste(target, nullptr);
    QCOMPARE(explicitSelection.size(), 2);
    QCOMPARE(target.rootGroup()->children.size(), size_t(4));

    const auto* secondImage = dynamic_cast<const ImageLayer*>(
        target.findLayer(explicitSelection.at(0)));
    const auto* secondAdj = dynamic_cast<const AdjustmentLayer*>(
        target.findLayer(explicitSelection.at(1)));
    QVERIFY(secondImage != nullptr);
    QVERIFY(secondAdj != nullptr);
    QVERIFY(secondImage->id() != firstImage->id());
    QVERIFY(secondAdj->id() != firstAdj->id());
    QVERIFY(secondImage->assetId != firstImage->assetId);
    QVERIFY(secondImage->adjustmentLayerId.has_value());
    QCOMPARE(*secondImage->adjustmentLayerId, secondAdj->id());
    QVERIFY(secondAdj->sourceLayerId.has_value());
    QCOMPARE(*secondAdj->sourceLayerId, secondImage->id());
}

void TestLayerClipboard::preservesGroupAndRemapsAdjustmentLinks()
{
    Document source(100, 100);
    auto group = std::make_unique<GroupLayer>();
    group->name = QStringLiteral("group");
    QImage bitmap(12, 12, QImage::Format_ARGB32);
    bitmap.fill(QColor(50, 60, 70));
    auto image = std::make_unique<ImageLayer>();
    image->name = QStringLiteral("image");
    image->assetId = source.assets().addImage(bitmap);
    image->naturalWidth = 12;
    image->naturalHeight = 12;
    const LayerId imageId = image->id();
    group->children.push_back(std::move(image));
    const LayerId groupId = group->id();
    source.addLayer(std::move(group));

    const LayerId adjId = source.createAdjustmentLayer(AdjustmentType::Invert, imageId);
    QVERIFY(source.setLayerAdjustmentLayerId(imageId, adjId));
    const auto* sourceImage = dynamic_cast<const ImageLayer*>(source.findLayer(imageId));
    QVERIFY(sourceImage != nullptr);
    QVERIFY(sourceImage->adjustmentLayerId.has_value());
    QCOMPARE(*sourceImage->adjustmentLayerId, adjId);

    LayerClipboard clipboard;
    QVERIFY(clipboard.capture(source, {groupId}));
    QCOMPARE(clipboard.layerCount(), 2); // o ajuste externo ligado vem junto
    Document target(100, 100);
    const QList<LayerId> pasted = clipboard.paste(target, nullptr);
    QCOMPARE(pasted.size(), 2);

    const auto* newGroup = dynamic_cast<const GroupLayer*>(target.findLayer(pasted.first()));
    QVERIFY(newGroup != nullptr);
    QCOMPARE(newGroup->children.size(), size_t(1));
    const auto* newImage = dynamic_cast<const ImageLayer*>(newGroup->children.front().get());
    QVERIFY(newImage != nullptr);
    QVERIFY(newImage->adjustmentLayerId.has_value());
    const auto* newAdj = target.findAdjustmentLayer(*newImage->adjustmentLayerId);
    QVERIFY(newAdj != nullptr);
    QVERIFY(newAdj->sourceLayerId.has_value());
    QCOMPARE(*newAdj->sourceLayerId, newImage->id());
    QVERIFY(!target.assets().decodedImage(newImage->assetId).isNull());
}

void TestLayerClipboard::groupSelectionDoesNotDuplicateChildren()
{
    Document source(100, 100);
    const LayerId imageId = addImage(source, Qt::red, QStringLiteral("child"));
    auto group = std::make_unique<GroupLayer>();
    auto taken = source.takeLayer(imageId);
    group->children.push_back(std::move(taken));
    const LayerId groupId = group->id();
    source.addLayer(std::move(group));

    const LayerId adjId = source.createAdjustmentLayer(AdjustmentType::Invert, imageId);
    QVERIFY(source.setLayerAdjustmentLayerId(imageId, adjId));
    QVERIFY(source.reorderLayer(adjId, static_cast<GroupLayer*>(source.findLayer(groupId)), -1));

    LayerClipboard clipboard;
    QVERIFY(clipboard.capture(source, {groupId, imageId, adjId}));
    QCOMPARE(clipboard.layerCount(), 1); // grupo já inclui imagem e ajuste

    Document target(100, 100);
    const QList<LayerId> pasted = clipboard.paste(target, nullptr);
    QCOMPARE(pasted.size(), 1);
    QCOMPARE(target.rootGroup()->children.size(), size_t(1));
    const auto* newGroup = dynamic_cast<const GroupLayer*>(target.findLayer(pasted.first()));
    QVERIFY(newGroup != nullptr);
    QCOMPARE(newGroup->children.size(), size_t(2));
    const auto* newImage = dynamic_cast<const ImageLayer*>(newGroup->children.at(0).get());
    const auto* newAdj = dynamic_cast<const AdjustmentLayer*>(newGroup->children.at(1).get());
    QVERIFY(newImage != nullptr);
    QVERIFY(newAdj != nullptr);
    QVERIFY(newImage->adjustmentLayerId.has_value());
    QCOMPARE(*newImage->adjustmentLayerId, newAdj->id());
    QVERIFY(newAdj->sourceLayerId.has_value());
    QCOMPARE(*newAdj->sourceLayerId, newImage->id());
    QCOMPARE(target.parentOf(newAdj->id()), newGroup);
}

void TestLayerClipboard::sameDocumentPasteRemapsIdsAssetsAndUndoRedo()
{
    Document source(100, 100);
    const LayerId imageId = addImage(source, QColor(10, 140, 80), QStringLiteral("image"));
    const LayerId oldAsset = static_cast<ImageLayer*>(source.findLayer(imageId))->assetId;
    const LayerId adjId = source.createAdjustmentLayer(AdjustmentType::BrightnessContrast, imageId);
    QVERIFY(source.setLayerAdjustmentLayerId(imageId, adjId));

    LayerClipboard clipboard;
    QVERIFY(clipboard.capture(source, {imageId}));
    QCOMPARE(clipboard.layerCount(), 2);

    CommandStack history;
    const QList<LayerId> pasted = clipboard.paste(source, &history);
    QCOMPARE(pasted.size(), 2);
    QCOMPARE(source.rootGroup()->children.size(), size_t(4));
    QCOMPARE(history.undoCount(), 1);

    const auto* newImage = dynamic_cast<const ImageLayer*>(source.findLayer(pasted.at(0)));
    const auto* newAdj = dynamic_cast<const AdjustmentLayer*>(source.findLayer(pasted.at(1)));
    QVERIFY(newImage != nullptr);
    QVERIFY(newAdj != nullptr);
    QVERIFY(newImage->id() != imageId);
    QVERIFY(newAdj->id() != adjId);
    QVERIFY(newImage->assetId != oldAsset);
    QVERIFY(!source.assets().decodedImage(oldAsset).isNull());
    QVERIFY(!source.assets().decodedImage(newImage->assetId).isNull());
    QVERIFY(newImage->adjustmentLayerId.has_value());
    QCOMPARE(*newImage->adjustmentLayerId, newAdj->id());
    QVERIFY(newAdj->sourceLayerId.has_value());
    QCOMPARE(*newAdj->sourceLayerId, newImage->id());

    const auto* originalImage = dynamic_cast<const ImageLayer*>(source.findLayer(imageId));
    const auto* originalAdj = dynamic_cast<const AdjustmentLayer*>(source.findLayer(adjId));
    QVERIFY(originalImage != nullptr);
    QVERIFY(originalAdj != nullptr);
    QCOMPARE(originalImage->assetId, oldAsset);
    QVERIFY(originalImage->adjustmentLayerId.has_value());
    QCOMPARE(*originalImage->adjustmentLayerId, adjId);
    QVERIFY(originalAdj->sourceLayerId.has_value());
    QCOMPARE(*originalAdj->sourceLayerId, imageId);

    history.undo();
    QCOMPARE(source.rootGroup()->children.size(), size_t(2));
    QVERIFY(source.findLayer(pasted.at(0)) == nullptr);
    QVERIFY(source.findLayer(pasted.at(1)) == nullptr);
    QCOMPARE(history.redoCount(), 1);

    history.redo();
    QCOMPARE(source.rootGroup()->children.size(), size_t(4));
    newImage = dynamic_cast<const ImageLayer*>(source.findLayer(pasted.at(0)));
    newAdj = dynamic_cast<const AdjustmentLayer*>(source.findLayer(pasted.at(1)));
    QVERIFY(newImage != nullptr);
    QVERIFY(newAdj != nullptr);
    QVERIFY(!source.assets().decodedImage(newImage->assetId).isNull());
    QVERIFY(newImage->adjustmentLayerId.has_value());
    QCOMPARE(*newImage->adjustmentLayerId, newAdj->id());
    QVERIFY(newAdj->sourceLayerId.has_value());
    QCOMPARE(*newAdj->sourceLayerId, newImage->id());
    QCOMPARE(history.undoCount(), 1);
    QCOMPARE(history.redoCount(), 0);
}

void TestLayerClipboard::undoRedoPaste()
{
    Document source(100, 100);
    const LayerId id = addImage(source, Qt::green, QStringLiteral("green"));
    LayerClipboard clipboard;
    QVERIFY(clipboard.capture(source, {id}));

    Document target(100, 100);
    CommandStack history;
    const QList<LayerId> ids = clipboard.paste(target, &history);
    QCOMPARE(target.rootGroup()->children.size(), size_t(1));
    history.undo();
    QVERIFY(target.rootGroup()->children.empty());
    history.redo();
    QCOMPARE(target.rootGroup()->children.size(), size_t(1));
    const auto* image = dynamic_cast<const ImageLayer*>(target.findLayer(ids.first()));
    QVERIFY(image != nullptr);
    QVERIFY(!target.assets().decodedImage(image->assetId).isNull());
}

QTEST_GUILESS_MAIN(TestLayerClipboard)
#include "test_layer_clipboard.moc"
