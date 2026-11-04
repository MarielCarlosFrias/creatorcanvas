#include <QtTest>
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
};

QTEST_MAIN(TestExportDialog)
#include "test_export_dialog.moc"
