#include "core/Document.h"
#include "core/commands/SmartFilterCommands.h"
#include "core/history/CommandStack.h"
#include "core/image/ImageProcessing.h"
#include "core/serialization/ProjectFile.h"
#include "rendering/CanvasRenderer.h"
#include "ui/smartfilterdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>

using namespace cc;

namespace {

QImage sourceImage(const QSize& size = QSize(40, 32))
{
    QImage image(size, QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixel(x, y, qRgba((x * 17 + y * 3) % 256,
                                       (y * 19 + x * 5) % 256,
                                       (x * 7 + y * 11) % 256, 255));
        }
    }
    return image;
}

LayerId addImageLayer(Document& doc, const QImage& image)
{
    auto layer = std::make_unique<ImageLayer>();
    layer->assetId = doc.assets().addImage(image, QStringLiteral("png"));
    layer->naturalWidth = image.width();
    layer->naturalHeight = image.height();
    layer->transform.position = QPointF(doc.width() / 2.0, doc.height() / 2.0);
    const LayerId id = layer->id();
    if (!doc.addLayer(std::move(layer)))
        return {};
    return id;
}

QImage renderDocumentImage(const Document& doc)
{
    QImage output(QSize(doc.width(), doc.height()), QImage::Format_ARGB32);
    output.fill(Qt::black);
    QPainter painter(&output);
    RenderOptions options;
    options.drawCheckerboard = false;
    renderDocument(doc, &painter, QTransform(), options);
    painter.end();
    return output;
}

} // namespace

class TestSmartFilters final : public QObject
{
    Q_OBJECT

private slots:
    void appendsTypesWithoutChangingStoredValues()
    {
        QCOMPARE(static_cast<int>(AdjustmentType::Vignette), 12);
        QCOMPARE(static_cast<int>(AdjustmentType::Blur), 13);
        QCOMPARE(static_cast<int>(AdjustmentType::MotionBlur), 14);
        QCOMPARE(static_cast<int>(AdjustmentType::Noise), 15);
        QCOMPARE(static_cast<int>(AdjustmentType::DistortTwirl), 16);
        QCOMPARE(static_cast<int>(AdjustmentType::Pinch), 17);
        QCOMPARE(static_cast<int>(AdjustmentType::Ripple), 18);

        for (AdjustmentType type : {AdjustmentType::Blur, AdjustmentType::MotionBlur,
                                    AdjustmentType::Noise, AdjustmentType::DistortTwirl,
                                    AdjustmentType::Pinch, AdjustmentType::Ripple}) {
            QVERIFY(isSmartFilterType(type));
        }
        QVERIFY(!isSmartFilterType(AdjustmentType::Vignette));
        QVERIFY(!isSmartFilterType(AdjustmentType::BrightnessContrast));
    }

    void engineUsesImageProcessingAndSerializedParams()
    {
        const QImage source = sourceImage();
        auto check = [&](AdjustmentType type, const AdjustmentParams& params,
                         const QImage& expected) {
            const QImage actual = applyAdjustmentParameters(source, type, params);
            QCOMPARE(actual.size(), source.size());
            QCOMPARE(actual, expected);
            QCOMPARE(applyAdjustmentParameters(source, type, params, 0.0f), source);
        };

        check(AdjustmentType::Blur, {{QStringLiteral("radius"), 3.0}},
              ImageProcessing::applyBlur(source, 3.0));
        check(AdjustmentType::MotionBlur,
              {{QStringLiteral("angle"), 30.0}, {QStringLiteral("length"), 12.0}},
              ImageProcessing::applyMotionBlur(source, 30.0, 12.0));
        check(AdjustmentType::Noise,
              {{QStringLiteral("amount"), 35.0}, {QStringLiteral("monochrome"), 1.0}},
              ImageProcessing::applyNoise(source, 35.0, true));
        check(AdjustmentType::DistortTwirl, {{QStringLiteral("amount"), -0.65}},
              ImageProcessing::applyDistort(source, ImageProcessing::DistortMode::Twirl,
                                            -0.65));
        check(AdjustmentType::Pinch, {{QStringLiteral("amount"), 0.4}},
              ImageProcessing::applyDistort(source, ImageProcessing::DistortMode::Pinch,
                                            0.4));
        check(AdjustmentType::Ripple,
              {{QStringLiteral("amount"), 0.7}, {QStringLiteral("wavelength"), 9.0}},
              ImageProcessing::applyDistort(source, ImageProcessing::DistortMode::Ripple,
                                            0.7, 9.0));

        // Defaults idênticos aos antigos prompts destrutivos.
        QCOMPARE(applyAdjustmentParameters(source, AdjustmentType::Blur, {}),
                 ImageProcessing::applyBlur(source, 5.0));
        QCOMPARE(applyAdjustmentParameters(source, AdjustmentType::MotionBlur, {}),
                 ImageProcessing::applyMotionBlur(source, 0.0, 15.0));
        QCOMPARE(applyAdjustmentParameters(source, AdjustmentType::Noise, {}),
                 ImageProcessing::applyNoise(source, 10.0, false));
        QCOMPARE(applyAdjustmentParameters(source, AdjustmentType::DistortTwirl, {}),
                 ImageProcessing::applyDistort(source, ImageProcessing::DistortMode::Twirl,
                                               0.5));
    }

