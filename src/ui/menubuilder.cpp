#include "menubuilder.h"

#include "actionmanager.h"
#include "localization/i18nservice.h"

namespace cc {

MenuBuilder::MenuBuilder(QMenuBar* menuBar, ActionManager* actions, QObject* parent)
    : QObject(parent)
    , m_menuBar(menuBar)
    , m_actions(actions)
{
}

void MenuBuilder::buildMenus(I18nService* i18n)
{
    if (!m_menuBar || !m_actions)
        return;

    // --- Arquivo ---
    fileMenu = m_menuBar->addMenu(QString());
    fileMenu->addAction(m_actions->newAction);
    fileMenu->addAction(m_actions->openAction);
    openRecentMenu = fileMenu->addMenu(QString());
    fileMenu->addAction(m_actions->saveAction);
    fileMenu->addAction(m_actions->saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actions->importAction);
    fileMenu->addAction(m_actions->exportAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actions->quitAction);

    // --- Editar ---
    editMenu = m_menuBar->addMenu(QString());
    editMenu->addAction(m_actions->undoAction);
    editMenu->addAction(m_actions->redoAction);

    // --- Imagem ---
    imageMenu = m_menuBar->addMenu(QStringLiteral("Image"));
    imageMenu->addAction(m_actions->importAction);
    imageMenu->addAction(m_actions->exportAction);
    imageMenu->addSeparator();
    imageMenu->addAction(m_actions->flipHAction);
    imageMenu->addAction(m_actions->flipVAction);
    imageMenu->addAction(m_actions->rotateLeftAction);
    imageMenu->addAction(m_actions->rotateRightAction);
    imageMenu->addSeparator();
    imageMenu->addAction(m_actions->removeBgAiAction);
    imageMenu->addAction(m_actions->removeBgAiQuickAction);
    imageMenu->addAction(m_actions->mockupAction);
    imageMenu->addSeparator();
    imageMenu->addAction(m_actions->mergeSelectedAction);
    imageMenu->addAction(m_actions->mergeVisibleAction);
    imageMenu->addAction(m_actions->flattenAction);

    // --- Filtros ---
    filterMenu = m_menuBar->addMenu(QStringLiteral("Filter"));
    artisticMenu = filterMenu->addMenu(QStringLiteral("Artistic & Color"));
    toneMenu = filterMenu->addMenu(QStringLiteral("Tone & Special Effects"));

    for (int i = 0; i < m_actions->filterActions.size(); ++i) {
        if (i < 8)
            artisticMenu->addAction(m_actions->filterActions[i]);
        else
            toneMenu->addAction(m_actions->filterActions[i]);
    }

    // --- Seleção ---
    selectMenu = m_menuBar->addMenu(QStringLiteral("Select"));
    selectMenu->addAction(m_actions->selectAllAction);
    selectMenu->addAction(m_actions->deselectAction);
    selectMenu->addAction(m_actions->invertSelectionAction);
    selectMenu->addSeparator();
    selectMenu->addAction(m_actions->selectionOpsAction);

    modifyMenu = selectMenu->addMenu(QStringLiteral("Modify"));
    for (QAction* act : m_actions->modifyActions) {
        modifyMenu->addAction(act);
    }
    selectMenu->addAction(m_actions->colorRangeAction);

    // --- Camada ---
    layerMenu = m_menuBar->addMenu(QString());
    layerMenu->addAction(m_actions->addTextAction);
    layerMenu->addAction(m_actions->addFillLayerAction);
    layerMenu->addAction(m_actions->addAdjustmentLayerAction);

    addShapeMenu = layerMenu->addMenu(QString());
    addShapeMenu->addAction(m_actions->addRectAction);
    addShapeMenu->addAction(m_actions->addRoundedRectAction);
    addShapeMenu->addAction(m_actions->addEllipseAction);
    addShapeMenu->addAction(m_actions->addLineAction);
    addShapeMenu->addAction(m_actions->addArrowRightAction);
    addShapeMenu->addAction(m_actions->addArrowCurvedAction);
    addShapeMenu->addAction(m_actions->addStarAction);
    addShapeMenu->addAction(m_actions->addBadgeAction);
    addShapeMenu->addAction(m_actions->addSpeechBubbleAction);
    addShapeMenu->addAction(m_actions->addHeartAction);
    addShapeMenu->addSeparator();
    for (QAction* act : m_actions->addNewShapeActions) {
        addShapeMenu->addAction(act);
    }

    alignMenu = layerMenu->addMenu(QString());
    alignMenu->addAction(m_actions->alignLeftAction);
    alignMenu->addAction(m_actions->alignCenterXAction);
    alignMenu->addAction(m_actions->alignRightAction);
    alignMenu->addSeparator();
    alignMenu->addAction(m_actions->alignTopAction);
    alignMenu->addAction(m_actions->alignCenterYAction);
    alignMenu->addAction(m_actions->alignBottomAction);
    alignMenu->addSeparator();
    alignMenu->addAction(m_actions->alignCenterBothAction);
    alignMenu->addSeparator();
    alignMenu->addAction(m_actions->distributeHAction);
    alignMenu->addAction(m_actions->distributeVAction);

    layerMenu->addSeparator();
    layerMenu->addAction(m_actions->groupAction);
    layerMenu->addAction(m_actions->ungroupAction);
    layerMenu->addSeparator();
    layerMenu->addAction(m_actions->mergeSelectedAction);
    layerMenu->addAction(m_actions->mergeVisibleAction);
    layerMenu->addAction(m_actions->flattenAction);
    layerMenu->addSeparator();
    layerMenu->addAction(m_actions->textBehindSubjectAction);
    layerMenu->addSeparator();
    layerMenu->addAction(m_actions->lockLayerAction);
    layerMenu->addAction(m_actions->deleteAction);

    // --- Exibir ---
    viewMenu = m_menuBar->addMenu(QString());
    viewMenu->addAction(m_actions->zoomInAction);
    viewMenu->addAction(m_actions->zoomOutAction);
    viewMenu->addAction(m_actions->zoomFitAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actions->toggleGridAction);
    viewMenu->addAction(m_actions->toggleSnapAction);
    viewMenu->addAction(m_actions->toggleHistoryAction);

    safeZoneMenu = viewMenu->addMenu(QString());
    safeZoneMenu->addAction(m_actions->safeNoneAction);
    safeZoneMenu->addAction(m_actions->safeYouTubeAction);
    safeZoneMenu->addAction(m_actions->safeInstagramAction);
    safeZoneMenu->addAction(m_actions->safeTikTokAction);

    panelsMenu = viewMenu->addMenu(QString());
    panelsMenu->addAction(m_actions->toggleToolsBarAction);
    panelsMenu->addAction(m_actions->toggleRightPanelAction);
    panelsMenu->addSeparator();
    panelsMenu->addAction(m_actions->toggleLayersDockAction);
    panelsMenu->addAction(m_actions->toggleTextDockAction);
    panelsMenu->addAction(m_actions->toggleShapeDockAction);
    panelsMenu->addAction(m_actions->toggleImageDockAction);

    // --- Ferramentas ---
    toolsMenu = m_menuBar->addMenu(QString());
    selectionToolsMenu = toolsMenu->addMenu(QString());
    selectionToolsMenu->addAction(m_actions->toolSelectAction);
    selectionToolsMenu->addAction(m_actions->toolMarqueeAction);
    selectionToolsMenu->addAction(m_actions->toolScissorsAction);
    selectionToolsMenu->addAction(m_actions->toolWandAction);
    selectionToolsMenu->addAction(m_actions->toolCropAction);

    retouchToolsMenu = toolsMenu->addMenu(QString());
    retouchToolsMenu->addAction(m_actions->toolPaintAction);
    retouchToolsMenu->addAction(m_actions->toolGradientAction);
    retouchToolsMenu->addAction(m_actions->toolFloodAction);
    retouchToolsMenu->addAction(m_actions->toolCloneAction);
    retouchToolsMenu->addAction(m_actions->toolRetouchAction);
    retouchToolsMenu->addAction(m_actions->toolHealingAction);
    retouchToolsMenu->addAction(m_actions->toolEyedropperAction);

    createToolsMenu = toolsMenu->addMenu(QString());
    createToolsMenu->addAction(m_actions->addTextAction);
    createToolsMenu->addAction(m_actions->addRectAction);

    // --- Janela ---
    windowMenu = m_menuBar->addMenu(QStringLiteral("Window"));
    windowMenu->addAction(m_actions->toggleLayersDockAction);
    windowMenu->addAction(m_actions->toggleTextDockAction);
    windowMenu->addAction(m_actions->toggleShapeDockAction);
    windowMenu->addAction(m_actions->toggleImageDockAction);
    windowMenu->addAction(m_actions->toggleHistoryAction);

    // --- Configurações & Ajuda ---
    settingsMenu = m_menuBar->addMenu(QString());
    settingsMenu->addAction(m_actions->settingsAction);

    helpMenu = m_menuBar->addMenu(QString());
    helpMenu->addAction(m_actions->startScreenAction);
    helpMenu->addSeparator();
    helpMenu->addAction(m_actions->aboutAction);
    helpMenu->addAction(m_actions->aboutQtAction);

    retranslate(i18n);
}

void MenuBuilder::retranslate(I18nService* i18n)
{
    if (!i18n)
        return;

    if (fileMenu)
        fileMenu->setTitle(i18n->t("common", "menu.file"));
    if (openRecentMenu)
        openRecentMenu->setTitle(i18n->t("common", "menu.file.openRecent"));
    if (editMenu)
        editMenu->setTitle(i18n->t("common", "menu.edit"));
    if (imageMenu)
        imageMenu->setTitle(i18n->t("common", "menu.image"));
    if (filterMenu)
        filterMenu->setTitle(i18n->t("common", "menu.filter"));
    if (artisticMenu)
        artisticMenu->setTitle(i18n->t("editor", "filters.artistic"));
    if (toneMenu)
        toneMenu->setTitle(i18n->t("editor", "filters.tone"));
    if (selectMenu)
        selectMenu->setTitle(i18n->t("common", "menu.select"));
    if (modifyMenu)
        modifyMenu->setTitle(i18n->t("editor", "selection.modify.title"));
    if (layerMenu)
        layerMenu->setTitle(i18n->t("common", "menu.layer"));
    if (addShapeMenu)
        addShapeMenu->setTitle(i18n->t("common", "menu.layer.addShape"));
    if (alignMenu)
        alignMenu->setTitle(i18n->t("common", "menu.layer.align"));
    if (viewMenu)
        viewMenu->setTitle(i18n->t("common", "menu.view"));
    if (safeZoneMenu)
        safeZoneMenu->setTitle(i18n->t("common", "menu.view.safeZones"));
    if (panelsMenu)
        panelsMenu->setTitle(i18n->t("common", "menu.view.panels"));
    if (toolsMenu)
        toolsMenu->setTitle(i18n->t("editor", "tools.title"));
    if (selectionToolsMenu)
        selectionToolsMenu->setTitle(i18n->t("common", "menu.tools.selection"));
    if (retouchToolsMenu)
        retouchToolsMenu->setTitle(i18n->t("common", "menu.tools.retouch"));
    if (createToolsMenu)
        createToolsMenu->setTitle(i18n->t("common", "menu.tools.create"));
    if (windowMenu)
        windowMenu->setTitle(i18n->t("common", "menu.window"));
    if (settingsMenu)
        settingsMenu->setTitle(i18n->t("common", "menu.settings"));
    if (helpMenu)
        helpMenu->setTitle(i18n->t("common", "menu.help"));
}

} // namespace cc
