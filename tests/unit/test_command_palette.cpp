#include <QtTest>
#include <QLineEdit>
#include <QListWidget>
#include <QWidget>

#include "localization/i18nservice.h"
#include "services/settingsservice.h"
#include "ui/actionmanager.h"
#include "ui/commandpalette.h"

using namespace cc;

class TestCommandPalette final : public QObject
{
    Q_OBJECT

private slots:
    static QString firstRowText(const CommandPalette& palette)
    {
        const auto* list = palette.findChild<QListWidget*>();
        if (!list || list->count() == 0)
            return QString();
        return list->item(0)->text();
    }

    void registryCoversEveryMenu()
    {
        SettingsService settings(QStringLiteral(":memory:"));
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        ActionManager actions;
        actions.createActions(&i18n);

        QVERIFY(actions.allActions().size() >= 120);
        QCOMPARE(actions.actionId(actions.newAction), QStringLiteral("file.new"));
        QCOMPARE(actions.actionId(actions.commandPaletteAction),
                 QStringLiteral("help.commandPalette"));
        QVERIFY(actions.actionId(actions.safeYouTubeAction) ==
                QStringLiteral("view.safeYouTube"));
        QVERIFY(actions.actionId(nullptr).isEmpty());
    }

    void paletteListsCommandsAndFilters()
    {
        SettingsService settings(QStringLiteral(":memory:"));
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        ActionManager actions;
        actions.createActions(&i18n);

        // Como na Produção (MainWindow passa a janela como pai): um popup
        // sem pai é um caso artificial que se comporta diferente entre
        // versões/plataformas do Qt.
        QWidget host;
        CommandPalette palette(&actions, &i18n, &host);
        palette.show();
        QVERIFY(palette.isVisible());

        auto* query = palette.findChild<QLineEdit*>();
        auto* list = palette.findChild<QListWidget*>();
        QVERIFY(query);
        QVERIFY(list);
        const int total = list->count();
        QVERIFY(total > 20);

        query->setText(QStringLiteral("zzzznadaaqui"));
        QCOMPARE(list->count(), 0);

        query->setText(QStringLiteral("duplicate"));
        QVERIFY(list->count() >= 1);
        QVERIFY(firstRowText(palette).contains(QStringLiteral("Duplicate"),
                                                Qt::CaseInsensitive));

        query->setText(QStringLiteral("layer.flatten"));
        QVERIFY(list->count() >= 1);
    }

    void paletteRunsSelectedCommand()
    {
        SettingsService settings(QStringLiteral(":memory:"));
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        ActionManager actions;
        actions.createActions(&i18n);

        int triggers = 0;
        connect(actions.zoomFitAction, &QAction::triggered, this,
                [&triggers] { ++triggers; });

        QWidget host;
        CommandPalette palette(&actions, &i18n, &host);
        palette.show();
        auto* query = palette.findChild<QLineEdit*>();
        auto* list = palette.findChild<QListWidget*>();
        QVERIFY(query && list);

        query->setText(QStringLiteral("zoom fit"));
        QVERIFY(list->count() >= 1);
        QCOMPARE(list->currentRow(), 0);

        QTest::keyClick(query, Qt::Key_Return);
        QCOMPARE(triggers, 1);
        QVERIFY(!palette.isVisible());
    }

    void paletteIsTranslated()
    {
        SettingsService settings(QStringLiteral(":memory:"));
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        ActionManager actions;
        actions.createActions(&i18n);

        QWidget host;
        qWarning() << "DBG 1: antes do popup";
        CommandPalette palette(&actions, &i18n, &host);
        palette.show();
        qWarning() << "DBG 2: popup mostrado";
        QCOMPARE(palette.windowTitle(), QStringLiteral("Command Palette"));
        auto* query = palette.findChild<QLineEdit*>();
        QVERIFY(query);
        QVERIFY(!query->placeholderText().isEmpty());
        qWarning() << "DBG 3: asserts base ok";

        // Mudar o idioma re-traduz o chrome do popup (sinal languageChanged):
        // título e placeholder em todos os idiomas. O popup é transitório, por
        // isso os rótulos dos itens são refrescados ao abrir (refresh()).
        qWarning() << "DBG 4: antes de setLanguage";
        QVERIFY(i18n.setLanguage(QStringLiteral("pt-BR")));
        qWarning() << "DBG 5: depois de setLanguage";
        QCOMPARE(palette.windowTitle(), QStringLiteral("Paleta de Comandos"));
        qWarning() << "DBG 6: titulo pt ok";
        QVERIFY(!query->placeholderText().isEmpty());
        QVERIFY(query->placeholderText() != QStringLiteral("Type a command name..."));

        QVERIFY(i18n.setLanguage(QStringLiteral("sv")));
        qWarning() << "DBG 7: sv ok";
        QCOMPARE(palette.windowTitle(), QStringLiteral("Kommandopalett"));

        // E ao reabrir, os rótulos voltam ao idioma certo.
        QVERIFY(i18n.setLanguage(QStringLiteral("en")));
        palette.refresh();
        qWarning() << "DBG 8: refresh ok";
        QVERIFY(firstRowText(palette).isEmpty() == false);
        QCOMPARE(palette.windowTitle(), QStringLiteral("Command Palette"));
        qWarning() << "DBG 9: fim do teste";
    }
};

QTEST_MAIN(TestCommandPalette)
#include "test_command_palette.moc"
