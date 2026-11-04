#include <QTest>
#include <QObject>
#include <QImage>
#include <QColor>
#include "ui/mockupdialog.h"
#include "core/Document.h"
#include "core/layers/Layer.h"
#include "core/image/ImageProcessing.h"

class TestMockupDialog : public QObject
{
    Q_OBJECT

private slots:
    void testProcessLogoDisabled();
    void testProcessLogoBackgroundRemovalAndCrop();
    void testProcessLogoTolerance();
    void testDialogCreation();
};

void TestMockupDialog::testProcessLogoDisabled()
{
    QImage src(50, 50, QImage::Format_ARGB32);
    src.fill(Qt::red);

    QImage result = cc::MockupDialog::processLogo(src, /*removeBackground=*/false);
    QCOMPARE(result, src);
}

void TestMockupDialog::testProcessLogoBackgroundRemovalAndCrop()
{
    // Criar imagem 100x100 com fundo branco uniforme
    QImage src(100, 100, QImage::Format_ARGB32);
    src.fill(Qt::white);

    // Pintar um quadrado azul de 20x20 no centro (40, 40 até 60, 60)
    for (int y = 40; y < 60; ++y) {
        for (int x = 40; x < 60; ++x) {
            src.setPixelColor(x, y, Qt::blue);
        }
    }

    QImage cleaned = cc::MockupDialog::processLogo(src, /*removeBackground=*/true, 18);
    QVERIFY(!cleaned.isNull());

    // O fundo branco deve ter sido removido e a imagem recortada para o quadrado azul
    QVERIFY(cleaned.width() <= 22 && cleaned.width() >= 18);
    QVERIFY(cleaned.height() <= 22 && cleaned.height() >= 18);

    // O pixel central do resultado deve ser azul opaco
    QColor centerColor = cleaned.pixelColor(cleaned.width() / 2, cleaned.height() / 2);
    QCOMPARE(centerColor.blue(), 255);
    QCOMPARE(centerColor.alpha(), 255);
}

void TestMockupDialog::testProcessLogoTolerance()
{
    // Fundo quase branco (245, 245, 245) com quadrado vermelho
    QImage src(60, 60, QImage::Format_ARGB32);
    src.fill(QColor(245, 245, 245));

    for (int y = 20; y < 40; ++y) {
        for (int x = 20; x < 40; ++x) {
            src.setPixelColor(x, y, Qt::red);
        }
    }

    // Com tolerância normal (18%), remove o fundo
    QImage cleanedNormal = cc::ImageProcessing::prepareMockupLogo(src, true, 18);
    QVERIFY(!cleanedNormal.isNull());
    QVERIFY(cleanedNormal.width() < 60);

    // O pixel central continua vermelho
    QColor centerColor = cleanedNormal.pixelColor(cleanedNormal.width() / 2, cleanedNormal.height() / 2);
    QCOMPARE(centerColor.red(), 255);
}

void TestMockupDialog::testDialogCreation()
{
    cc::Document doc(800, 600);
    auto layer = std::make_unique<cc::ImageLayer>();
    cc::LayerId id = layer->id();
    doc.addLayer(std::move(layer));

    cc::MockupDialog dialog(nullptr, &doc, id, nullptr);
    QVERIFY(!dialog.windowTitle().isEmpty());
}

QTEST_MAIN(TestMockupDialog)
#include "test_mockup_dialog.moc"
