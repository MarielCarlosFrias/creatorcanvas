#include <QtTest>

#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>

#include "core/Document.h"
#include "core/commands/CanvasCommands.h"
#include "core/commands/ImageSizeCommands.h"
#include "core/history/CommandStack.h"
#include "services/ImageResizer.h"
#include "ui/imagesizedialog.h"

using namespace cc;

namespace {

QImage colorFixture()
{
    QImage image(2, 2, QImage::Format_ARGB32);
    image.setPixelColor(0, 0, Qt::red);
    image.setPixelColor(1, 0, Qt::green);
    image.setPixelColor(0, 1, Qt::blue);
    image.setPixelColor(1, 1, Qt::white);
    return image;
}

QImage maskFixture(const QSize& size, uchar value = 0)
{
    QImage mask(size, QImage::Format_Alpha8);
    mask.fill(value);
    return mask;
}

std::unique_ptr<ImageLayer> imageLayer(const LayerId& assetId,
                                       const QPointF& position = QPointF(1, 1))
{
    auto layer = std::make_unique<ImageLayer>();
    layer->assetId = assetId;
    layer->naturalWidth = 2;
    layer->naturalHeight = 2;
    layer->transform.position = position;
    return layer;
}

} // namespace

class TestImageSize final : public QObject
{
    Q_OBJECT

private slots:
    void resampleMethodsAndMasks();
    void resizeCommandUndoRedoRestoresEverything();
    void nestedLayersAndSharedAssetsAreNotScaledTwice();
    void invalidResizeDoesNotMutateDocument();
    void imageSizeIsSeparateFromCanvasSize();
    void dialogConstrainsProportionsAndReturnsOptions();
};

