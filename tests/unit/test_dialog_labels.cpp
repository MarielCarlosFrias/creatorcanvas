#include <QtTest>
#include <QTemporaryDir>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include "core/Document.h"
#include "localization/i18nservice.h"
#include "services/settingsservice.h"
#include "ui/imagesizedialog.h"
#include "ui/refineedgedialog.h"

namespace {
// NUNCA SettingsService(":memory:"): o QSettings grava um FICHEIRO real com
// esse nome no directorio de trabalho (mais um .lock), o que suja a build e
// pode bloquear corridas seguintes num lock preso. Usa um tmp dir.
QString testSettingsFile()
{
    static QTemporaryDir dir;
    return dir.filePath(QStringLiteral("settings.ini"));
}
} // namespace

using namespace cc;

class TestDialogLabels final : public QObject
{
    Q_OBJECT

private slots:
    void imageSizeDialogUsesTranslatedLabels()
    {
        SettingsService settings(testSettingsFile());
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        Document doc(1332, 749);
        ImageSizeDialog dialog(&doc, &i18n);

        QVERIFY(!dialog.windowTitle().contains(QStringLiteral("imageSize")));
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Image Size"));

        // As labels não têm objectName; valida-se pelo texto agregado de
        // labels e checkboxes.
        QString all;
        for (const QLabel* l : dialog.findChildren<QLabel*>())
            all += l->text() + QLatin1Char('\n');
        for (const QCheckBox* c : dialog.findChildren<QCheckBox*>())
            all += c->text() + QLatin1Char('\n');
        QVERIFY(!all.contains(QStringLiteral("common.imageSize")));
        QVERIFY(all.contains(QStringLiteral("Width:")));
        QVERIFY(all.contains(QStringLiteral("Height:")));
        QVERIFY(all.contains(QStringLiteral("Current size: 1332")));
        QVERIFY(all.contains(QStringLiteral("Constrain proportions")));
        QVERIFY(all.contains(QStringLiteral("Resample:")));

        auto* combo = dialog.findChild<QComboBox*>(QStringLiteral("resampleComboBox"));
        QVERIFY(combo);
        QString items;
        for (int i = 0; i < combo->count(); ++i)
            items += combo->itemText(i) + QLatin1Char('\n');
        QVERIFY(items.contains(QStringLiteral("Bicubic")));
        QVERIFY(!combo->currentText().contains(QStringLiteral("imageSize")));
        QVERIFY(!items.contains(QStringLiteral("imageSize")));
    }

    void imageSizeDialogTranslatesToPortuguese()
    {
        SettingsService settings(testSettingsFile());
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());
        i18n.setLanguage(QStringLiteral("pt-BR"));

        Document doc(800, 600);
        ImageSizeDialog dialog(&doc, &i18n);
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Tamanho da Imagem"));
        QString all;
        for (const QLabel* l : dialog.findChildren<QLabel*>())
            all += l->text() + QLatin1Char('\n');
        for (const QCheckBox* c : dialog.findChildren<QCheckBox*>())
            all += c->text() + QLatin1Char('\n');
        QVERIFY(all.contains(QStringLiteral("Largura:")));
        QVERIFY(all.contains(QStringLiteral("Manter proporções")));
        QVERIFY(!all.contains(QStringLiteral("imageSize")));
    }

    void refineEdgeDialogButtonsAreTranslated()
    {
        SettingsService settings(testSettingsFile());
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());
        // Explícito: o ficheiro de settings é partilhado no processo e o teste
        // anterior mudou o idioma.
        QVERIFY(i18n.setLanguage(QStringLiteral("en")));

        QImage selection(64, 48, QImage::Format_Alpha8);
        selection.fill(0);
        for (int y = 8; y < 40; ++y)
            for (int x = 8; x < 56; ++x)
                selection.scanLine(y)[x] = 255;

        RefineEdgeDialog dialog(selection, &i18n);
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Refine Selection Edge - CreatorCanvas"));
        const auto buttons = dialog.findChildren<QPushButton*>();
        QCOMPARE(buttons.size(), 2);
        QString all;
        for (const QPushButton* b : buttons)
            all += b->text() + QLatin1Char('\n');
        QVERIFY(all.contains(QStringLiteral("Cancel")));
        QVERIFY(all.contains(QStringLiteral("Apply")));
        QVERIFY(!all.contains(QStringLiteral("Cancelar")));

        // O resultado final continua à resolução original.
        QCOMPARE(dialog.refinedSelection().size(), selection.size());
    }
};

QTEST_MAIN(TestDialogLabels)
#include "test_dialog_labels.moc"
