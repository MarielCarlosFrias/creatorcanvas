#include <QTest>
#include <QObject>
#include <QImage>
#include <QColor>
#include "ui/mockupdialog.h"
#include "core/Document.h"
#include "core/layers/Layer.h"

class TestMockupDialog : public QObject
{
    Q_OBJECT

private slots:
    void testProcessLogoDisabled();
    void testProcessLogoBackgroundRemovalAndCrop();
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

    QImage cleaned = cc::MockupDialog::processLogo(src, /*removeBackground=*/true);
    QVERIFY(!cleaned.isNull());

    // O fundo branco deve ter sido removido e a imagem recortada para o quadrado azul
    // O tamanho resultante deve ser aproximadamente 20x20
    QVERIFY(cleaned.width() <= 22 && cleaned.width() >= 18);
    QVERIFY(cleaned.height() <= 22 && cleaned.height() >= 18);

    // O pixel central do resultado deve ser azul opaco
    QColor centerColor = cleaned.pixelColor(cleaned.width() / 2, cleaned.height() / 2);
    QCOMPARE(centerColor.blue(), 255);
    QCOMPARE(centerColor.alpha(), 255);
}

void TestMockupDialog::testDialogCreation()
{
    cc::Document doc(800, 600);
    auto layer = std::make_unique<cc::ImageLayer>();
    cc::LayerId id = layer->id();
    doc.addLayer(std::move(layer));

    cc::MockupDialog dialog(nullptr, &doc, id);
    QCOMPARE(dialog.windowTitle(), QStringLiteral("Escolher imagem para o mockup"));
}

QTEST_MAIN(TestMockupDialog)
#include "test_mockup_dialog.moc"
