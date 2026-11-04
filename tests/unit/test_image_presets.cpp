#include <QTest>
#include <QObject>
#include "core/image/ImagePresets.h"

class TestImagePresets : public QObject
{
    Q_OBJECT

private slots:
    void testIsPixelFilter();
    void testGetPixelFilter();
    void testGetSliderPresetValues();
    void testPresetNames();
};

void TestImagePresets::testIsPixelFilter()
{
    for (int i = 0; i <= 7; ++i) {
        QVERIFY(!cc::ImagePresets::isPixelFilter(i));
    }
    for (int i = 8; i <= 10; ++i) {
        QVERIFY(cc::ImagePresets::isPixelFilter(i));
    }
    QVERIFY(!cc::ImagePresets::isPixelFilter(11));
    QVERIFY(!cc::ImagePresets::isPixelFilter(-1));
}

void TestImagePresets::testGetPixelFilter()
{
    auto f8 = cc::ImagePresets::getPixelFilter(8);
    QVERIFY(f8.has_value());
    QCOMPARE(*f8, cc::ImageProcessing::PresetFilter::Invert);

    auto f9 = cc::ImagePresets::getPixelFilter(9);
    QVERIFY(f9.has_value());
    QCOMPARE(*f9, cc::ImageProcessing::PresetFilter::Posterize);

    auto f10 = cc::ImagePresets::getPixelFilter(10);
    QVERIFY(f10.has_value());
    QCOMPARE(*f10, cc::ImageProcessing::PresetFilter::Threshold);

    QVERIFY(!cc::ImagePresets::getPixelFilter(0).has_value());
}

void TestImagePresets::testGetSliderPresetValues()
{
    auto p0 = cc::ImagePresets::getSliderPreset(0);
    QCOMPARE(p0.saturation, -100);
    QCOMPARE(p0.contrast, 10);
    QCOMPARE(p0.brightness, 0);

    auto p1 = cc::ImagePresets::getSliderPreset(1);
    QCOMPARE(p1.saturation, -50);
    QCOMPARE(p1.temperature, 40);
    QCOMPARE(p1.contrast, 5);

    auto p5 = cc::ImagePresets::getSliderPreset(5); // Cyberpunk
    QCOMPARE(p5.contrast, 35);
    QCOMPARE(p5.saturation, 30);
    QCOMPARE(p5.temperature, -40);
    QCOMPARE(p5.sharpen, 10);

    auto p7 = cc::ImagePresets::getSliderPreset(7); // Noir
    QCOMPARE(p7.saturation, -100);
    QCOMPARE(p7.contrast, 60);
    QCOMPARE(p7.brightness, -10);
    QCOMPARE(p7.sharpen, 25);
}

void TestImagePresets::testPresetNames()
{
    QCOMPARE(cc::ImagePresets::presetName(0), QStringLiteral("P&B"));
    QCOMPARE(cc::ImagePresets::presetName(1), QStringLiteral("Sépia"));
    QCOMPARE(cc::ImagePresets::presetName(8), QStringLiteral("Invert"));
    QCOMPARE(cc::ImagePresets::presetName(99), QStringLiteral("Custom"));
}

QTEST_MAIN(TestImagePresets)
#include "test_image_presets.moc"
