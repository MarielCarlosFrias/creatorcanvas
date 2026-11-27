#include <QtTest>
#include <QTemporaryDir>

#include "localization/i18nservice.h"
#include "services/settingsservice.h"
#include "ui/actionmanager.h"

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

/// Duas acoes com o mesmo atalho fazem o Qt disparar as duas (ou nenhuma).
/// Este teste falha se um registo novo colidir com outro.
class TestShortcuts final : public QObject
{
    Q_OBJECT

private slots:
    void noDuplicateDefaultShortcuts()
    {
        SettingsService settings(testSettingsFile());
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        ActionManager actions;
        actions.createActions(&i18n);

        const QMap<QString, QKeySequence> all = actions.getAllShortcuts();
        QMap<QString, QStringList> byShortcut;
        for (auto it = all.cbegin(); it != all.cend(); ++it) {
            if (it.value().isEmpty())
                continue;
            byShortcut[it.value().toString(QKeySequence::PortableText)].append(it.key());
        }
        for (auto it = byShortcut.cbegin(); it != byShortcut.cend(); ++it) {
            if (it.value().size() > 1) {
                qWarning() << "Atalho duplicado" << it.key() << it.value();
                QFAIL("dois comandos partilham o mesmo atalho");
            }
        }
        // Ha atalhos por omissao registados (nao é um teste vazio).
        QVERIFY(!byShortcut.isEmpty());
    }

    void packBrowserShortcutIsNotTheToolsBarOne()
    {
        SettingsService settings(testSettingsFile());
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        ActionManager actions;
        actions.createActions(&i18n);
        QVERIFY(actions.backgroundsAction);
        QVERIFY(actions.toggleToolsBarAction);
        QVERIFY(actions.backgroundsAction->shortcut()
                != actions.toggleToolsBarAction->shortcut());
    }
};

QTEST_MAIN(TestShortcuts)
#include "test_shortcuts.moc"
