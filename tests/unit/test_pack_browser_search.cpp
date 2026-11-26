#include <QtTest>
#include <QLineEdit>
#include <QListWidget>

#include "localization/i18nservice.h"
#include "services/settingsservice.h"
#include "ui/packbrowser.h"

using namespace cc;

/// Com 71 itens, a pesquisa do navegador tem de filtrar (e aceitar o nome
/// traduzido ou a chave).
class TestPackBrowserSearch final : public QObject
{
    Q_OBJECT

private slots:
    void searchFiltersGrid()
    {
        SettingsService settings(QStringLiteral(":memory:"));
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        PackBrowser browser(&i18n);
        browser.show();
        auto* grid = browser.findChild<QListWidget*>();
        auto* query = browser.findChild<QLineEdit*>();
        QVERIFY(grid && query);
        const int total = grid->count();
        QCOMPARE(total, 71);

        query->setText(QStringLiteral("dust"));
        QVERIFY(grid->count() > 0);
        QVERIFY(grid->count() < total);

        query->setText(QStringLiteral("hexagon"));
        QVERIFY(grid->count() >= 10);

        query->setText(QStringLiteral("zzz nada"));
        QCOMPARE(grid->count(), 0);

        query->clear();
        QCOMPARE(grid->count(), total);
    }
};

QTEST_MAIN(TestPackBrowserSearch)
#include "test_pack_browser_search.moc"
