#include "actionmanager.h"
#include <QMap>
#include <QSettings>

#include <QKeySequence>
#include "localization/i18nservice.h"
#include "themeicons.h"

namespace cc {

const NewShapeDef kNewShapeDefs[8] = {
    {ShapeKind::Triangle,   "triangle",   "Triângulo"},
    {ShapeKind::Diamond,    "diamond",    "Losango"},
    {ShapeKind::Pentagon,   "pentagon",   "Pentágono"},
    {ShapeKind::Hexagon,    "hexagon",    "Hexágono"},
    {ShapeKind::Cross,      "cross",      "Cruz"},
    {ShapeKind::Checkmark,  "checkmark",  "Marca de verificação"},
    {ShapeKind::Cloud,      "cloud",      "Nuvem"},
    {ShapeKind::Explosion,  "explosion",  "Explosão (Burst)"},
};

ActionManager::ActionManager(QObject* parent)
    : QObject(parent)
{
}

void ActionManager::createActions(I18nService* i18n)
{
    // --- Arquivo ---
    newAction = new QAction(this);
    newAction->setIcon(ThemeIcons::actionNewDocument());
    newAction->setShortcut(QKeySequence::New);

    openAction = new QAction(this);
    openAction->setIcon(ThemeIcons::actionOpenFolder());
    openAction->setShortcut(QKeySequence::Open);

    saveAction = new QAction(this);
    saveAction->setIcon(ThemeIcons::actionSave());
    saveAction->setShortcut(QKeySequence::Save);

    saveAsAction = new QAction(this);
    saveAsAction->setIcon(ThemeIcons::actionSaveAs());
    saveAsAction->setShortcut(QKeySequence::SaveAs);

    importAction = new QAction(this);
    importAction->setIcon(ThemeIcons::actionImport());
    importAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+I")));

    exportAction = new QAction(this);
    exportAction->setIcon(ThemeIcons::actionExport());
    exportAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));

    exportDocumentPngAction = new QAction(this);
    exportDocumentPngAction->setIcon(ThemeIcons::actionExport());
    exportDocumentPngAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+E")));

    quickExportLayerPngAction = new QAction(this);
    quickExportLayerPngAction->setIcon(ThemeIcons::actionExport());
    quickExportLayerPngAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+E")));

    batchAction = new QAction(this);
    batchAction->setIcon(ThemeIcons::actionExport());
    batchAction->setText(i18n->t("common", "menu.file.batch"));
    batchAction->setToolTip(i18n->t("common", "menu.file.batch.tooltip"));

    workspaceLayoutAction = new QAction(this);
    workspaceLayoutAction->setIcon(ThemeIcons::actionSettings());
    workspaceLayoutAction->setText(i18n->t("common", "menu.workspace.layout"));
    workspaceLayoutAction->setToolTip(i18n->t("common", "menu.workspace.layout.tooltip"));

    quitAction = new QAction(this);
    quitAction->setIcon(ThemeIcons::actionQuit());

    // --- Edição & Histórico ---
    undoAction = new QAction(this);
    undoAction->setIcon(ThemeIcons::actionUndo());
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setEnabled(false);

    redoAction = new QAction(this);
    redoAction->setIcon(ThemeIcons::actionRedo());
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setEnabled(false);

    // --- Área de transferência ---
    copyAction = new QAction(this);
    copyAction->setShortcut(QKeySequence::Copy);   // Ctrl+C

    cutAction = new QAction(this);
    cutAction->setShortcut(QKeySequence::Cut);     // Ctrl+X

    pasteAction = new QAction(this);
    pasteAction->setShortcut(QKeySequence::Paste); // Ctrl+V

    pasteIntoAction = new QAction(this);
    pasteIntoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));

    // --- Zoom & Exibição ---
    zoomInAction = new QAction(this);
    zoomInAction->setIcon(ThemeIcons::actionZoomIn());
    zoomInAction->setShortcut(QKeySequence::ZoomIn);

    zoomOutAction = new QAction(this);
    zoomOutAction->setIcon(ThemeIcons::actionZoomOut());
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);

    zoomFitAction = new QAction(this);
    zoomFitAction->setIcon(ThemeIcons::actionZoomFit());
    zoomFitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));

    toggleGridAction = new QAction(this);
    toggleGridAction->setCheckable(true);
    toggleGridAction->setChecked(false);
    toggleGridAction->setIcon(ThemeIcons::actionGrid());
    toggleGridAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+'")));

    toggleSnapAction = new QAction(this);
    toggleSnapAction->setCheckable(true);
    toggleSnapAction->setChecked(true);
    toggleSnapAction->setIcon(ThemeIcons::actionSnap());

    toggleHistoryAction = new QAction(this);
    toggleHistoryAction->setCheckable(true);
    toggleHistoryAction->setIcon(ThemeIcons::actionHistory());
    toggleHistoryAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+H")));

    toggleRulersAction = new QAction(this);
    toggleRulersAction->setCheckable(true);
    toggleGuidesAction = new QAction(this);
    toggleGuidesAction->setCheckable(true);
    toggleGuidesAction->setChecked(true);
    newGuideAction = new QAction(this);
    clearGuidesAction = new QAction(this);
    toggleNavigatorAction = new QAction(this);
    toggleNavigatorAction->setCheckable(true);
    toggleSwatchesAction = new QAction(this);
    toggleSwatchesAction->setCheckable(true);

    toggleToolsBarAction = new QAction(this);
    toggleToolsBarAction->setCheckable(true);
    toggleToolsBarAction->setChecked(true);
    toggleToolsBarAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+B")));

    toggleRightPanelAction = new QAction(this);
    toggleRightPanelAction->setCheckable(true);
    toggleRightPanelAction->setChecked(true);
    toggleRightPanelAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+J")));

    toggleLayersDockAction = new QAction(this);
    toggleLayersDockAction->setCheckable(true);
    toggleLayersDockAction->setChecked(true);

    toggleTextDockAction = new QAction(this);
    toggleTextDockAction->setCheckable(true);
    toggleTextDockAction->setChecked(false);

    toggleShapeDockAction = new QAction(this);
    toggleShapeDockAction->setCheckable(true);
    toggleShapeDockAction->setChecked(false);

    toggleImageDockAction = new QAction(this);
    toggleImageDockAction->setCheckable(true);
    toggleImageDockAction->setChecked(false);

    // --- Zonas Seguras ---
    auto* safeGroup = new QActionGroup(this);
    safeGroup->setExclusive(true);
    auto makeSafeAction = [this, safeGroup](int /*mode*/) {
        auto* act = new QAction(this);
        act->setCheckable(true);
        safeGroup->addAction(act);
        return act;
    };
    safeNoneAction = makeSafeAction(0);
    safeNoneAction->setChecked(true);
    safeYouTubeAction = makeSafeAction(1);
    safeInstagramAction = makeSafeAction(2);
    safeTikTokAction = makeSafeAction(3);

    // --- Camadas: Criação ---
    addTextAction = new QAction(this);
    addTextAction->setIcon(ThemeIcons::toolText());
    addTextAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));

    addFillLayerAction = new QAction(this);
    addFillLayerAction->setIcon(ThemeIcons::toolGradient());

    addAdjustmentLayerAction = new QAction(this);
    addAdjustmentLayerAction->setIcon(ThemeIcons::actionHistory());

    addRectAction = new QAction(this);
    addRectAction->setIcon(ThemeIcons::toolShape());
    addRectAction->setShortcut(QKeySequence(QStringLiteral("R")));

    addRoundedRectAction = new QAction(this);
    addRoundedRectAction->setIcon(ThemeIcons::toolShape());

    addEllipseAction = new QAction(this);
    addEllipseAction->setIcon(ThemeIcons::toolShape());
    addEllipseAction->setShortcut(QKeySequence(QStringLiteral("O")));

    addLineAction = new QAction(this);
    addLineAction->setIcon(ThemeIcons::toolShape());
    addLineAction->setShortcut(QKeySequence(QStringLiteral("L")));

    addArrowRightAction = new QAction(this);
    addArrowRightAction->setIcon(ThemeIcons::toolShape());

    addArrowCurvedAction = new QAction(this);
    addArrowCurvedAction->setIcon(ThemeIcons::toolShape());

    addStarAction = new QAction(this);
    addStarAction->setIcon(ThemeIcons::toolShape());

    addBadgeAction = new QAction(this);
    addBadgeAction->setIcon(ThemeIcons::toolShape());

    addSpeechBubbleAction = new QAction(this);
    addSpeechBubbleAction->setIcon(ThemeIcons::toolShape());

    addHeartAction = new QAction(this);
    addHeartAction->setIcon(ThemeIcons::toolShape());

    addNewShapeActions.clear();
    for (int i = 0; i < kNewShapeCount; ++i) {
        auto* act = new QAction(this);
        act->setIcon(ThemeIcons::toolShape());
        addNewShapeActions.append(act);
    }

    // --- Camadas: Alinhamento & Distribuição ---
    alignLeftAction = new QAction(this);
    alignLeftAction->setIcon(ThemeIcons::actionAlignLeft());

    alignCenterXAction = new QAction(this);
    alignCenterXAction->setIcon(ThemeIcons::actionAlignCenter());

    alignRightAction = new QAction(this);
    alignRightAction->setIcon(ThemeIcons::actionAlignRight());

    alignTopAction = new QAction(this);
    alignTopAction->setIcon(ThemeIcons::actionAlignTop());

    alignCenterYAction = new QAction(this);
    alignCenterYAction->setIcon(ThemeIcons::actionAlignMiddle());

    alignBottomAction = new QAction(this);
    alignBottomAction->setIcon(ThemeIcons::actionAlignBottom());

    alignCenterBothAction = new QAction(this);
    alignCenterBothAction->setIcon(ThemeIcons::actionAlignCenter());

    distributeHAction = new QAction(this);
    distributeHAction->setIcon(ThemeIcons::actionDistributeH());

    distributeVAction = new QAction(this);
    distributeVAction->setIcon(ThemeIcons::actionDistributeV());

    // --- Camadas: Organização ---
    groupAction = new QAction(this);
    groupAction->setIcon(ThemeIcons::layerTypeGroup());
    groupAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));

    ungroupAction = new QAction(this);
    ungroupAction->setIcon(ThemeIcons::layerTypeGroup());
    ungroupAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));

    mergeSelectedAction = new QAction(this);
    mergeDownAction = new QAction(this);
    mergeVisibleAction = new QAction(this);
    flattenAction = new QAction(this);
    duplicateLayerAction = new QAction(this);
    duplicateLayerAction->setIcon(ThemeIcons::actionDuplicate());
    rasterizeLayerAction = new QAction(this);
    rasterizeLayerAction->setIcon(ThemeIcons::layerTypeImage());
    textBehindSubjectAction = new QAction(this);

    flipHAction = new QAction(this);
    flipHAction->setIcon(ThemeIcons::actionFlipH());

    flipVAction = new QAction(this);
    flipVAction->setIcon(ThemeIcons::actionFlipV());

    rotateLeftAction = new QAction(this);
    rotateLeftAction->setIcon(ThemeIcons::actionRotateLeft());

    rotateRightAction = new QAction(this);
    rotateRightAction->setIcon(ThemeIcons::actionRotateRight());

    // --- Tela (canvas) ---
    imageSizeAction = new QAction(this);
    canvasSizeAction = new QAction(this);
    rotateCanvasCwAction = new QAction(this);
    rotateCanvasCcwAction = new QAction(this);
    rotateCanvas180Action = new QAction(this);

    deleteAction = new QAction(this);
    deleteAction->setIcon(ThemeIcons::actionDelete());
    deleteAction->setShortcut(QKeySequence::Delete);

    lockLayerAction = new QAction(this);
    lockLayerAction->setIcon(ThemeIcons::layerLock(true));
    lockLayerAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+/")));

    createClippingMaskAction = new QAction(this);
    createClippingMaskAction->setIcon(ThemeIcons::iconClipMask());
    createClippingMaskAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+G")));

    releaseClippingMaskAction = new QAction(this);
    releaseClippingMaskAction->setIcon(ThemeIcons::iconClipMask());
    releaseClippingMaskAction->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+Shift+Alt+G")));

    // --- Seleção ---
    selectAllAction = new QAction(this);
    selectAllAction->setShortcut(QKeySequence::SelectAll);

    deselectAction = new QAction(this);
    deselectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));

    invertSelectionAction = new QAction(this);
    invertSelectionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));

    reselectAction = new QAction(this);
    reselectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));

    transformSelectionAction = new QAction(this);
    saveSelectionAction = new QAction(this);

    selectionOpsAction = new QAction(this);
    colorRangeAction = new QAction(this);

    // --- Edição sobre a seleção ativa (menu Editar) ---
    fillSelectionAction = new QAction(this);
    strokeSelectionAction = new QAction(this);
    clearSelectionAction = new QAction(this);

    // Selecionar Sujeito (IA): mesmo motor do recorte de fundo, mas o
    // resultado é uma seleção de píxeis em vez de descartar o fundo.
    selectSubjectAction = new QAction(this);
    selectSubjectAction->setIcon(ThemeIcons::toolWand());
    selectSubjectAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+S")));

    modifyActions.clear();
    for (int i = 0; i < 5; ++i) {
        modifyActions.append(new QAction(this));
    }

    // --- Ferramentas ---
    toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);

    auto makeTool = [this](const QIcon& icon, const QKeySequence& shortcut) {
        auto* act = new QAction(this);
        act->setIcon(icon);
        act->setCheckable(true);
        if (!shortcut.isEmpty())
            act->setShortcut(shortcut);
        toolGroup->addAction(act);
        return act;
    };

    toolSelectAction = makeTool(ThemeIcons::toolSelect(), QKeySequence(QStringLiteral("V")));
    toolSelectAction->setChecked(true);
    toolMarqueeAction = makeTool(ThemeIcons::toolMarquee(), QKeySequence(QStringLiteral("M")));
    toolScissorsAction = makeTool(ThemeIcons::toolScissors(), QKeySequence(QStringLiteral("X")));
    toolWandAction = makeTool(ThemeIcons::toolWand(), QKeySequence(QStringLiteral("W")));
    toolCropAction = makeTool(ThemeIcons::toolCrop(), QKeySequence(QStringLiteral("C")));
    toolPaintAction = makeTool(ThemeIcons::toolPaint(), QKeySequence(QStringLiteral("B")));
    toolGradientAction = makeTool(ThemeIcons::toolGradient(), QKeySequence(QStringLiteral("G")));
    toolFloodAction = makeTool(ThemeIcons::toolFlood(), QKeySequence(QStringLiteral("Shift+G")));
    toolCloneAction = makeTool(ThemeIcons::toolClone(), QKeySequence(QStringLiteral("S")));
    toolRetouchAction = makeTool(ThemeIcons::toolRetouch(), QKeySequence(QStringLiteral("U")));
    toolHealingAction = makeTool(ThemeIcons::toolHealing(), QKeySequence(QStringLiteral("J")));
    toolEyedropperAction = makeTool(ThemeIcons::toolEyedropper(), QKeySequence(QStringLiteral("I")));
    toolPenAction = makeTool(ThemeIcons::toolPen(), QKeySequence(QStringLiteral("P")));
    toolVectorNodesAction = makeTool(ThemeIcons::toolVectorNodes(), QKeySequence(QStringLiteral("A")));

    // --- Filtros & IA ---
    removeBgAiAction = new QAction(this);
    removeBgAiQuickAction = new QAction(this);

    mockupAction = new QAction(this);
    mockupAction->setEnabled(false);

    editInMiniPaintAction = new QAction(this);
    editInMiniPaintAction->setIcon(ThemeIcons::actionExport());
    editInMiniPaintAction->setEnabled(false);

    filterActions.clear();
    for (int i = 0; i < 11; ++i) {
        filterActions.append(new QAction(this));
    }

    // --- Filtros com parâmetros (Blur / Noise / Distort) ---
    blurGaussianAction = new QAction(this);
    blurMotionAction = new QAction(this);
    noiseAddAction = new QAction(this);
    distortTwirlAction = new QAction(this);
    distortPinchAction = new QAction(this);
    distortRippleAction = new QAction(this);

    // Ajustes (submenu Image ▸ Adjustments), na ordem de kAdjustmentTypes
    // do diálogo.
    adjustmentActions.clear();
    for (int i = 0; i < 11; ++i) {
        adjustmentActions.append(new QAction(this));
    }

    // --- Janela & Ajuda ---
    settingsAction = new QAction(this);
    settingsAction->setIcon(ThemeIcons::actionSettings());
    settingsAction->setShortcut(QKeySequence::Preferences);

    shortcutsAction = new QAction(this);

    commandPaletteAction = new QAction(this);
    commandPaletteAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+K")));

    quickTourAction = new QAction(this);

    backgroundsAction = new QAction(this);
    backgroundsAction->setIcon(ThemeIcons::actionNewDocument());
    // Ctrl+B já é da barra de ferramentas (View): conflito de atalho fazia
    // os dois dispararem. Ctrl+Shift+B fica livre.
    backgroundsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+B")));

    layerCompsAction = new QAction(this);
    webSlicesAction = new QAction(this);
    toggleAdvancedModeAction = new QAction(this);
    refineEdgeAction = new QAction(this);
    toggleAdvancedModeAction->setCheckable(true);

    startScreenAction = new QAction(this);
    startScreenAction->setIcon(ThemeIcons::actionNewDocument());

    sampleProjectsAction = new QAction(this);
    sampleProjectsAction->setIcon(ThemeIcons::actionOpenFolder());

    aboutAction = new QAction(this);
    aboutAction->setIcon(ThemeIcons::actionAbout());

    aboutQtAction = new QAction(this);

    retranslate(i18n);
    registerShortcutActions();
    for (auto it = m_shortcutActions.cbegin(); it != m_shortcutActions.cend(); ++it)
        m_defaultShortcuts.insert(it.key(), it.value()->shortcut());
    loadCustomShortcuts();
}

