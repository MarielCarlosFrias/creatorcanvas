#include <QtTest>
#include <QTemporaryDir>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

#include "localization/i18nservice.h"
#include "services/settingsservice.h"
#include "ui/onboardingpanel.h"
#include "ui/onboardingtour.h"
#include "ui/welcomedialog.h"

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

class TestOnboarding final : public QObject
{
    Q_OBJECT

private slots:
    void welcomeDialogPersistsLanguageAndFlag()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString ini = dir.filePath("settings.ini");
        SettingsService settings(ini);
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());
        QVERIFY(!settings.welcomeShown());

        WelcomeDialog dialog(&i18n, &settings);
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Welcome"));
        // O diálogo é traduzido: o corpo do texto existe.
        QVERIFY(!dialog.findChildren<QLabel*>().isEmpty());

        auto* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
        QVERIFY(ok);
        QTest::mouseClick(ok, Qt::LeftButton);

        QVERIFY(settings.welcomeShown());
        QCOMPARE(i18n.currentLanguage(), QStringLiteral("en"));
    }

    void panelTracksStepsAndDismisses()
    {
        SettingsService settings(testSettingsFile());
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        OnboardingPanel panel(&i18n);
        QSignalSpy spy(&panel, &OnboardingPanel::dismissed);

        panel.setSteps({true, false, false, false});
        QVERIFY(!panel.allDone());
        QCOMPARE(spy.count(), 0);

        panel.setSteps({true, true, true, false});
        QVERIFY(!panel.allDone());

        panel.setSteps({true, true, true, true});
        QVERIFY(panel.allDone());
        QCOMPARE(spy.count(), 1); // fecha-se sozinho
    }

    void tourShowsPopupAndCompletes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsService settings(dir.filePath("settings.ini"));
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());
        QVERIFY(!settings.tourCompleted());

        QWidget host;
        host.resize(900, 600);
        auto* target = new QWidget(&host);
        target->setGeometry(40, 60, 300, 40);
        target->show();
        host.show();

        OnboardingTour tour(&i18n, &settings, &host);
        tour.setTargets({target, nullptr, nullptr, nullptr});
        tour.start();
        QVERIFY(tour.isRunning());

        // O popup é filho da janela anfitriã: Saltar marca-o como
        // concluído e esconde-o.
        auto* popup = host.findChild<QWidget*>(QStringLiteral("TourPopup"));
        QVERIFY(popup);
        const auto buttons = popup->findChildren<QPushButton*>();
        QVERIFY(buttons.size() >= 2);
        QTest::mouseClick(buttons.last(), Qt::LeftButton);
        QVERIFY(!tour.isRunning());
        QVERIFY(settings.tourCompleted());
    }
};

QTEST_MAIN(TestOnboarding)
#include "test_onboarding.moc"
