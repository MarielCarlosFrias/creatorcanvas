#include "core/Document.h"
#include "core/layers/ToneAdjustments.h"
#include "core/serialization/ProjectFile.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace cc;

class TestPhotocraft final : public QObject
{
    Q_OBJECT

private slots:
    void pixelSelectionRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("selection.creatorcanvas"));

        Document doc(32, 24);
        QImage selection(32, 24, QImage::Format_Alpha8);
        selection.fill(0);
        selection.scanLine(9)[7] = 255;
        selection.scanLine(18)[20] = 128;
        doc.setPixelSelection(selection);

        QString error;
        QVERIFY(saveDocument(doc, path, &error));
        auto loaded = loadDocument(path, &error);
        QVERIFY2(loaded != nullptr, qPrintable(error));
        QCOMPARE(loaded->pixelSelection().size(), selection.size());
        QCOMPARE(loaded->pixelSelection().constScanLine(9)[7], uchar(255));
        QCOMPARE(loaded->pixelSelection().constScanLine(18)[20], uchar(128));
    }

    void layerCompsAndWebSlicesRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("metadata.creatorcanvas"));

        Document doc(1280, 720);
        auto layer = std::make_unique<TextLayer>();
        layer->content = QStringLiteral("Comp layer");
        const LayerId layerId = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        LayerComp comp;
        comp.id = QStringLiteral("thumbnail-state");
        comp.name = QStringLiteral("Thumbnail state");
        comp.comment = QStringLiteral("Visible title variant");
        CompLayerState state;
        state.layerId = layerId;
        state.visible = true;
        state.position = QPointF(42.0, 84.0);
        state.opacity = 0.75f;
        state.blendMode = BlendMode::Screen;
        comp.states.push_back(state);
        doc.addLayerComp(comp);

        WebSlice slice;
        slice.id = QStringLiteral("hero-export");
        slice.name = QStringLiteral("Hero export");
        slice.rect = QRectF(10.0, 20.0, 640.0, 360.0);
        slice.kind = SliceKind::NoImage;
        slice.exportFormat = QStringLiteral("webp");
        doc.addWebSlice(slice);

        QString error;
        QVERIFY(saveDocument(doc, path, &error));
        auto loaded = loadDocument(path, &error);
        QVERIFY2(loaded != nullptr, qPrintable(error));

        QCOMPARE(loaded->layerComps().size(), std::size_t(1));
        const LayerComp& loadedComp = loaded->layerComps().front();
        QCOMPARE(loadedComp.id, comp.id);
        QCOMPARE(loadedComp.name, comp.name);
        QCOMPARE(loadedComp.comment, comp.comment);
        QCOMPARE(loadedComp.states.size(), std::size_t(1));
        QVERIFY(loadedComp.states.front() == state);

        QCOMPARE(loaded->webSlices().size(), std::size_t(1));
        QVERIFY(loaded->webSlices().front() == slice);
    }

    void toneAdjustmentsIdentityAndCurve()
    {
        LevelsChannel identity;
        QVERIFY(identity.isIdentity());

        LevelsChannel adjusted;
        adjusted.gamma = 1.4f;
        QVERIFY(!adjusted.isIdentity());
        QVERIFY(ToneCurve::evaluateLevels(adjusted, 0.5f) > 0.0f);

        const auto lut = ToneCurve::buildCurveLut({
            CurvePoint{0.0f, 0.0f},
            CurvePoint{0.5f, 0.8f},
            CurvePoint{1.0f, 1.0f}});
        QCOMPARE(static_cast<int>(lut.size()), ToneCurve::LUT_SIZE);
        QVERIFY(lut.front() == 0);
        QVERIFY(lut.back() == 255);
        QVERIFY(lut[128] > 128);
    }
};

QTEST_MAIN(TestPhotocraft)
#include "test_photocraft.moc"
