#include "localization/i18nservice.h"
#include "services/settingsservice.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
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

/// Achata um objeto JSON aninhado em chaves pontuadas (mesma regra do
/// I18nService: {"menu": {"file": {"new": "..."}}} -> menu.file.new).
void flattenObject(const QJsonObject& obj, const QString& prefix, QSet<QString>& out)
{
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString key = prefix.isEmpty() ? it.key() : prefix + QLatin1Char('.') + it.key();
        if (it.value().isObject())
            flattenObject(it.value().toObject(), key, out);
        else
            out.insert(key);
    }
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

    void everyLocaleJsonIsListedInQrc()
    {
        const QString root = QString::fromLatin1(CC_PROJECT_ROOT);
        const QDir locales(root + QStringLiteral("/resources/locales"));
        QFile qrc(root + QStringLiteral("/resources/locales.qrc"));
        QVERIFY2(locales.exists(), "resources/locales directory not found");
        QVERIFY2(qrc.open(QIODevice::ReadOnly | QIODevice::Text),
                 "resources/locales.qrc could not be opened");

        const QString qrcText = QString::fromUtf8(qrc.readAll());
        const QRegularExpression filePattern(QStringLiteral(
            R"(<file>\s*([^<]+)\s*</file>)"));
        QSet<QString> listed;
        auto matches = filePattern.globalMatch(qrcText);
        while (matches.hasNext())
            listed.insert(matches.next().captured(1).trimmed());

        QStringList missing;
        const QStringList languages =
            locales.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString& lang : languages) {
            const QDir langDir(locales.filePath(lang));
            for (const QString& file :
                 langDir.entryList({QStringLiteral("*.json")}, QDir::Files)) {
                const QString resourcePath =
                    QStringLiteral("locales/%1/%2").arg(lang, file);
                if (!listed.contains(resourcePath))
                    missing << resourcePath
                            + QStringLiteral(" (add it to resources/locales.qrc)");
            }
        }

        missing.sort();
        QVERIFY2(missing.isEmpty(),
                 qPrintable(QStringLiteral("Locale JSON files missing from resources/locales.qrc:\n")
                            + missing.join(QLatin1Char('\n'))));
    }
    /// Regressão real: a barra de status mostrava a chave crua
    /// ("editor.canvas.status.paintHelp") quando o código pedia uma chave
    /// ausente do catálogo. Este teste varre src/ e falha para qualquer
    /// chave sem tradução em nenhum dos idiomas.
    void everyKeyUsedInSourcesExists()
    {
        const QString root = QString::fromLatin1(CC_PROJECT_ROOT);
        const QDir locales(root + QStringLiteral("/resources/locales"));
        if (!locales.exists())
            QSKIP("catálogos de locale não encontrados (build empacotado)");

        QMap<QString, QSet<QString>> catalogs;
        const QStringList languages =
            locales.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        QVERIFY(languages.contains(QStringLiteral("en")));
        for (const QString& lang : languages) {
            const QDir langDir(locales.filePath(lang));
            QSet<QString> keys;
            for (const QString& file :
                 langDir.entryList({QStringLiteral("*.json")}, QDir::Files)) {
                QFile f(langDir.filePath(file));
                if (!f.open(QIODevice::ReadOnly))
                    continue;
                const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                if (!doc.isObject())
                    continue;
                const QString section = QFileInfo(file).completeBaseName();
                flattenObject(doc.object(), section, keys);
            }
            // Metadados (nativeName etc.) não são chaves de tradução.
            keys.remove(QStringLiteral("_meta"));
            catalogs.insert(lang, keys);
        }

        // Varre o código por chamadas t("seção", "chave").
        QSet<QString> used;
        static const QRegularExpression callPattern(QStringLiteral(
            "\\bt\\(\\s*\"([A-Za-z0-9_\\-]+)\"\\s*,\\s*\"([^\"]+)\""));
        QDirIterator it(root + QStringLiteral("/src"),
                        {QStringLiteral("*.cpp"), QStringLiteral("*.h")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile f(it.next());
            if (!f.open(QIODevice::ReadOnly))
                continue;
            const QString source = QString::fromUtf8(f.readAll());
            auto matches = callPattern.globalMatch(source);
            while (matches.hasNext()) {
                const auto match = matches.next();
                used.insert(match.captured(1) + QLatin1Char('.') + match.captured(2));
            }
        }
        QVERIFY2(used.size() > 100,
                 "varredura de i18n não encontrou chamadas t(...) — caminho errado?");

        // --- Chaves DINÂMICAS: o scan acima só vê t("ns", "chave") com dois
        // literais. Os diálogos novos usam um helper tr("chave", ...) que
        // monta a chave como QStringLiteral("<prefixo>.") + chave — foi
        // assim que o rótulo do histograma apareceu cru na UI. Aqui varremos
        // esses ficheiros: prefixos usados em cada ficheiro × chaves literais
        // passadas ao helper tr().
        QSet<QString> dynamicUsed;
        const QStringList dialogFiles = {
            QStringLiteral("adjustmentsdialog.cpp"), QStringLiteral("adjustmentsection.cpp"),
            QStringLiteral("layercompsdialog.cpp"), QStringLiteral("refineedgedialog.cpp"),
            QStringLiteral("webslicesdialog.cpp"),
        };
        for (const QString& dialogFile : dialogFiles) {
            const QString dialogPath = root + QStringLiteral("/src/ui/") + dialogFile;
            QFile f(dialogPath);
            if (!f.open(QIODevice::ReadOnly))
                continue;
            const QString source = QString::fromUtf8(f.readAll());
            // Prefixos: QStringLiteral("xxx.") usados em chamadas t(...).
            QSet<QString> prefixes;
            // [A-Za-z0-9_] e não apenas minúsculas: prefixos como
            // "refineEdge" e "webSlices" têm maiúsculas e eram ignorados.
            const QRegularExpression prefixPattern(QStringLiteral(
                R"(t\(\s*\"[A-Za-z0-9_\-]+\"\s*,\s*QStringLiteral\(\"([A-Za-z0-9_]+)\.\"\))"));
            for (auto it = prefixPattern.globalMatch(source); it.hasNext();)
                prefixes.insert(it.next().captured(1));
            // Chaves: tr("chave", ...).
            const QRegularExpression keyPattern(QStringLiteral(
                R"(\btr\(\s*\"([A-Za-z0-9_]+)\"\s*,)"));
            for (auto it = keyPattern.globalMatch(source); it.hasNext();) {
                const QString key = it.next().captured(1);
                for (const QString& prefix : prefixes)
                    dynamicUsed.insert(QStringLiteral("editor.") + prefix + QStringLiteral(".") + key);
            }
        }
        QVERIFY2(dynamicUsed.size() >= 10,
                 qPrintable(QStringLiteral("nenhuma chave dinâmica encontrada — o scan dos diálogos "
                                           "está correcto? ficheiros: %1")
                            .arg(dialogFiles.join(QLatin1Char(' ')))));
        used.unite(dynamicUsed);

        QStringList missing;
        for (const QString& lang : languages) {
            for (const QString& key : used) {
                if (!catalogs.value(lang).contains(key)) {
                    const int separator = key.indexOf(QLatin1Char('.'));
                    const QString ns = separator >= 0 ? key.left(separator) : key;
                    const QString leaf = separator >= 0 ? key.mid(separator + 1) : key;
                    missing << lang + QStringLiteral(": namespace '") + ns
                            + QStringLiteral("' (") + ns + QStringLiteral(".json), key '")
                            + leaf + QLatin1Char('\'');
                }
            }
        }
        missing.sort();
        QVERIFY2(missing.isEmpty(),
                 qPrintable(QStringLiteral("Chaves i18n ausentes no catálogo:\n")
                                + missing.join(QLatin1Char('\n'))));
    }

};

QTEST_GUILESS_MAIN(TestI18n)
#include "test_i18n.moc"