void TestImageSize::resampleMethodsAndMasks()
{
    const QImage source = colorFixture();
    for (const ResampleMethod method : {ResampleMethod::NearestNeighbor,
                                        ResampleMethod::Bilinear,
                                        ResampleMethod::Bicubic}) {
        const QImage result = ImageResizer::resample(source, QSize(4, 4), method);
        QCOMPARE(result.size(), QSize(4, 4));
        QVERIFY(!result.isNull());
    }

    const QImage nearest = ImageResizer::resample(
        source, QSize(4, 4), ResampleMethod::NearestNeighbor);
    QCOMPARE(nearest.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(nearest.pixelColor(3, 0), QColor(Qt::green));
    QCOMPARE(nearest.pixelColor(0, 3), QColor(Qt::blue));
    QCOMPARE(nearest.pixelColor(3, 3), QColor(Qt::white));

    QImage constant(3, 3, QImage::Format_ARGB32);
    constant.fill(QColor(20, 40, 60, 80));
    const QImage cubic = ImageResizer::resample(
        constant, QSize(7, 5), ResampleMethod::Bicubic);
    const QColor cubicPixel = cubic.pixelColor(3, 2);
    QCOMPARE(cubicPixel.alpha(), 80);
    QVERIFY(qAbs(cubicPixel.red() - 20) <= 1);
    QVERIFY(qAbs(cubicPixel.green() - 40) <= 1);
    QVERIFY(qAbs(cubicPixel.blue() - 60) <= 1);

    QImage mask = maskFixture(QSize(2, 2));
    mask.scanLine(0)[0] = 255;
    const QImage scaledMask = ImageResizer::resampleMask(
        mask, QSize(5, 3), ResampleMethod::Bicubic);
    QCOMPARE(scaledMask.size(), QSize(5, 3));
    QCOMPARE(scaledMask.format(), QImage::Format_Alpha8);
    QVERIFY(ImageResizer::resampleMask(mask, QSize(), ResampleMethod::Bilinear).isNull());
}

void TestImageSize::resizeCommandUndoRedoRestoresEverything()
{
    Document document(4, 2);
    const LayerId oldAsset = document.assets().addImage(colorFixture());
    auto layer = imageLayer(oldAsset);
    const LayerId layerId = layer->id();
    layer->alphaMask = maskFixture(QSize(2, 2));
    layer->alphaMask.scanLine(0)[0] = 255;
    layer->alphaMask.scanLine(1)[1] = 128;
    layer->vectorMask = maskFixture(QSize(2, 2), 200);
    const QImage oldAlphaMask = layer->alphaMask;
    const QImage oldVectorMask = layer->vectorMask;
    const AffineTransform oldTransform = layer->transform;
    QVERIFY(document.addLayer(std::move(layer)));

    QImage previous = maskFixture(QSize(4, 2), 7);
    previous.scanLine(1)[3] = 90;
    document.setPixelSelection(previous, SelectionSpace::Document);
    QImage active = maskFixture(QSize(4, 2));
    active.scanLine(1)[3] = 200;
    document.setPixelSelection(active, SelectionSpace::Document);
    QImage savedDocument = maskFixture(QSize(4, 2));
    savedDocument.scanLine(0)[1] = 111;
    const QUuid savedDocumentId = document.addSavedSelection(
        QStringLiteral("Document"), savedDocument, SelectionSpace::Document);
    QImage savedLayer = maskFixture(QSize(2, 2));
    savedLayer.scanLine(1)[1] = 123;
    const QUuid savedLayerId = document.addSavedSelection(
        QStringLiteral("Layer"), savedLayer, SelectionSpace::Layer);

    document.addGuide(Guide{true, 1.0});
    document.addGuide(Guide{false, 3.0});
    WebSlice slice;
    slice.rect = QRectF(1, 0, 2, 2);
    document.addWebSlice(slice);
    const auto oldGuides = document.guides();
    const auto oldSlices = document.webSlices();

    CommandStack history;
    auto command = std::make_unique<ResizeImageCommand>(
        document, 8, 4, ResampleMethod::NearestNeighbor);
    ResizeImageCommand* rawCommand = command.get();
    history.execute(std::move(command));
    QVERIFY(rawCommand->wasApplied());
    QCOMPARE(history.nextUndoName(), QStringLiteral("image.resize"));

    QCOMPARE(document.width(), 8);
    QCOMPARE(document.height(), 4);
    const auto* resizedLayer = static_cast<const ImageLayer*>(document.findLayer(layerId));
    QVERIFY(resizedLayer);
    const LayerId newAsset = resizedLayer->assetId;
    QVERIFY(newAsset != oldAsset);
    QCOMPARE(resizedLayer->naturalWidth, 4);
    QCOMPARE(resizedLayer->naturalHeight, 4);
    QCOMPARE(resizedLayer->transform.position, QPointF(2, 2));
    QCOMPARE(resizedLayer->transform.scaleX, oldTransform.scaleX);
    QCOMPARE(resizedLayer->transform.scaleY, oldTransform.scaleY);
    QCOMPARE(document.assets().decodedImage(newAsset).size(), QSize(4, 4));
    QCOMPARE(document.assets().decodedImage(newAsset).pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(document.assets().decodedImage(newAsset).pixelColor(3, 3), QColor(Qt::white));
    QCOMPARE(resizedLayer->alphaMask.size(), QSize(4, 4));
    QCOMPARE(resizedLayer->alphaMask.format(), QImage::Format_Alpha8);
    QCOMPARE(resizedLayer->vectorMask.size(), QSize(4, 4));
    QVERIFY(document.assets().find(oldAsset));

    QCOMPARE(document.pixelSelection().size(), QSize(8, 4));
    QCOMPARE(document.pixelSelection().constScanLine(2)[6], uchar(200));
    QCOMPARE(document.previousPixelSelection().size(), QSize(8, 4));
    QCOMPARE(document.previousPixelSelection().constScanLine(2)[6], uchar(90));
    QCOMPARE(document.findSavedSelection(savedDocumentId)->mask.size(), QSize(8, 4));
    QCOMPARE(document.findSavedSelection(savedDocumentId)->space, SelectionSpace::Document);
    QCOMPARE(document.findSavedSelection(savedLayerId)->mask.size(), QSize(4, 4));
    QCOMPARE(document.findSavedSelection(savedLayerId)->space, SelectionSpace::Layer);
    QCOMPARE(document.guides().at(0), (Guide{true, 2.0}));
    QCOMPARE(document.guides().at(1), (Guide{false, 6.0}));
    QCOMPARE(document.webSlices().at(0).rect, QRectF(2, 0, 4, 4));
    QCOMPARE(document.assets().assets().size(), 2);

    history.undo();
    QCOMPARE(document.width(), 4);
    QCOMPARE(document.height(), 2);
    const auto* restored = static_cast<const ImageLayer*>(document.findLayer(layerId));
    QCOMPARE(restored->assetId, oldAsset);
    QCOMPARE(restored->naturalWidth, 2);
    QCOMPARE(restored->naturalHeight, 2);
    QCOMPARE(restored->transform, oldTransform);
    QVERIFY(restored->alphaMask == oldAlphaMask);
    QVERIFY(restored->vectorMask == oldVectorMask);
    QVERIFY(document.pixelSelection() == active);
    QVERIFY(document.previousPixelSelection() == previous);
    QVERIFY(document.findSavedSelection(savedDocumentId)->mask == savedDocument);
    QVERIFY(document.findSavedSelection(savedLayerId)->mask == savedLayer);
    QVERIFY(document.guides() == oldGuides);
    QVERIFY(document.webSlices() == oldSlices);

    history.redo();
    const auto* redone = static_cast<const ImageLayer*>(document.findLayer(layerId));
    QCOMPARE(redone->assetId, newAsset); // não reamostra nem cria outro asset
    QVERIFY(redone->alphaMask.size() == QSize(4, 4));
    QVERIFY(document.pixelSelection().size() == QSize(8, 4));
    QCOMPARE(document.assets().assets().size(), 2);

    history.undo();
    history.redo();
    QCOMPARE(static_cast<const ImageLayer*>(document.findLayer(layerId))->assetId,
             newAsset);
    QCOMPARE(document.assets().assets().size(), 2);

    QSet<LayerId> retained;
    rawCommand->collectAssetIds(retained);
    QVERIFY(retained.contains(oldAsset));
    QVERIFY(retained.contains(newAsset));
}

void TestImageSize::nestedLayersAndSharedAssetsAreNotScaledTwice()
{
    Document document(8, 4);
    const LayerId asset = document.assets().addImage(colorFixture());
    auto group = std::make_unique<GroupLayer>();
    const LayerId groupId = group->id();
    group->transform.position = QPointF(2, 1);
    group->transform.scaleX = 1.5;
    group->transform.scaleY = 0.75;
    auto child = imageLayer(asset);
    const LayerId childId = child->id();
    child->transform.scaleX = 2.0;
    group->children.push_back(std::move(child));
    QVERIFY(document.addLayer(std::move(group)));

    auto shared = imageLayer(asset, QPointF(4, 2));
    const LayerId sharedId = shared->id();
    QVERIFY(document.addLayer(std::move(shared)));
    auto text = std::make_unique<TextLayer>();
    const LayerId textId = text->id();
    text->transform.position = QPointF(2, 1);
    QVERIFY(document.addLayer(std::move(text)));

    QVERIFY(ImageResizer::resize(document, QSize(16, 8),
                                 ResampleMethod::NearestNeighbor));
    const Layer* resizedGroup = document.findLayer(groupId);
    const auto* resizedChild = static_cast<const ImageLayer*>(document.findLayer(childId));
    const auto* resizedShared = static_cast<const ImageLayer*>(document.findLayer(sharedId));
    const Layer* resizedText = document.findLayer(textId);
    QCOMPARE(resizedGroup->transform.position, QPointF(4, 2));
    QCOMPARE(resizedGroup->transform.scaleX, 1.5);
    QCOMPARE(resizedGroup->transform.scaleY, 0.75);
    QCOMPARE(resizedChild->transform.position, QPointF(2, 2));
    QCOMPARE(resizedChild->transform.scaleX, 2.0);
    QCOMPARE(resizedChild->naturalWidth, 4);
    QCOMPARE(resizedChild->naturalHeight, 4);
    QCOMPARE(resizedShared->assetId, resizedChild->assetId);
    QCOMPARE(document.assets().assets().size(), 2);
    QCOMPARE(resizedText->transform.position, QPointF(4, 2));
    QCOMPARE(resizedText->transform.scaleX, 2.0);
    QCOMPARE(resizedText->transform.scaleY, 2.0);
}

void TestImageSize::invalidResizeDoesNotMutateDocument()
{
    Document document(4, 4);
    auto broken = std::make_unique<ImageLayer>();
    broken->assetId = QUuid::createUuid();
    broken->naturalWidth = 2;
    broken->naturalHeight = 2;
    const LayerId id = broken->id();
    const AffineTransform transform = broken->transform;
    QVERIFY(document.addLayer(std::move(broken)));
    const quint64 revision = document.revision();

    QString error;
    QVERIFY(!ImageResizer::resize(document, QSize(8, 8),
                                  ResampleMethod::Bicubic, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(document.width(), 4);
    QCOMPARE(document.height(), 4);
    QCOMPARE(document.findLayer(id)->transform, transform);
    QCOMPARE(document.revision(), revision);
    QVERIFY(!ImageResizer::resize(document, QSize(0, 8),
                                  ResampleMethod::Bilinear, &error));
    QVERIFY(!ImageResizer::resize(document, QSize(4, 4),
                                  ResampleMethod::NearestNeighbor, &error));
}

void TestImageSize::imageSizeIsSeparateFromCanvasSize()
{
    Document document(4, 2);
    const LayerId asset = document.assets().addImage(colorFixture());
    auto layer = imageLayer(asset);
    const LayerId id = layer->id();
    QVERIFY(document.addLayer(std::move(layer)));

    CommandStack canvasHistory;
    canvasHistory.execute(std::make_unique<ResizeCanvasCommand>(
        document, 8, 4, QPointF()));
    const auto* afterCanvas = static_cast<const ImageLayer*>(document.findLayer(id));
    QCOMPARE(afterCanvas->assetId, asset);
    QCOMPARE(afterCanvas->naturalWidth, 2);
    QCOMPARE(afterCanvas->naturalHeight, 2);

    CommandStack imageHistory;
    imageHistory.execute(std::make_unique<ImageSizeCommand>(
        document, 16, 8, ResampleMethod::NearestNeighbor));
    const auto* afterImage = static_cast<const ImageLayer*>(document.findLayer(id));
    QVERIFY(afterImage->assetId != asset);
    QCOMPARE(afterImage->naturalWidth, 4); // Canvas Size anterior não reamostra
    QCOMPARE(afterImage->naturalHeight, 4);
}

void TestImageSize::dialogConstrainsProportionsAndReturnsOptions()
{
    Document document(400, 200);
    ImageSizeDialog dialog(&document);
    auto* width = dialog.findChild<QSpinBox*>(QStringLiteral("widthSpinBox"));
    auto* height = dialog.findChild<QSpinBox*>(QStringLiteral("heightSpinBox"));
    auto* constrain = dialog.findChild<QCheckBox*>(
        QStringLiteral("constrainProportionsCheckBox"));
    auto* resample = dialog.findChild<QComboBox*>(QStringLiteral("resampleComboBox"));
    QVERIFY(width && height && constrain && resample);
    QCOMPARE(dialog.windowTitle(), QStringLiteral("Image Size"));
    QCOMPARE(dialog.imageSize(), QSize(400, 200));

    width->setValue(200);
    QCOMPARE(height->value(), 100);
    height->setValue(50);
    QCOMPARE(width->value(), 100);

    constrain->setChecked(false);
    width->setValue(300);
    QCOMPARE(height->value(), 50);
    resample->setCurrentIndex(resample->findData(
        static_cast<int>(ResampleMethod::NearestNeighbor)));
    QCOMPARE(dialog.options().size, QSize(300, 50));
    QCOMPARE(dialog.options().method, ResampleMethod::NearestNeighbor);
}

QTEST_MAIN(TestImageSize)
#include "test_image_size.moc"
