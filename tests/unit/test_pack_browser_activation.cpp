#include <QtTest>
#include <QListWidget>
#include <QSignalSpy>

#include "localization/i18nservice.h"
#include "services/settingsservice.h"
#include "ui/packbrowser.h"

using namespace cc;

/// Um duplo clique num item tem de aplicar UMA vez só: sem esta guarda o
/// navegador emitia itemChosen duas vezes e criava duas camadas.
class TestPackBrowserActivation final : public QObject
{
    Q_OBJECT

private slots:
    void doubleClickEmitsOnce()
    {
        SettingsService settings(QStringLiteral(":memory:"));
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        PackBrowser browser(&i18n);
        browser.show();
        auto* grid = browser.findChild<QListWidget*>();
        QVERIFY(grid);
        QVERIFY(grid->count() > 0);
        grid->setCurrentRow(0);

        QSignalSpy spy(&browser, &PackBrowser::itemChosen);
        const QRect rect = grid->visualItemRect(grid->item(0));
        QTest::mouseClick(grid->viewport(), Qt::LeftButton, Qt::NoModifier,
                         rect.center());
        QTest::mouseDClick(grid->viewport(), Qt::LeftButton, Qt::NoModifier,
                          rect.center());
        QCOMPARE(spy.count(), 1);
    }

    void enterKeyEmitsOnce()
    {
        SettingsService settings(QStringLiteral(":memory:"));
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        PackBrowser browser(&i18n);
        browser.show();
        auto* grid = browser.findChild<QListWidget*>();
        QVERIFY(grid);
        grid->setCurrentRow(1);

        QSignalSpy spy(&browser, &PackBrowser::itemChosen);
        QTest::keyClick(grid, Qt::Key_Return);
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestPackBrowserActivation)
#include "test_pack_browser_activation.moc"
