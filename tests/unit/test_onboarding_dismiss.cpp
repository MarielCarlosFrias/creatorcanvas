#include <QtTest>
#include <QPushButton>
#include <QTemporaryDir>

#include "localization/i18nservice.h"
#include "services/settingsservice.h"
#include "ui/onboardingpanel.h"

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

/// O painel "Primeiros passos" tem de ter saida definitiva: fechar pelo
/// botao (ou pela dock) persiste e nao volta a aparecer.
class TestOnboardingDismiss final : public QObject
{
    Q_OBJECT

private slots:
    void dismissButtonEmitsAndPersists()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsService settings(dir.filePath("settings.ini"));
        QVERIFY(!settings.onboardingDismissed());

        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        OnboardingPanel panel(&i18n);
        QSignalSpy spy(&panel, &OnboardingPanel::dismissed);
        auto* button = panel.findChild<QPushButton*>();
        QVERIFY(button);
        QCOMPARE(button->text(), QStringLiteral("Skip for now"));

        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);

        // O MainWindow persiste; aqui simulamos o que ele faz com o sinal.
        settings.setOnboardingDismissed(true);
        QVERIFY(settings.onboardingDismissed());

        SettingsService reloaded(dir.filePath("settings.ini"));
        QVERIFY(reloaded.onboardingDismissed());
    }

    void panelStillTracksSteps()
    {
        SettingsService settings(testSettingsFile());
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        OnboardingPanel panel(&i18n);
        panel.setSteps({true, true, false, false});
        QVERIFY(!panel.allDone());
        panel.setSteps({true, true, true, true});
        QVERIFY(panel.allDone());
    }
};

QTEST_MAIN(TestOnboardingDismiss)
#include "test_onboarding_dismiss.moc"
