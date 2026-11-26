#include <QtTest>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>

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

        // O registo cobre menus, ferramentas, formas, filtros e ajustes:
        // nada de "órfão" que a paleta não consiga identificar.
        QVERIFY(actions.allActions().size() >= 120);
        QCOMPARE(actions.actionId(actions.newAction),
                 QStringLiteral("file.new"));
        QCOMPARE(actions.actionId(actions.commandPaletteAction),
                 QStringLiteral("help.commandPalette"));
        QVERIFY(actions.actionId(actions.safeYouTubeAction) ==
                QStringLiteral("view.safeYouTube"));
        QVERIFY(!actions.actionId(nullptr).isEmpty() == false);
    }

    void paletteListsCommandsAndFilters()
    {
        SettingsService settings(QStringLiteral(":memory:"));
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        ActionManager actions;
        actions.createActions(&i18n);

        CommandPalette palette(&actions, &i18n);
        palette.show();
        QVERIFY(palette.isVisible());

        auto* query = palette.findChild<QLineEdit*>();
        auto* list = palette.findChild<QListWidget*>();
        QVERIFY(query);
        QVERIFY(list);
        const int total = list->count();
        QVERIFY(total > 20);

        //Consulta sem resultados esvazia a lista.
        query->setText(QStringLiteral("zzzznadaaqui"));
        QCOMPARE(list->count(), 0);

        //Consulta por subsequência encontra o comando.
        query->setText(QStringLiteral("duplicate"));
        QVERIFY(list->count() >= 1);
        QVERIFY(firstRowText(palette).contains(QStringLiteral("Duplicate"),
                                                Qt::CaseInsensitive));

        //Consulta pelo id estavel tambem funciona.
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

        CommandPalette palette(&actions, &i18n);
        palette.show();
        auto* query = palette.findChild<QLineEdit*>();
        auto* list = palette.findChild<QListWidget*>();
        QVERIFY(query && list);

        query->setText(QStringLiteral("zoom fit"));
        QVERIFY(list->count() >= 1);
        QCOMPARE(list->currentRow(), 0);

        QTest::keyClick(query, Qt::Key_Return);
        QCOMPARE(triggers, 1);
        QVERIFY(!palette.isVisible()); // fecha apos executar
    }

    void paletteIsTranslated()
    {
        SettingsService settings(QStringLiteral(":memory:"));
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        ActionManager actions;
        actions.createActions(&i18n);

        CommandPalette palette(&actions, &i18n);
        QCOMPARE(palette.windowTitle(), QStringLiteral("Command Palette"));
        auto* query = palette.findChild<QLineEdit*>();
        QVERIFY(query);
        QVERIFY(!query->placeholderText().isEmpty());

        QVERIFY(i18n.setLanguage(QStringLiteral("pt-BR")));
        QCOMPARE(palette.windowTitle(), QStringLiteral("Paleta de Comandos"));
    }
};

QTEST_MAIN(TestCommandPalette)
#include "test_command_palette.moc"