void ActionManager::retranslate(I18nService* i18n)
{
    if (!i18n)
        return;

    // --- Arquivo ---
    newAction->setText(i18n->t("common", "menu.file.new"));
    newAction->setToolTip(newAction->text() + QStringLiteral(" (Ctrl+N)"));

    openAction->setText(i18n->t("common", "menu.file.open"));
    openAction->setToolTip(openAction->text() + QStringLiteral(" (Ctrl+O)"));

    saveAction->setText(i18n->t("common", "menu.file.save"));
    saveAction->setToolTip(saveAction->text() + QStringLiteral(" (Ctrl+S)"));

    saveAsAction->setText(i18n->t("common", "menu.file.saveAs"));

    importAction->setText(i18n->t("common", "menu.file.import"));
    importAction->setToolTip(importAction->text() + QStringLiteral(" (Ctrl+I)"));

    exportAction->setText(i18n->t("common", "menu.file.export"));
    exportAction->setToolTip(exportAction->text() + QStringLiteral(" (Ctrl+E)"));
    exportDocumentPngAction->setText(i18n->t("editor", "export.quickPng"));
    exportDocumentPngAction->setToolTip(i18n->t("editor", "export.quickPng.tooltip"));
    quickExportLayerPngAction->setText(i18n->t("editor", "hints.layerQuickExport"));
    quickExportLayerPngAction->setToolTip(i18n->t("common", "panel.quickExportPng.tooltip"));

    batchAction->setText(i18n->t("common", "menu.file.batch"));
    batchAction->setToolTip(i18n->t("common", "menu.file.batch.tooltip"));

    quitAction->setText(i18n->t("common", "menu.file.quit"));

    // --- Edição ---
    undoAction->setText(i18n->t("common", "menu.edit.undo"));
    undoAction->setToolTip(undoAction->text() + QStringLiteral(" (Ctrl+Z)"));

    redoAction->setText(i18n->t("common", "menu.edit.redo"));
    redoAction->setToolTip(redoAction->text() + QStringLiteral(" (Ctrl+Y)"));

    copyAction->setText(i18n->t("common", "menu.edit.copy"));
    copyAction->setToolTip(copyAction->text() + QStringLiteral(" (Ctrl+C)"));
    cutAction->setText(i18n->t("common", "menu.edit.cut"));
    cutAction->setToolTip(cutAction->text() + QStringLiteral(" (Ctrl+X)"));
    pasteAction->setText(i18n->t("common", "menu.edit.paste"));
    pasteAction->setToolTip(pasteAction->text() + QStringLiteral(" (Ctrl+V)"));
    pasteIntoAction->setText(i18n->t("common", "menu.edit.pasteInto"));
    pasteIntoAction->setToolTip(pasteIntoAction->text() + QStringLiteral(" (Ctrl+Shift+V)"));

    fillSelectionAction->setText(i18n->t("common", "menu.edit.fill"));
    strokeSelectionAction->setText(i18n->t("common", "menu.edit.stroke"));
    clearSelectionAction->setText(i18n->t("common", "menu.edit.clear"));
    clearSelectionAction->setToolTip(i18n->t("common", "menu.edit.clear.tooltip"));

    // --- Zoom ---
    const QString zInText = i18n->t("common", "action.zoomIn");
    zoomInAction->setText(zInText);
    zoomInAction->setToolTip(zInText + QStringLiteral(" (Ctrl++)"));

    const QString zOutText = i18n->t("common", "action.zoomOut");
    zoomOutAction->setText(zOutText);
    zoomOutAction->setToolTip(zOutText + QStringLiteral(" (Ctrl+-)"));

    const QString zFitText = i18n->t("common", "action.zoomFit");
    zoomFitAction->setText(zFitText);
    zoomFitAction->setToolTip(zFitText + QStringLiteral(" (Ctrl+0)"));

    toggleGridAction->setText(i18n->t("common", "menu.view.showGrid"));
    toggleGridAction->setToolTip(toggleGridAction->text() + QStringLiteral(" (Ctrl+')"));

    toggleSnapAction->setText(i18n->t("common", "menu.view.snapToGrid"));

    toggleHistoryAction->setText(i18n->t("common", "menu.view.history"));
    toggleHistoryAction->setToolTip(toggleHistoryAction->text() + QStringLiteral(" (Ctrl+H)"));

    toggleRulersAction->setText(i18n->t("common", "menu.view.rulers"));
    toggleGuidesAction->setText(i18n->t("editor", "guides.show"));
    newGuideAction->setText(i18n->t("editor", "guides.new"));
    clearGuidesAction->setText(i18n->t("editor", "guides.clear"));
    toggleNavigatorAction->setText(i18n->t("common", "menu.view.panels.navigator"));
    toggleSwatchesAction->setText(i18n->t("common", "menu.view.panels.swatches"));

    toggleToolsBarAction->setText(i18n->t("common", "menu.view.toggleToolsBar"));
    toggleToolsBarAction->setToolTip(toggleToolsBarAction->text() + QStringLiteral(" (Ctrl+B)"));

    toggleRightPanelAction->setText(i18n->t("common", "menu.view.toggleRightPanel"));
    toggleRightPanelAction->setToolTip(toggleRightPanelAction->text() + QStringLiteral(" (Ctrl+J)"));

    toggleLayersDockAction->setText(i18n->t("common", "panel.layers"));
    toggleTextDockAction->setText(i18n->t("editor", "text.title"));
    toggleShapeDockAction->setText(i18n->t("editor", "shape.title"));
    toggleImageDockAction->setText(i18n->t("common", "panel.image"));

    // --- Zonas Seguras ---
    safeNoneAction->setText(i18n->t("common", "menu.view.safe.none"));
    safeYouTubeAction->setText(i18n->t("common", "menu.view.safe.youtube"));
    safeInstagramAction->setText(i18n->t("common", "menu.view.safe.instagram"));
    safeTikTokAction->setText(i18n->t("common", "menu.view.safe.tiktok"));

    // --- Camadas: Criação ---
    addTextAction->setText(i18n->t("common", "menu.layer.addText"));
    addTextAction->setToolTip(QStringLiteral("%1 (T)").arg(addTextAction->text()));

    addFillLayerAction->setText(i18n->t("editor", "layer.newFillLayer"));
    addAdjustmentLayerAction->setText(i18n->t("editor", "layer.newAdjustmentLayer"));

    addRectAction->setText(i18n->t("common", "menu.layer.shape.rect"));
    addRectAction->setToolTip(QStringLiteral("%1 (R)").arg(addRectAction->text()));

    addRoundedRectAction->setText(i18n->t("common", "menu.layer.shape.roundedRect"));
    addEllipseAction->setText(i18n->t("common", "menu.layer.shape.ellipse"));
    addLineAction->setText(i18n->t("common", "menu.layer.shape.line"));
    addArrowRightAction->setText(i18n->t("common", "menu.layer.shape.arrowRight"));
    addArrowCurvedAction->setText(i18n->t("common", "menu.layer.shape.arrowCurved"));
    addStarAction->setText(i18n->t("common", "menu.layer.shape.star"));
    addBadgeAction->setText(i18n->t("common", "menu.layer.shape.badge"));
    addSpeechBubbleAction->setText(i18n->t("common", "menu.layer.shape.speechBubble"));
    addHeartAction->setText(i18n->t("common", "menu.layer.shape.heart"));

    for (int i = 0; i < addNewShapeActions.size() && i < kNewShapeCount; ++i) {
        addNewShapeActions[i]->setText(
            i18n->t("editor", QStringLiteral("shape.kind.%1")
                                  .arg(QLatin1String(kNewShapeDefs[i].key))));
    }

    // --- Alinhamento ---
    alignLeftAction->setText(i18n->t("common", "menu.layer.align.left"));
    alignCenterXAction->setText(i18n->t("common", "menu.layer.align.centerX"));
    alignRightAction->setText(i18n->t("common", "menu.layer.align.right"));
    alignTopAction->setText(i18n->t("common", "menu.layer.align.top"));
    alignCenterYAction->setText(i18n->t("common", "menu.layer.align.centerY"));
    alignBottomAction->setText(i18n->t("common", "menu.layer.align.bottom"));
    alignCenterBothAction->setText(i18n->t("common", "menu.layer.align.centerBoth"));
    distributeHAction->setText(i18n->t("common", "menu.layer.distribute.horizontal"));
    distributeVAction->setText(i18n->t("common", "menu.layer.distribute.vertical"));

    // --- Operações de Camadas ---
    groupAction->setText(i18n->t("common", "menu.layer.group"));
    ungroupAction->setText(i18n->t("common", "menu.layer.ungroup"));
    mergeSelectedAction->setText(i18n->t("common", "action.mergeSelected"));
    mergeDownAction->setText(i18n->t("common", "action.mergeDown"));
    mergeVisibleAction->setText(i18n->t("common", "action.mergeVisible"));
    flattenAction->setText(i18n->t("common", "action.flatten"));
    duplicateLayerAction->setText(i18n->t("common", "action.duplicateLayer"));
    rasterizeLayerAction->setText(i18n->t("common", "action.rasterizeLayer"));
    textBehindSubjectAction->setText(i18n->t("common", "action.textBehindSubject"));

    flipHAction->setText(i18n->t("common", "menu.layer.flipH"));
    flipVAction->setText(i18n->t("common", "menu.layer.flipV"));

    imageSizeAction->setText(i18n->t("common", "menu.image.imageSize"));
    canvasSizeAction->setText(i18n->t("common", "menu.image.canvasSize"));
    rotateCanvasCwAction->setText(i18n->t("common", "menu.image.rotateCw"));
    rotateCanvasCcwAction->setText(i18n->t("common", "menu.image.rotateCcw"));
    rotateCanvas180Action->setText(i18n->t("common", "menu.image.rotate180"));
    rotateLeftAction->setText(i18n->t("common", "menu.layer.rotateLeft"));
    rotateRightAction->setText(i18n->t("common", "menu.layer.rotateRight"));
    deleteAction->setText(i18n->t("common", "menu.layer.delete"));
    lockLayerAction->setText(i18n->t("common", "action.toggleLayerLock"));
    lockLayerAction->setToolTip(lockLayerAction->text() + QStringLiteral(" (Ctrl+/)"));
    createClippingMaskAction->setText(i18n->t("common", "action.createClippingMask"));
    createClippingMaskAction->setToolTip(createClippingMaskAction->text()
                                         + QStringLiteral(" (Ctrl+Alt+G)"));
    releaseClippingMaskAction->setText(i18n->t("common", "action.releaseClippingMask"));
    releaseClippingMaskAction->setToolTip(releaseClippingMaskAction->text()
                                         + QStringLiteral(" (Ctrl+Shift+Alt+G)"));

    // --- Seleção ---
    selectAllAction->setText(i18n->t("common", "action.selectAll"));
    deselectAction->setText(i18n->t("editor", "selection.deselect"));
    invertSelectionAction->setText(i18n->t("editor", "selection.invert"));
    reselectAction->setText(i18n->t("editor", "selection.reselect"));
    reselectAction->setToolTip(reselectAction->text() + QStringLiteral(" (Ctrl+Shift+D)"));
    transformSelectionAction->setText(i18n->t("common", "action.transformSelection"));
    saveSelectionAction->setText(i18n->t("editor", "selection.save"));
    selectionOpsAction->setText(i18n->t("editor", "selection.operations"));
    selectionOpsAction->setToolTip(i18n->t("editor", "selection.operationsTooltip"));
    colorRangeAction->setText(i18n->t("editor", "selection.colorRange.title"));
    selectSubjectAction->setText(i18n->t("editor", "selection.subjectAi"));
    selectSubjectAction->setToolTip(i18n->t("editor", "selection.subjectAiTooltip"));

    static const char* const modifyKeys[] = {
        "selection.modify.expand", "selection.modify.contract",
        "selection.modify.border", "selection.modify.smooth",
        "selection.modify.feather",
    };
    for (int i = 0; i < modifyActions.size() && i < 5; ++i) {
        modifyActions[i]->setText(i18n->t("editor", QString::fromLatin1(modifyKeys[i])));
    }

    // --- Ferramentas e Dicas ---
    auto setToolHint = [i18n](QAction* act, const char* key, const QString& shortcut) {
        if (!act) return;
        const QString name = i18n->t("editor", QString::fromLatin1(key));
        act->setText(name);
        act->setToolTip(QStringLiteral("%1 (%2)").arg(name, shortcut));
    };

    setToolHint(toolSelectAction,     "tools.select",     QStringLiteral("V"));
    setToolHint(toolMarqueeAction,    "tools.marquee",    QStringLiteral("M"));
    setToolHint(toolScissorsAction,   "tools.scissors",   QStringLiteral("X"));
    setToolHint(toolWandAction,       "tools.wand",       QStringLiteral("W"));
    setToolHint(toolCropAction,       "tools.crop",       QStringLiteral("C"));
    setToolHint(toolPaintAction,      "tools.paintName",  QStringLiteral("B"));
    setToolHint(toolGradientAction,   "tools.gradient",   QStringLiteral("G"));
    setToolHint(toolFloodAction,      "tools.floodName",  QStringLiteral("Shift+G"));
    setToolHint(toolCloneAction,      "tools.clone",      QStringLiteral("S"));
    setToolHint(toolRetouchAction,    "tools.retouch",    QStringLiteral("U"));
    setToolHint(toolHealingAction,    "tools.healing",    QStringLiteral("J"));
    setToolHint(toolPenAction,        "tools.pen",        QStringLiteral("P"));
    setToolHint(toolVectorNodesAction, "tools.vectorNodes", QStringLiteral("A"));

    const QString eyedropperText = i18n->t("editor", "tools.eyedropper");
    toolEyedropperAction->setText(eyedropperText);
    toolEyedropperAction->setToolTip(QStringLiteral("%1 (I)").arg(eyedropperText));

    // --- IA & Filtros ---
    removeBgAiAction->setText(i18n->t("editor", "ai.removeBackground"));
    removeBgAiQuickAction->setText(i18n->t("editor", "ai.quickRemoveBackground"));
    mockupAction->setText(i18n->t("editor", "ai.mockup"));

    editInMiniPaintAction->setText(i18n->t("editor", "menu.image.editInMiniPaint"));
    editInMiniPaintAction->setToolTip(i18n->t("editor", "menu.image.editInMiniPaint.tooltip"));

    static const char* const filterKeys[] = {
        "filters.grayscale", "filters.sepia", "filters.vintage",
        "filters.highContrast", "filters.vividPop", "filters.cyberpunk",
        "filters.goldenHour", "filters.cinemaNoir", "filters.invert",
        "filters.posterize", "filters.threshold",
    };
    for (int i = 0; i < filterActions.size() && i < 11; ++i) {
        filterActions[i]->setText(i18n->t("editor", QString::fromLatin1(filterKeys[i])));
    }

    blurGaussianAction->setText(i18n->t("editor", "filters.blurGaussian"));
    blurMotionAction->setText(i18n->t("editor", "filters.blurMotion"));
    noiseAddAction->setText(i18n->t("editor", "filters.noiseAdd"));
    distortTwirlAction->setText(i18n->t("editor", "filters.distortTwirl"));
    distortPinchAction->setText(i18n->t("editor", "filters.distortPinch"));
    distortRippleAction->setText(i18n->t("editor", "filters.distortRipple"));

    static const char* const adjustmentKeys[] = {
        "adjustments.brightnessContrast", "adjustments.hueSaturation",
        "adjustments.shadowsHighlights", "adjustments.vignette",
        "adjustments.invert", "adjustments.posterize", "adjustments.threshold",
        "adjustments.levels", "adjustments.curves", "adjustments.channelMixer",
        "adjustments.gradientMap",
    };
    for (int i = 0; i < adjustmentActions.size() && i < 11; ++i) {
        adjustmentActions[i]->setText(i18n->t("editor", QString::fromLatin1(adjustmentKeys[i])));
    }

    // --- Janela, Ajuda & Configurações ---
    settingsAction->setText(i18n->t("common", "menu.settings.open"));
    shortcutsAction->setText(i18n->t("common", "menu.settings.shortcuts"));
    commandPaletteAction->setText(i18n->t("editor", "commandPalette.menu"));
    commandPaletteAction->setToolTip(i18n->t("editor", "commandPalette.tip"));
    quickTourAction->setText(i18n->t("editor", "tour.title"));
    quickTourAction->setToolTip(i18n->t("editor", "tour.restartHint"));
    backgroundsAction->setText(i18n->t("editor", "pack.action.browse"));
    backgroundsAction->setToolTip(i18n->t("editor", "pack.action.browse.tooltip"));
    layerCompsAction->setText(i18n->t("editor", "comps.menuOpen"));
    toggleAdvancedModeAction->setText(i18n->t("editor", "advancedMode.menu"));
    refineEdgeAction->setText(i18n->t("editor", "refineEdge.menuOpen"));
    toggleAdvancedModeAction->setToolTip(i18n->t("editor", "advancedMode.tip"));
    webSlicesAction->setText(i18n->t("editor", "webSlices.menuOpen"));
    startScreenAction->setText(i18n->t("common", "start.screenAction"));
    sampleProjectsAction->setText(i18n->t("common", "menu.help.sampleProjects"));
    aboutAction->setText(i18n->t("common", "menu.help.about"));
    aboutQtAction->setText(i18n->t("common", "menu.help.aboutQt"));
}

