#include "localization/i18nservice.h"
#include "services/settingsservice.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using cc::I18nService;
using cc::SettingsService;

namespace {

bool writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    return f.write(contents) == contents.size();
}

} // namespace

class TestI18n final : public QObject
{
    Q_OBJECT

private slots:
    void discoversLanguagesAndDefaultsToEnglish()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(writeFile(root.filePath("en/common.json"), R"json({
            "_meta": { "nativeName": "English" },
            "menu": { "file": "File", "file.quit": "Quit", "greet": "Hello %1!" },
            "deep": { "a": { "b": "DeepValue" } }
        })json"));
        QVERIFY(writeFile(root.filePath("pt-BR/common.json"), R"json({
            "_meta": { "nativeName": "Português" },
            "menu": { "file": "Arquivo", "greet": "Olá %1!" }
        })json"));

        SettingsService settings(root.filePath("cfg.ini"));
        I18nService i18n(&settings, {root.path()});
        QVERIFY(i18n.loadAvailableLanguages());
        QCOMPARE(i18n.availableLanguages().size(), 2);
        QCOMPARE(i18n.currentLanguage(), QStringLiteral("en"));
    }

    void translatesCurrentLanguage()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(writeFile(root.filePath("en/common.json"),
                          R"({"menu":{"file":"File"}})"));
        QVERIFY(writeFile(root.filePath("pt-BR/common.json"),
                          R"({"menu":{"file":"Arquivo"}})"));

        SettingsService settings(root.filePath("cfg.ini"));
        I18nService i18n(&settings, {root.path()});
        QVERIFY(i18n.loadAvailableLanguages());
        QVERIFY(i18n.setLanguage(QStringLiteral("pt-BR")));
        QCOMPARE(i18n.t("common", "menu.file"), QStringLiteral("Arquivo"));
    }

    void fallsBackToEnglishForMissingKey()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(writeFile(root.filePath("en/common.json"),
                          R"({"menu":{"file.quit":"Quit"}})"));
        QVERIFY(writeFile(root.filePath("pt-BR/common.json"),
                          R"({"menu":{"file":"Arquivo"}})"));

        SettingsService settings(root.filePath("cfg.ini"));
        I18nService i18n(&settings, {root.path()});
        QVERIFY(i18n.loadAvailableLanguages());
        QVERIFY(i18n.setLanguage(QStringLiteral("pt-BR")));
        QCOMPARE(i18n.t("common", "menu.file.quit"), QStringLiteral("Quit"));
    }

    void totalMissReturnsDottedKeyWithoutCrashing()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(writeFile(root.filePath("en/common.json"), R"({})"));

        SettingsService settings(root.filePath("cfg.ini"));
        I18nService i18n(&settings, {root.path()});
        QVERIFY(i18n.loadAvailableLanguages());
        QCOMPARE(i18n.t("common", "does.not.exist"),
                 QStringLiteral("common.does.not.exist"));
    }

    void substitutesPositionalArguments()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(writeFile(root.filePath("en/common.json"),
                          R"({"menu":{"greet":"Hello %1, %2!"}})"));
        QVERIFY(writeFile(root.filePath("pt-BR/common.json"),
                          R"({"menu":{"greet":"Olá %1, %2!"}})"));

        SettingsService settings(root.filePath("cfg.ini"));
        I18nService i18n(&settings, {root.path()});
        QVERIFY(i18n.loadAvailableLanguages());
        QVERIFY(i18n.setLanguage(QStringLiteral("pt-BR")));
        QCOMPARE(i18n.t("common", "menu.greet", {QStringLiteral("Ana"),
                                                 QStringLiteral("Bia")}),
                 QStringLiteral("Olá Ana, Bia!"));
    }

    void flattensNestedNamespaces()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(writeFile(root.filePath("en/common.json"),
                          R"({"deep":{"a":{"b":"DeepValue"}}})"));
        QVERIFY(writeFile(root.filePath("en/editor.json"),
                          R"({"tool":{"move":"Move"}})"));

        SettingsService settings(root.filePath("cfg.ini"));
        I18nService i18n(&settings, {root.path()});
        QVERIFY(i18n.loadAvailableLanguages());
        QCOMPARE(i18n.t("common", "deep.a.b"), QStringLiteral("DeepValue"));
        QCOMPARE(i18n.t("editor", "tool.move"), QStringLiteral("Move"));
    }

    void displayNameUsesNativeName()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    QVERIFY(writeFile(root.filePath("en/common.json"),
                      R"({"_meta":{"nativeName":"English"}})"));

    QVERIFY(writeFile(root.filePath("pt-BR/common.json"),
                      R"({"_meta":{"nativeName":"Português"}})"));

    SettingsService settings(root.filePath("cfg.ini"));
    I18nService i18n(&settings, {root.path()});

    QVERIFY(i18n.loadAvailableLanguages());

    QCOMPARE(i18n.displayName(QStringLiteral("pt-BR")),
             QStringLiteral("Português"));

    QCOMPARE(i18n.displayName(QStringLiteral("xx")),
             QStringLiteral("xx"));
}

    void rejectsUnknownLanguage()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(writeFile(root.filePath("en/common.json"), R"({})"));

        SettingsService settings(root.filePath("cfg.ini"));
        I18nService i18n(&settings, {root.path()});
        QVERIFY(i18n.loadAvailableLanguages());
        QVERIFY(!i18n.setLanguage(QStringLiteral("fr")));
        QCOMPARE(i18n.currentLanguage(), QStringLiteral("en"));
    }
};

QTEST_GUILESS_MAIN(TestI18n)
#include "test_i18n.moc"
