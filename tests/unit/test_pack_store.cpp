#include <QtTest>
#include <QComboBox>
#include <QFile>
#include <QListWidget>

#include "localization/i18nservice.h"
#include "services/packstore.h"
#include "services/settingsservice.h"
#include "ui/packbrowser.h"

using namespace cc;

class TestPackStore final : public QObject
{
    Q_OBJECT

private slots:
    void packsAreEmbeddedAndConsistent()
    {
        const QVector<PackInfo>& packs = PackStore::packs();
        QVERIFY(!packs.isEmpty());

        const PackInfo* starter = nullptr;
        for (const PackInfo& pack : packs) {
            if (pack.id == QLatin1String("starter"))
                starter = &pack;
        }
        QVERIFY(starter);
        QCOMPARE(starter->items.size(), 23);

        // Pacotes de texturas e sobreposicoes com categorias proprias.
        const PackInfo* texturas = nullptr;
        const PackInfo* sobre = nullptr;
        for (const PackInfo& pack : packs) {
            if (pack.id == QLatin1String("texturas"))
                texturas = &pack;
            if (pack.id == QLatin1String("sobreposicoes"))
                sobre = &pack;
        }
        QVERIFY(texturas);
        QCOMPARE(texturas->items.size(), 32);
        QVERIFY(sobre);
        QCOMPARE(sobre->items.size(), 16);
        QVERIFY(PackStore::categories().contains(QLatin1String("overlay")));
        QVERIFY(PackStore::categories().contains(QLatin1String("texture")));

        // Todos os ficheiros do manifesto existem no recurso Qt e abrem.
        for (const PackItem& item : PackStore::allItems()) {
            QFile file(item.resource);
            QVERIFY2(file.exists(), qPrintable(item.resource));
            QVERIFY2(item.naturalSize().isValid(), qPrintable(item.resource));
        }
    }

    void everyLabelIsTranslated()
    {
        SettingsService settings(QStringLiteral(":memory:"));
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        // Uma chave em falta devolve a própria chave -- o teste falha.
        for (const PackItem& item : PackStore::allItems()) {
            const QString label = item.label(&i18n);
            QVERIFY2(!label.isEmpty(), qPrintable(item.labelKey));
            QVERIFY2(label != item.labelKey, qPrintable(item.labelKey));
        }
    }

    void browserListsAndFilters()
    {
        SettingsService settings(QStringLiteral(":memory:"));
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        PackBrowser browser(&i18n);
        auto* grid = browser.findChild<QListWidget*>();
        QVERIFY(grid);
        QCOMPARE(grid->count(), 71); // 23 starter + 32 texturas + 16 sobreposicoes

        auto* combo = browser.findChild<QComboBox*>();
        QVERIFY(combo);
        // Categoria "template": os três ecrãs finais.
        int templateIndex = -1;
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toString() == QLatin1String("template"))
                templateIndex = i;
        }
        QVERIFY(templateIndex >= 0);
        combo->setCurrentIndex(templateIndex);
        QCOMPARE(grid->count(), 3);

        // "background": fundos.
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toString() == QLatin1String("background")) {
                combo->setCurrentIndex(i);
                break;
            }
        }
        QCOMPARE(grid->count(), 6);
    }
};

QTEST_MAIN(TestPackStore)
#include "test_pack_store.moc"