void ActionManager::selectToolByName(const QString& name)
{
    QAction* target = nullptr;
    if (name == QLatin1String("select"))          target = toolSelectAction;
    else if (name == QLatin1String("marquee"))    target = toolMarqueeAction;
    else if (name == QLatin1String("crop"))       target = toolCropAction;
    else if (name == QLatin1String("scissors"))   target = toolScissorsAction;
    else if (name == QLatin1String("wand"))       target = toolWandAction;
    else if (name == QLatin1String("clone"))      target = toolCloneAction;
    else if (name == QLatin1String("paint"))      target = toolPaintAction;
    else if (name == QLatin1String("gradient"))   target = toolGradientAction;
    else if (name == QLatin1String("flood"))      target = toolFloodAction;
    else if (name == QLatin1String("retouch"))    target = toolRetouchAction;
    else if (name == QLatin1String("healing"))    target = toolHealingAction;
    else if (name == QLatin1String("eyedropper")) target = toolEyedropperAction;
    else if (name == QLatin1String("pen"))       target = toolPenAction;
    else if (name == QLatin1String("vectornodes")) target = toolVectorNodesAction;
    if (target)
        target->trigger();
}

QMap<QString, QKeySequence> ActionManager::getAllShortcuts() const
{
    QMap<QString, QKeySequence> shortcuts;

    for (QAction* action : m_shortcutActions) {
        if (action && !action->text().isEmpty())
            shortcuts.insert(action->text(), action->shortcut());
    }

    return shortcuts;
}