    void dialogDefaultsToNonDestructiveAndCollectsParams()
    {
        SmartFilterDialog dialog(sourceImage(QSize(24, 24)), AdjustmentType::MotionBlur);
        SmartFilterRequest request = dialog.request();
        QVERIFY(request.valid);
        QCOMPARE(request.type, AdjustmentType::MotionBlur);
        QVERIFY(request.nonDestructive);
        QCOMPARE(adjustmentParam(request.params, QStringLiteral("angle"), -1.0), 0.0);
        QCOMPARE(adjustmentParam(request.params, QStringLiteral("length"), -1.0), 15.0);
        QVERIFY(!dialog.previewImage().isNull());

        auto* length = dialog.findChild<QDoubleSpinBox*>(
            QStringLiteral("smartFilter.motionBlur.length"));
        QVERIFY(length);
        length->setValue(27.5);
        request = dialog.request();
        QCOMPARE(adjustmentParam(request.params, QStringLiteral("length"), -1.0), 27.5);

        auto* nonDestructive = dialog.findChild<QCheckBox*>(
            QStringLiteral("smartFilterNonDestructive"));
        QVERIFY(nonDestructive);
        nonDestructive->setChecked(false);
        QVERIFY(!dialog.request().nonDestructive);

        auto* typeCombo = dialog.findChild<QComboBox*>(QStringLiteral("smartFilterType"));
        QVERIFY(typeCombo);
        const int rippleIndex = typeCombo->findData(static_cast<int>(AdjustmentType::Ripple));
        QVERIFY(rippleIndex >= 0);
        typeCombo->setCurrentIndex(rippleIndex);
        auto* amount = dialog.findChild<QDoubleSpinBox*>(
            QStringLiteral("smartFilter.ripple.amount"));
        auto* wavelength = dialog.findChild<QDoubleSpinBox*>(
            QStringLiteral("smartFilter.ripple.wavelength"));
        QVERIFY(amount && wavelength);
        amount->setValue(-0.25);
        wavelength->setValue(13.0);
        request = dialog.request();
        QCOMPARE(request.type, AdjustmentType::Ripple);
        QCOMPARE(adjustmentParam(request.params, QStringLiteral("amount"), 0.0), -0.25);
        QCOMPARE(adjustmentParam(request.params, QStringLiteral("wavelength"), 0.0), 13.0);

        dialog.accept();
        QCOMPARE(dialog.result(), QDialog::Accepted);
    }

    void nonDestructiveApplyUndoRedoAndRender()
    {
        QImage source(32, 24, QImage::Format_ARGB32);
        source.fill(QColor(100, 120, 140, 255));
        Document doc(source.width(), source.height());
        const LayerId imageId = addImageLayer(doc, source);
        QVERIFY(!imageId.isNull());
        const auto* image = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        QVERIFY(image);
        const LayerId originalAsset = image->assetId;

        CommandStack history;
        const AdjustmentParams params{{QStringLiteral("amount"), 30.0},
                                      {QStringLiteral("monochrome"), 1.0}};
        QVERIFY(executeApplySmartFilter(doc, &history, imageId, AdjustmentType::Noise,
                                        params));
        image = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        QVERIFY(image && image->adjustmentLayerId.has_value());
        QCOMPARE(image->assetId, originalAsset);
        const AdjustmentLayer* adjustment =
            doc.findAdjustmentLayer(*image->adjustmentLayerId);
        QVERIFY(adjustment);
        QCOMPARE(adjustment->kind, AdjustmentType::Noise);
        QVERIFY(adjustment->parameters == params);

        const QImage rendered = renderDocumentImage(doc);
        const QImage expected = ImageProcessing::applyNoise(source, 30.0, true);
        QCOMPARE(rendered.pixelColor(11, 7), QColor::fromRgba(expected.pixel(11, 7)));

        history.undo();
        image = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        QVERIFY(image && !image->adjustmentLayerId.has_value());
        QCOMPARE(image->assetId, originalAsset);
        QCOMPARE(renderDocumentImage(doc).pixelColor(11, 7), source.pixelColor(11, 7));

        history.redo();
        image = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        QVERIFY(image && image->adjustmentLayerId.has_value());
        adjustment = doc.findAdjustmentLayer(*image->adjustmentLayerId);
        QVERIFY(adjustment && adjustment->kind == AdjustmentType::Noise);
        QCOMPARE(renderDocumentImage(doc).pixelColor(11, 7),
                 QColor::fromRgba(expected.pixel(11, 7)));
        clearRendererCaches();
    }

