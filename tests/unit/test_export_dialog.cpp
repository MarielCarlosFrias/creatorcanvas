#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>

#include "ui/exportdialog.h"
#include "core/Document.h"
#include "localization/i18nservice.h"
#include "services/settingsservice.h"

using namespace cc;

class TestExportDialog : public QObject
{
    Q_OBJECT

private slots:
    void testPresetsAndChipsSynchronization()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        i18n.loadAvailableLanguages();

        Document doc(1920, 1080, 96);
        ExportDialog dlg(&doc, &i18n);

        // Inicialmente deve estar no preset original (escala 1.0)
        ExportSettings initialSettings = dlg.settings();
        QCOMPARE(initialSettings.scale, 1.0);

        // Aplica preset Instagram Post (1080x1080)
        dlg.applyPresetById(QStringLiteral("ig_post"));
        ExportSettings igSettings = dlg.settings();
        QCOMPARE(igSettings.format, QStringLiteral("jpeg"));
        QCOMPARE(igSettings.quality, 90);
        // 1080 / 1920 = 0.5625
        QVERIFY(qAbs(igSettings.scale - (1080.0 / 1920.0)) < 0.05);

        // Aplica preset YouTube Thumbnail (1280x720)
        dlg.applyPresetById(QStringLiteral("yt_thumb"));
        ExportSettings ytSettings = dlg.settings();
        QCOMPARE(ytSettings.format, QStringLiteral("jpeg"));
        QCOMPARE(ytSettings.quality, 92);
        // 1280 / 1920 = ~0.666
        QVERIFY(qAbs(ytSettings.scale - (1280.0 / 1920.0)) < 0.05);

        // Aplica preset Story (1080x1920)
        dlg.applyPresetById(QStringLiteral("story"));
        ExportSettings storySettings = dlg.settings();
        QCOMPARE(storySettings.format, QStringLiteral("png"));
        QVERIFY(qAbs(storySettings.scale - (1080.0 / 1920.0)) < 0.05);

        // Restaura preset Original (100%)
        dlg.applyPresetById(QStringLiteral("original"));
        ExportSettings restoredSettings = dlg.settings();
        QCOMPARE(restoredSettings.scale, 1.0);
    }

    void testTransparencyFollowsFormat()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        i18n.loadAvailableLanguages();

        Document doc(640, 480, 96);
        ExportDialog dlg(&doc, &i18n);

        // PNG (initial): transparência ligada por omissão.
        QCOMPARE(dlg.settings().transparent, true);

        // JPEG não tem alfa: o checkbox fica desabilitado e o settings
        // reflecte "sem transparência".
        QComboBox* format = nullptr;
        for (QComboBox* combo : dlg.findChildren<QComboBox*>())
        {
            if (combo->findData(QStringLiteral("png")) >= 0)
            {
                format = combo; // o combo de formato (o outro é a escala)
                break;
            }
        }
        QVERIFY(format != nullptr);
        QCheckBox* transparent = nullptr;
        for (QCheckBox* box : dlg.findChildren<QCheckBox*>())
            transparent = box;
        QVERIFY(transparent != nullptr);

        const int pngIndex = format->findData(QStringLiteral("png"));
        const int jpegIndex = format->findData(QStringLiteral("jpeg"));
        QVERIFY(pngIndex >= 0);
        QVERIFY(jpegIndex >= 0);

        format->setCurrentIndex(jpegIndex);
        QCOMPARE(transparent->isEnabled(), false);
        QCOMPARE(transparent->isChecked(), false);
        QCOMPARE(dlg.settings().transparent, false);

        // Voltar a PNG restaura a intenção do utilizador.
        format->setCurrentIndex(pngIndex);
        QCOMPARE(transparent->isEnabled(), true);
        QCOMPARE(dlg.settings().transparent, true);

        // Desligar a transparência é honrado pelos settings.
        transparent->setChecked(false);
        QCOMPARE(dlg.settings().transparent, false);

        // ...e sobrevive a um ida e volta pelo JPEG.
        format->setCurrentIndex(jpegIndex);
        QCOMPARE(dlg.settings().transparent, false);
        format->setCurrentIndex(pngIndex);
        QCOMPARE(dlg.settings().transparent, false);
    }
};

QTEST_MAIN(TestExportDialog)
#include "test_export_dialog.moc"