bool ActionManager::setShortcut(const QString& actionName, const QKeySequence& key)
{
    for (auto it = m_shortcutActions.cbegin(); it != m_shortcutActions.cend(); ++it) {
        QAction* action = it.value();
        if (action && action->text() == actionName) {
            action->setShortcut(key);
            persistShortcut(it.key(), key);
            return true;
        }
    }
    return false;
}

QKeySequence ActionManager::getShortcut(const QString& actionName) const
{
    auto shortcuts = getAllShortcuts();
    return shortcuts.value(actionName, QKeySequence());
}

QKeySequence ActionManager::defaultShortcut(const QString& actionName) const
{
    for (auto it = m_shortcutActions.cbegin(); it != m_shortcutActions.cend(); ++it) {
        if (it.value() && it.value()->text() == actionName)
            return m_defaultShortcuts.value(it.key());
    }
    return {};
}

bool ActionManager::resetShortcut(const QString& actionName)
{
    for (auto it = m_shortcutActions.cbegin(); it != m_shortcutActions.cend(); ++it) {
        if (it.value() && it.value()->text() == actionName) {
            const QKeySequence shortcut = m_defaultShortcuts.value(it.key());
            it.value()->setShortcut(shortcut);
            persistShortcut(it.key(), shortcut);
            return true;
        }
    }
    return false;
}