    void destructiveApplyBakesAssetAndUndoRedoRestoresIds()
    {
        const QImage source = sourceImage(QSize(32, 24));
        Document doc(source.width(), source.height());
        const LayerId imageId = addImageLayer(doc, source);
        const auto* image = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        QVERIFY(image);
        const LayerId originalAsset = image->assetId;

        CommandStack history;
        const AdjustmentParams params{{QStringLiteral("angle"), 90.0},
                                      {QStringLiteral("length"), 9.0}};
        QVERIFY(executeApplySmartFilter(doc, &history, imageId, AdjustmentType::MotionBlur,
                                        params, false));
        image = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        QVERIFY(image);
        const LayerId filteredAsset = image->assetId;
        QVERIFY(filteredAsset != originalAsset);
        QVERIFY(!image->adjustmentLayerId.has_value());
        QCOMPARE(doc.assets().decodedImage(filteredAsset),
                 ImageProcessing::applyMotionBlur(source, 90.0, 9.0));

        history.undo();
        image = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        QCOMPARE(image->assetId, originalAsset);
        history.redo();
        image = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        QCOMPARE(image->assetId, filteredAsset);
    }

    void deepCopyPersistsTypeSourceAndParameters()
    {
        AdjustmentLayer original;
        original.kind = AdjustmentType::Ripple;
        original.sourceLayerId = QUuid::createUuid();
        original.parameters = {{QStringLiteral("amount"), -0.4},
                               {QStringLiteral("wavelength"), 18.0}};

        std::unique_ptr<Layer> copyBase = original.deepCopy();
        const auto* copy = dynamic_cast<const AdjustmentLayer*>(copyBase.get());
        QVERIFY(copy);
        QCOMPARE(copy->kind, AdjustmentType::Ripple);
        QVERIFY(copy->sourceLayerId == original.sourceLayerId);
        QVERIFY(copy->parameters == original.parameters);
        QCOMPARE(adjustmentParam(copy->parameters, QStringLiteral("wavelength"), 0.0), 18.0);
    }

    void projectRoundTripPersistsTypeAndParameters()
    {
        Document doc(32, 24);
        const LayerId imageId = addImageLayer(doc, sourceImage(QSize(32, 24)));
        QVERIFY(!imageId.isNull());
        const LayerId adjustmentId = doc.createAdjustmentLayer(AdjustmentType::Ripple, imageId);
        QVERIFY(!adjustmentId.isNull());
        QVERIFY(doc.setLayerAdjustmentLayerId(imageId, adjustmentId));
        const AdjustmentParams params{{QStringLiteral("amount"), 0.65},
                                      {QStringLiteral("wavelength"), 12.0}};
        QVERIFY(doc.setAdjustmentParameters(adjustmentId, params));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("smart-filter.creatorcanvas"));
        QString error;
        QVERIFY2(saveDocument(doc, path, &error), qPrintable(error));
        std::unique_ptr<Document> loaded = loadDocument(path, &error);
        QVERIFY2(loaded != nullptr, qPrintable(error));
        const AdjustmentLayer* adjustment = loaded->findAdjustmentLayer(adjustmentId);
        QVERIFY(adjustment);
        QCOMPARE(adjustment->kind, AdjustmentType::Ripple);
        QVERIFY(adjustment->parameters == params);
        QVERIFY(adjustment->sourceLayerId.has_value());
        QCOMPARE(*adjustment->sourceLayerId, imageId);
        const auto* loadedImage = dynamic_cast<const ImageLayer*>(loaded->findLayer(imageId));
        QVERIFY(loadedImage && loadedImage->adjustmentLayerId.has_value());
        QCOMPARE(*loadedImage->adjustmentLayerId, adjustmentId);

        const QImage source = sourceImage(QSize(32, 24));
        QCOMPARE(applyAdjustmentParameters(source, adjustment->kind, adjustment->parameters),
                 ImageProcessing::applyDistort(source, ImageProcessing::DistortMode::Ripple,
                                               0.65, 12.0));
    }

    void rejectsUnsupportedTypesAndNonImageLayers()
    {
        Document doc(16, 16);
        const QImage source = sourceImage(QSize(16, 16));
        const LayerId imageId = addImageLayer(doc, source);
        CommandStack history;
        QVERIFY(!executeApplySmartFilter(doc, &history, imageId,
                                         AdjustmentType::BrightnessContrast, {}));
        QCOMPARE(history.undoCount(), 0);

        auto text = std::make_unique<TextLayer>();
        const LayerId textId = text->id();
        QVERIFY(doc.addLayer(std::move(text)));
        QVERIFY(!executeApplySmartFilter(doc, &history, textId, AdjustmentType::Blur, {}));
        QCOMPARE(history.undoCount(), 0);
    }
};

QTEST_MAIN(TestSmartFilters)
#include "test_smart_filters.moc"
