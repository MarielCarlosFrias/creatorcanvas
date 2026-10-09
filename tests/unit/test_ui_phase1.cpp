#include <QtTest>
#include <QSignalSpy>
#include <QListWidget>
#include <QToolButton>
#include <QLabel>
#include <QPlainTextEdit>

#include "ui/themeicons.h"
#include "ui/collapsiblesection.h"
#include "ui/startscreen.h"
#include "ui/layerspanel.h"
#include "ui/historypanel.h"
#include "ui/textinspector.h"
#include "ui/darktheme.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
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
        QCOMPARE(templates.size(), 11);

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

    void testCreativeShapesAndKinds()
    {
        Document doc(1280, 720, 96);
        auto shape = std::make_unique<ShapeLayer>();
        shape->kind = ShapeKind::Rectangle;
        const LayerId id = shape->id();
        doc.addLayer(std::move(shape));

        // Test mutating shape kind to SpeechBubble
        QVERIFY(doc.setShapeKind(id, ShapeKind::SpeechBubble));
        auto* layer = static_cast<ShapeLayer*>(doc.findLayer(id));
        QVERIFY(layer != nullptr);
        QCOMPARE(layer->kind, ShapeKind::SpeechBubble);

        // Test mutating shape kind to Heart
        QVERIFY(doc.setShapeKind(id, ShapeKind::Heart));
        QCOMPARE(layer->kind, ShapeKind::Heart);

        // Test undo/redo with SetShapeKindCommand
        CommandStack history;
        history.execute(std::make_unique<SetShapeKindCommand>(
            doc, id, QStringLiteral("shape.kind"), &Document::setShapeKind,
            ShapeKind::Heart, ShapeKind::Star));
        QCOMPARE(layer->kind, ShapeKind::Star);

        history.undo();
        QCOMPARE(layer->kind, ShapeKind::Heart);

        history.redo();
        QCOMPARE(layer->kind, ShapeKind::Star);
    }

    void testThemeSwitching()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        i18n.loadAvailableLanguages();

        CommandStack history;
        Document doc(800, 600, 96);

        LayersPanel layersPanel(&i18n, &history);
        layersPanel.setDocument(&doc);

        HistoryPanel historyPanel(&i18n, &history);
        CollapsibleSection section(QStringLiteral("Effects"), true);

        // Switch to Light Mode
        applyTheme(*qApp, false);
        const QColor lightWindow = qApp->palette().color(QPalette::Window);
        QVERIFY(lightWindow.value() >= 128);

        layersPanel.refresh();
        historyPanel.refresh();

        // Switch to Dark Mode
        applyTheme(*qApp, true);
        const QColor darkWindow = qApp->palette().color(QPalette::Window);
        QVERIFY(darkWindow.value() < 128);

        layersPanel.refresh();
        historyPanel.refresh();
    }

    void testHistoryPanelIntegration()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        i18n.loadAvailableLanguages();

        CommandStack history;
        Document doc(800, 600, 96);

        HistoryPanel panel(&i18n, &history);
        auto* list = panel.findChild<QListWidget*>(QStringLiteral("HistoryListWidget"));
        auto* undoBtn = panel.findChild<QToolButton*>(QStringLiteral("HistoryUndoButton"));
        auto* redoBtn = panel.findChild<QToolButton*>(QStringLiteral("HistoryRedoButton"));
        auto* statusLabel = panel.findChild<QLabel*>(QStringLiteral("HistoryStatusLabel"));

        QVERIFY(list != nullptr);
        QVERIFY(undoBtn != nullptr);
        QVERIFY(redoBtn != nullptr);
        QVERIFY(statusLabel != nullptr);

        // Initial state: Item 0 is New Document
        QCOMPARE(list->count(), 1);
        QCOMPARE(history.currentIndex(), 0);
        QCOMPARE(undoBtn->isEnabled(), false);
        QCOMPARE(redoBtn->isEnabled(), false);
        QCOMPARE(statusLabel->text(), QStringLiteral("0 / 0"));

        // Execute command 1 (Add Shape Layer)
        history.execute(std::make_unique<AddLayerCommand>(doc, std::make_unique<ShapeLayer>()));
        QCOMPARE(list->count(), 2);
        QCOMPARE(history.currentIndex(), 1);
        QCOMPARE(undoBtn->isEnabled(), true);
        QCOMPARE(redoBtn->isEnabled(), false);
        QCOMPARE(statusLabel->text(), QStringLiteral("1 / 1"));

        // Execute command 2 (Add Text Layer)
        history.execute(std::make_unique<AddLayerCommand>(doc, std::make_unique<TextLayer>()));
        QCOMPARE(list->count(), 3);
        QCOMPARE(history.currentIndex(), 2);
        QCOMPARE(undoBtn->isEnabled(), true);
        QCOMPARE(redoBtn->isEnabled(), false);
        QCOMPARE(statusLabel->text(), QStringLiteral("2 / 2"));

        // Click past item (Item 1: Shape layer) -> should rewind 1 step
        emit list->itemClicked(list->item(1));
        QCOMPARE(history.currentIndex(), 1);
        QCOMPARE(list->currentRow(), 1);
        QCOMPARE(undoBtn->isEnabled(), true);
        QCOMPARE(redoBtn->isEnabled(), true);
        QCOMPARE(statusLabel->text(), QStringLiteral("1 / 2"));

        // Click state 0 (Initial document) -> should rewind to 0
        emit list->itemClicked(list->item(0));
        QCOMPARE(history.currentIndex(), 0);
        QCOMPARE(list->currentRow(), 0);
        QCOMPARE(undoBtn->isEnabled(), false);
        QCOMPARE(redoBtn->isEnabled(), true);
        QCOMPARE(statusLabel->text(), QStringLiteral("0 / 2"));

        // Click redo button
        redoBtn->click();
        QCOMPARE(history.currentIndex(), 1);
        QCOMPARE(undoBtn->isEnabled(), true);
        QCOMPARE(redoBtn->isEnabled(), true);

        // Click redo button again to reach head
        redoBtn->click();
        QCOMPARE(history.currentIndex(), 2);
        QCOMPARE(undoBtn->isEnabled(), true);
        QCOMPARE(redoBtn->isEnabled(), false);

        // Click undo button
        undoBtn->click();
        QCOMPARE(history.currentIndex(), 1);
        QCOMPARE(undoBtn->isEnabled(), true);
        QCOMPARE(redoBtn->isEnabled(), true);
    }

    void testTextInspectorDebounce()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        i18n.loadAvailableLanguages();

        CommandStack history;
        Document doc(800, 600, 96);

        auto textLayer = std::make_unique<TextLayer>();
        textLayer->content = QStringLiteral("Primeiro Texto");
        const LayerId textId = textLayer->id();
        doc.addLayer(std::move(textLayer));

        TextInspector inspector(&i18n, &doc, &history);
        inspector.setSelectedLayer(textId);

        auto* edit = inspector.findChild<QPlainTextEdit*>();
        QVERIFY(edit != nullptr);
        QCOMPARE(edit->toPlainText(), QStringLiteral("Primeiro Texto"));

        // Simulate typing into the plain text editor
        edit->setPlainText(QStringLiteral("Texto Atualizado"));

        // Live visual feedback: Document was updated immediately
        auto* layer = static_cast<TextLayer*>(doc.findLayer(textId));
        QVERIFY(layer != nullptr);
        QCOMPARE(layer->content, QStringLiteral("Texto Atualizado"));

        // Debounce: History has NOT recorded a command per-keystroke yet
        QCOMPARE(history.totalCount(), 0);

        // Explicitly commit (as happens when debounce timer triggers or widget loses focus)
        inspector.commitPendingTextChange();
        QCOMPARE(history.totalCount(), 1);
        QCOMPARE(history.commandNameAt(0), QStringLiteral("layer.text"));

        // Undo properly restores original text before typing started
        history.undo();
        QCOMPARE(layer->content, QStringLiteral("Primeiro Texto"));

        // Redo properly restores final typed text
        history.redo();
        QCOMPARE(layer->content, QStringLiteral("Texto Atualizado"));
    }
};

QTEST_MAIN(TestUiPhase1)
#include "test_ui_phase1.moc"
