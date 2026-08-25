#include "services/settingsservice.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using cc::SettingsService;

class TestSettings final : public QObject
{
    Q_OBJECT

private slots:
    void returnsDefaultsWhenFileIsEmpty()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsService s(dir.filePath("settings.ini"));
        QCOMPARE(s.language(), QStringLiteral("en"));
        QCOMPARE(s.autosaveIntervalMinutes(), 5);
        QCOMPARE(s.gpuAccelerationEnabled(), false);
    }

    void persistsValuesAcrossInstances()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("settings.ini");
        {
            SettingsService s(path);
            s.setLanguage(QStringLiteral("pt-BR"));
            s.setAutosaveIntervalMinutes(10);
            s.setGpuAccelerationEnabled(true);
        }
        SettingsService s(path);
        QCOMPARE(s.language(), QStringLiteral("pt-BR"));
        QCOMPARE(s.autosaveIntervalMinutes(), 10);
        QCOMPARE(s.gpuAccelerationEnabled(), true);
    }

    void zeroDisablesAutosave()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsService s(dir.filePath("settings.ini"));
        s.setAutosaveIntervalMinutes(0);
        QCOMPARE(s.autosaveIntervalMinutes(), 0);
    }

    void changeSignalEmitsOnlyOnActualChange()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsService s(dir.filePath("settings.ini"));
        QSignalSpy spy(&s, &SettingsService::changed);
        QVERIFY(spy.isValid());

        s.setLanguage(QStringLiteral("pt-BR"));
        QCOMPARE(spy.count(), 1);

        s.setLanguage(QStringLiteral("pt-BR")); // no change -> no signal
        QCOMPARE(spy.count(), 1);

        s.setAutosaveIntervalMinutes(1);
        QCOMPARE(spy.count(), 2);
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "test_settings.moc"