void ActionManager::resetAllShortcuts()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("shortcuts"));
    settings.remove(QString());
    settings.endGroup();
    settings.sync();

    for (auto it = m_shortcutActions.cbegin(); it != m_shortcutActions.cend(); ++it) {
        if (it.value())
            it.value()->setShortcut(m_defaultShortcuts.value(it.key()));
    }
}

void ActionManager::registerShortcutActions()
{
    m_shortcutActions.clear();
    const auto add = [this](const char* id, QAction* action) {
        if (action)
            m_shortcutActions.insert(QLatin1String(id), action);
    };
    add("file.new", newAction); add("file.open", openAction); add("file.save", saveAction);
    add("file.saveAs", saveAsAction); add("file.import", importAction); add("file.export", exportAction);
    add("file.batch", batchAction); add("file.exportPng", exportDocumentPngAction); add("file.quit", quitAction);
    add("layer.quickExportPng", quickExportLayerPngAction);
    add("edit.undo", undoAction); add("edit.redo", redoAction);
    add("edit.copy", copyAction); add("edit.cut", cutAction);
    add("edit.paste", pasteAction); add("edit.pasteInto", pasteIntoAction);
    add("view.zoomIn", zoomInAction); add("view.zoomOut", zoomOutAction); add("view.zoomFit", zoomFitAction);
    add("view.grid", toggleGridAction); add("view.snap", toggleSnapAction); add("view.history", toggleHistoryAction);
    add("view.rulers", toggleRulersAction); add("view.guides", toggleGuidesAction);
    add("view.newGuide", newGuideAction); add("view.clearGuides", clearGuidesAction);
    add("panel.navigator", toggleNavigatorAction); add("panel.swatches", toggleSwatchesAction);
    add("view.toolsBar", toggleToolsBarAction); add("view.rightPanel", toggleRightPanelAction);
    add("panel.layers", toggleLayersDockAction); add("panel.text", toggleTextDockAction);
    add("panel.shape", toggleShapeDockAction); add("panel.image", toggleImageDockAction);
    add("layer.addText", addTextAction); add("layer.addFill", addFillLayerAction);
    add("layer.addAdjustment", addAdjustmentLayerAction); add("shape.rectangle", addRectAction);
    add("shape.roundedRectangle", addRoundedRectAction); add("shape.ellipse", addEllipseAction);
    add("shape.line", addLineAction); add("shape.arrow", addArrowRightAction); add("shape.curvedArrow", addArrowCurvedAction);
    add("shape.star", addStarAction); add("shape.badge", addBadgeAction); add("shape.speechBubble", addSpeechBubbleAction);
    add("shape.heart", addHeartAction); add("align.left", alignLeftAction); add("align.centerX", alignCenterXAction);
    add("align.right", alignRightAction); add("align.top", alignTopAction); add("align.centerY", alignCenterYAction);
    add("align.bottom", alignBottomAction); add("align.centerBoth", alignCenterBothAction);
    add("align.distributeH", distributeHAction); add("align.distributeV", distributeVAction);
    add("layer.group", groupAction); add("layer.ungroup", ungroupAction); add("layer.mergeSelected", mergeSelectedAction);
    add("layer.mergeDown", mergeDownAction); add("layer.mergeVisible", mergeVisibleAction);
    add("layer.flatten", flattenAction); add("layer.duplicate", duplicateLayerAction);
    add("layer.rasterize", rasterizeLayerAction); add("layer.textBehind", textBehindSubjectAction);
    add("transform.flipH", flipHAction); add("transform.flipV", flipVAction); add("transform.rotateLeft", rotateLeftAction);
    add("transform.rotateRight", rotateRightAction);
    add("image.imageSize", imageSizeAction); add("image.canvasSize", canvasSizeAction);
    add("image.rotateCanvasCw", rotateCanvasCwAction);
    add("image.rotateCanvasCcw", rotateCanvasCcwAction); add("image.rotateCanvas180", rotateCanvas180Action);
    add("layer.delete", deleteAction); add("layer.lock", lockLayerAction);
    add("layer.createClippingMask", createClippingMaskAction); add("layer.releaseClippingMask", releaseClippingMaskAction);
    add("select.all", selectAllAction); add("select.none", deselectAction); add("select.invert", invertSelectionAction);
    add("select.reselect", reselectAction); add("select.transform", transformSelectionAction);
    add("select.saveSelection", saveSelectionAction);
    add("edit.fill", fillSelectionAction); add("edit.stroke", strokeSelectionAction); add("edit.clear", clearSelectionAction);
    add("select.operations", selectionOpsAction); add("select.colorRange", colorRangeAction);
    add("select.subjectAi", selectSubjectAction);
    add("tool.select", toolSelectAction); add("tool.marquee", toolMarqueeAction); add("tool.crop", toolCropAction);
    add("tool.scissors", toolScissorsAction); add("tool.wand", toolWandAction); add("tool.clone", toolCloneAction);
    add("tool.paint", toolPaintAction); add("tool.gradient", toolGradientAction); add("tool.flood", toolFloodAction);
    add("tool.eyedropper", toolEyedropperAction); add("tool.pen", toolPenAction);
    add("tool.vectorNodes", toolVectorNodesAction); add("tool.retouch", toolRetouchAction); add("tool.healing", toolHealingAction);
    static const char* const adjustmentRegistry[] = {
        "adjustments.brightnessContrast", "adjustments.hueSaturation",
        "adjustments.shadowsHighlights", "adjustments.vignette",
        "adjustments.invert", "adjustments.posterize", "adjustments.threshold",
        "adjustments.levels", "adjustments.curves", "adjustments.channelMixer",
        "adjustments.gradientMap",
    };
    for (int i = 0; i < adjustmentActions.size() && i < 11; ++i)
        add(adjustmentRegistry[i], adjustmentActions[i]);
    add("filter.blurGaussian", blurGaussianAction); add("filter.blurMotion", blurMotionAction);
    add("filter.noiseAdd", noiseAddAction);
    add("filter.distortTwirl", distortTwirlAction); add("filter.distortPinch", distortPinchAction);
    add("filter.distortRipple", distortRippleAction);
    add("ai.removeBackground", removeBgAiAction); add("ai.quickRemoveBackground", removeBgAiQuickAction);
    add("ai.mockup", mockupAction); add("image.editInMiniPaint", editInMiniPaintAction); add("settings.open", settingsAction); add("settings.shortcuts", shortcutsAction);
    add("help.startScreen", startScreenAction); add("help.about", aboutAction); add("help.aboutQt", aboutQtAction);
    add("help.commandPalette", commandPaletteAction);
    add("help.quickTour", quickTourAction);
    add("layer.backgrounds", backgroundsAction);

    // Acções que ficavam de fora do registo (e portanto sem atalho
    // configurável nem pesquisa): formas extra, filtros artísticos/tonais,
    // Select ▸ Modify, zonas seguras e utilitários de janela/projeto.
    static const char* const newShapeRegistry[] = {
        "shape.triangle", "shape.diamond", "shape.pentagon", "shape.hexagon",
        "shape.cross", "shape.checkmark", "shape.cloud", "shape.explosion",
    };
    for (int i = 0; i < addNewShapeActions.size() && i < 8; ++i)
        add(newShapeRegistry[i], addNewShapeActions[i]);

    static const char* const artisticFilterRegistry[] = {
        "filter.grayscale", "filter.sepia", "filter.vintage",
        "filter.highContrast", "filter.vividPop", "filter.cyberpunk",
        "filter.goldenHour", "filter.cinemaNoir", "filter.invert",
        "filter.posterize", "filter.threshold",
    };
    for (int i = 0; i < filterActions.size() && i < 11; ++i)
        add(artisticFilterRegistry[i], filterActions[i]);

    static const char* const modifyRegistry[] = {
        "select.modify.expand", "select.modify.contract",
        "select.modify.border", "select.modify.smooth", "select.modify.feather",
    };
    for (int i = 0; i < modifyActions.size() && i < 5; ++i)
        add(modifyRegistry[i], modifyActions[i]);

    add("view.safeNone", safeNoneAction); add("view.safeYouTube", safeYouTubeAction);
    add("view.safeInstagram", safeInstagramAction); add("view.safeTikTok", safeTikTokAction);
    add("window.workspaceLayout", workspaceLayoutAction);
    add("layer.layerComps", layerCompsAction);
    add("layer.webSlices", webSlicesAction);
    add("window.advancedMode", toggleAdvancedModeAction);
    add("select.refineEdge", refineEdgeAction);
    add("help.sampleProjects", sampleProjectsAction);
}

QVector<QAction*> ActionManager::allActions() const
{
    return findChildren<QAction*>().toVector();
}

QString ActionManager::actionId(const QAction* action) const
{
    if (!action)
        return QString();
    for (auto it = m_shortcutActions.cbegin(); it != m_shortcutActions.cend(); ++it) {
        if (it.value() == action)
            return it.key();
    }
    return QString();
}

void ActionManager::loadCustomShortcuts()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("shortcuts"));
    for (auto it = m_shortcutActions.cbegin(); it != m_shortcutActions.cend(); ++it) {
        if (settings.contains(it.key()) && it.value()) {
            const QString portable = settings.value(it.key()).toString();
            it.value()->setShortcut(QKeySequence::fromString(portable, QKeySequence::PortableText));
        }
    }
    settings.endGroup();
}

void ActionManager::persistShortcut(const QString& id, const QKeySequence& key) const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("shortcuts"));
    settings.setValue(id, key.toString(QKeySequence::PortableText));
    settings.endGroup();
    settings.sync();
}

} // namespace cc
