#include <QtTest>
#include <QSignalSpy>

#include "ui/themeicons.h"
#include "ui/collapsiblesection.h"
#include "ui/startscreen.h"
#include "ui/layerspanel.h"
#include "localization/i18nservice.h"
#include "services/presetstore.h"
#include "services/recentfiles.h"
#include "services/settingsservice.h"
#include "core/Document.h"
#include "core/layers/Layer.h"
#include "core/templates/TemplateFactory.h"

using namespace cc;

class TestUiPhase1 : public QObject
{
    Q_OBJECT

private slots:
    void testThemeIconsGeneration()
    {
        // Tools icons
        QVERIFY(!ThemeIcons::toolSelect().isNull());
        QVERIFY(!ThemeIcons::toolCrop().isNull());
        QVERIFY(!ThemeIcons::toolScissors().isNull());
        QVERIFY(!ThemeIcons::toolWand().isNull());
        QVERIFY(!ThemeIcons::toolClone().isNull());
        QVERIFY(!ThemeIcons::toolPaint().isNull());
        QVERIFY(!ThemeIcons::toolFlood().isNull());
        QVERIFY(!ThemeIcons::toolText().isNull());
        QVERIFY(!ThemeIcons::toolShape().isNull());

        // Layer controls icons
        QVERIFY(!ThemeIcons::layerVisibility(true).isNull());
        QVERIFY(!ThemeIcons::layerVisibility(false).isNull());
        QVERIFY(!ThemeIcons::layerLock(true).isNull());
        QVERIFY(!ThemeIcons::layerLock(false).isNull());
        QVERIFY(!ThemeIcons::layerTypeText().isNull());
        QVERIFY(!ThemeIcons::layerTypeImage().isNull());
        QVERIFY(!ThemeIcons::layerTypeShape().isNull());
        QVERIFY(!ThemeIcons::layerTypeGroup().isNull());

        // Platform brand badges
        QVERIFY(!ThemeIcons::brandYoutube(32).isNull());
        QVERIFY(!ThemeIcons::brandInstagram(32).isNull());
        QVERIFY(!ThemeIcons::brandTiktok(32).isNull());
        QVERIFY(!ThemeIcons::brandBanner(32).isNull());
        QVERIFY(!ThemeIcons::emptyProjectsPlaceholder(64, 64).isNull());

        // App and action icons
        QVERIFY(!ThemeIcons::appIcon().isNull());
        QVERIFY(!ThemeIcons::actionSave().isNull());
        QVERIFY(!ThemeIcons::actionSaveAs().isNull());
        QVERIFY(!ThemeIcons::actionImport().isNull());
        QVERIFY(!ThemeIcons::actionExport().isNull());
        QVERIFY(!ThemeIcons::actionQuit().isNull());
        QVERIFY(!ThemeIcons::actionUndo().isNull());
        QVERIFY(!ThemeIcons::actionRedo().isNull());
        QVERIFY(!ThemeIcons::actionZoomIn().isNull());
        QVERIFY(!ThemeIcons::actionZoomOut().isNull());
        QVERIFY(!ThemeIcons::actionZoomFit().isNull());
        QVERIFY(!ThemeIcons::actionGrid().isNull());
        QVERIFY(!ThemeIcons::actionSnap().isNull());
        QVERIFY(!ThemeIcons::actionAlignLeft().isNull());
        QVERIFY(!ThemeIcons::actionAlignCenter().isNull());
        QVERIFY(!ThemeIcons::actionAlignRight().isNull());
        QVERIFY(!ThemeIcons::actionAlignTop().isNull());
        QVERIFY(!ThemeIcons::actionAlignMiddle().isNull());
        QVERIFY(!ThemeIcons::actionAlignBottom().isNull());
        QVERIFY(!ThemeIcons::actionDistributeH().isNull());
        QVERIFY(!ThemeIcons::actionDistributeV().isNull());
        QVERIFY(!ThemeIcons::actionSettings().isNull());
        QVERIFY(!ThemeIcons::actionAbout().isNull());
        QVERIFY(!ThemeIcons::actionCheck().isNull());
        QVERIFY(!ThemeIcons::actionCancel().isNull());
    }

    void testCollapsibleSection()
    {
        CollapsibleSection section(QStringLiteral("Settings"), true);
        QVERIFY(section.isExpanded());

        QSignalSpy spy(&section, &CollapsibleSection::toggled);
        section.setExpanded(false);
        QVERIFY(!section.isExpanded());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), false);

        section.setTitle(QStringLiteral("Nova Seção"));
        QCOMPARE(section.title(), QStringLiteral("Nova Seção"));
    }

    void testStartScreenRetranslate()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        i18n.loadAvailableLanguages();

        PresetStore presets(tmpDir.path() + "/presets.json");
        RecentFiles recents(tmpDir.path() + "/recent.json");

        StartScreen screen(&i18n, &presets, &recents);

        // Switch to pt-BR and ensure no crashes and clean dynamic text
        QVERIFY(i18n.setLanguage(QStringLiteral("pt-BR")));
        QCOMPARE(i18n.currentLanguage(), QStringLiteral("pt-BR"));

        // Switch back to en
        QVERIFY(i18n.setLanguage(QStringLiteral("en")));
        QCOMPARE(i18n.currentLanguage(), QStringLiteral("en"));
    }

    void testLayersPanelThumbnailsAndRebuild()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        i18n.loadAvailableLanguages();

        Document doc(800, 600, 96);

        // Add a Text layer
        auto text = std::make_unique<TextLayer>();
        text->content = QStringLiteral("Título Criativo");
        text->color = QColor(Qt::yellow);
        doc.addLayer(std::move(text));

        // Add a Shape layer
        auto shape = std::make_unique<ShapeLayer>();
        shape->fill = QColor(Qt::cyan);
        doc.addLayer(std::move(shape));

        LayersPanel panel(&i18n, nullptr);
        panel.setDocument(&doc);

        // Rebuild and refresh without crashing
        panel.refresh();
        QVERIFY(doc.rootGroup()->children.size() >= 2);
    }

    void testTemplatesCreationAndLayerLoading()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        i18n.loadAvailableLanguages();

        const auto templates = TemplateFactory::availableTemplates();
        QCOMPARE(templates.size(), 7);

        for (const auto& tmpl : templates) {
            auto doc = TemplateFactory::createTemplate(tmpl.kind, &i18n);
            QVERIFY(doc != nullptr);
            QCOMPARE(doc->width(), tmpl.width);
            QCOMPARE(doc->height(), tmpl.height);
            QVERIFY(doc->rootGroup()->children.size() >= 3);

            LayersPanel panel(&i18n, nullptr);
            panel.setDocument(doc.get());
            panel.refresh();
        }
    }
};

QTEST_MAIN(TestUiPhase1)
#include "test_ui_phase1.moc"
