#pragma once

#include <QMainWindow>
#include <QList>
#include <QPointF>
#include <QString>

#include <memory>
#include <functional>

#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/document/NewDocumentSpec.h"
#include "core/geometry/LayerAlignment.h"
#include "services/presetstore.h"
#include "services/textstylestore.h"
#include "services/LayerClipboard.h"
#include "services/minipaintbridge.h"
#include "services/autosaveservice.h"
#include "services/recentfiles.h"
#include "tools/CanvasTool.h"

class QCloseEvent;
class QResizeEvent;
class QShowEvent;
class QDockWidget;
class QLabel;
class QMenu;
class QAction;
class QActionGroup;
class QStackedWidget;
class QToolBar;
class QScrollArea;
class QComboBox;
class QCheckBox;
class QPushButton;
class QSlider;
class QLabel;
class QFrame;
class QToolButton;

namespace cc {

class TransformBar;

class I18nService;
class SettingsService;
class DocumentController;
class CanvasView;
class LayersPanel;
class TextInspector;
class ShapeInspector;
class ImageInspector;
class HistoryPanel;
class NavigatorPanel;
class SwatchPanel;
class StartScreen;
class ToolOptionsBar;
class ActionManager;
class CommandPalette;
class MenuBuilder;
class OnboardingPanel;
class OnboardingTour;
class PackBrowser;
class DockVisibilityPolicy;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    /// suppressStartupInteractions=true é usado por `--smoke-test` e
    /// `--ui-snapshot`: sem isso o diálogo de recuperação (ou "alterações
    /// não salvas") abre modal e trava qualquer execução headless, e o
    /// autosave deixa um recovery falso que quebra a execução seguinte.
    explicit MainWindow(SettingsService* settings,
                        I18nService* i18n,
                        QWidget* parent = nullptr,
                        bool suppressStartupInteractions = false);
    ~MainWindow() override;

    /// Ferramenta de desenvolvimento (--ui-snapshot): seleciona a
    /// ferramenta pelo nome usando a mesma action da UI.
    void selectToolByName(const QString& name);

    /// Documento actualmente aberto (nullptr antes de abrir/criar um).
    Document* document() const { return m_document; }

    /// Selecciona uma camada (como se o utilizador clicasse na lista).
    void selectLayer(const cc::LayerId& id);

    /// Ferramenta de desenvolvimento (--ui-snapshot): cria um documento demo
    /// para o canvas não ficar vazio nas capturas.
    void createDemoDocumentForSnapshot();

    /// Em janelas estreitas, esconde rótulos longos e encolhe sliders para
    /// a barra de opções não ficar cortada (o conteúdo é rolável, mas
    /// esconder os rótulos evita rolagem na maioria dos casos).
    void applyToolOptionsCompact(bool compact);

    void openFromPath(const QString& path);

protected:
    void closeEvent(QCloseEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void showEvent(QShowEvent*) override;

private:
    void createDefaultDocument();
    void connectDocumentSignals();
    void buildCentralWidget();
    void buildLayersDock();
    void buildSidePanels();
    void buildActions();
    void connectActionSignals();
    void buildToolBars();
    void buildMenus();
    void buildStatusBar();
    void showStartScreen();
    void enterEditor();
    void attachDocument(std::unique_ptr<Document> doc, const QString& filePath = QString());
    void startFromPreset(const NewDocumentSpec& spec);
    void startFromTemplate(int templateKind);
    void newDocument();
    void importImageViaDialog();
    void importImage(const QString& filePath);
    void exportImage();
    void openBatchDialog();
    void openWorkspaceLayoutDialog();
    void openCommandPalette();
    void quickExportSelectedLayerPng();
    void updateCanvasEmptyHint();
    void positionCanvasEmptyHint();
    void showBeginnerTipsBar();
    void showToolHint(CanvasTool tool);
    void buildOnboardingDock();
    void updateOnboarding();
    void startOnboardingTour();
    void markExported();
    void openPackBrowser();
    void applyPackItem(const QString& resource, const QSize& naturalSize);
    /// Pasta inicial dos diálogos de exportação: a última usada, senão a do
    /// projecto, senão a home. Evita caçar a pasta em cada exportação.
    QString initialExportDir(const QString& suggestedDir = QString()) const;
    bool processBatchFile(const QString& inputPath, const QString& outputPath,
                          const QStringList& operations, const QSize& resizeSize,
                          const QByteArray& outputFormat, int outputQuality,
                          bool overwrite);
    void quickExportLayerPng(const LayerId& id);
    void quickExportDocumentPng();
    void flipLayer(bool horizontal);
    void rotateLayer(double degrees);
    void addText();
    void addShape(ShapeKind kind);
    using AlignTarget = cc::AlignTarget;
    void alignSelectedLayer(AlignTarget target);
    void alignMultipleLayers(AlignTarget target);
    void distributeHorizontally();
    void distributeVertically();
    void groupSelectedLayers();
    void ungroupSelectedLayer();
    void mergeSelectedLayers();
    void mergeVisibleLayers();
    void flattenDocument();
    void duplicateSelectedLayer();
    void mergeDownSelectedLayer();
    void rasterizeSelectedLayer();
    void addAdjustmentLayer(cc::AdjustmentType type);
    void textBehindSubject();
    void deleteSelectedLayer();

    /// Cria (true) ou libera (false) a máscara de recorte da camada
    /// selecionada — equivale ao botão de recorte do painel de camadas.
    void setSelectedLayerClipped(bool clipped);
    bool saveDocument();
    bool saveDocumentAs();
    void openDocument();
    void openSampleProjects();
    void refreshRecentMenu();
    bool confirmDiscardUnsavedChanges();
    void openSettings();
    /// Aplica uma edição da barra de transformação numérica (um passo de
    /// histórico, fundível durante o arrasto do spin).
    void onTransformEdited(const cc::LayerId& id, cc::AffineTransform oldTransform,
                           cc::AffineTransform newTransform);
    /// Abre o painel de composições de camadas (guardar/aplicar estado).
    /// Abre o painel Refine Edge sobre a selecção activa (modo avançado).
    /// Abre a camada selecionada no editor externo MiniPaint.
    void editSelectedLayerInMiniPaint();

    void openRefineEdge();
    void openLayerComps();
    /// Abre o painel de fatias de exportação web.
    void openWebSlices();
    /// Abre o diálogo de ajustes para a camada de imagem selecionada.
    /// `type` escolhe a página inicial (brilho/contraste, curvas, …).
    void openAdjustments(cc::AdjustmentType type);
    void checkForRecoveryFile();
    void updateWindowTitle();
    void retranslateUi();
    void restoreWindowLayout();
    void updateSelectionMenuEnabled();
    void reselectSelection();
    void openTransformSelectionDialog();
    void saveCurrentSelection();
    void rebuildLoadSelectionMenu();
    void fillSelectionWithColor(const QColor& color);
    void strokeSelectionWithColor(const QColor& color, double widthPx);
    void clearSelectionPixels();
    void copySelectedLayers();
    void cutSelectedLayer();
    bool pasteFromClipboard();
    void pasteIntoSelection();
    void openImageSizeDialog();
    void openCanvasSizeDialog();
    void openSmartFilterDialog(cc::AdjustmentType type);
    void rotateCanvas(double degrees);
    void newGuide();
    /// Aplica um filtro de píxeis à camada de imagem ativa (com undo e
    /// respeita a seleção ativa e os locks). |fn| recebe o bitmap original.
    void applyImageFilterToActiveLayer(const std::function<QImage(const QImage&)>& fn,
                                       const QString& commandName);
    /// Substitui o bitmap da camada de imagem ativa com undo (um passo).
    bool commitActiveLayerBitmap(const QImage& newImage, const QString& commandName,
                                 const LayerId& layerId = {});
    quint64 m_filterGeneration = 0;
    bool m_filterInFlight = false;
    bool windowFitsOnScreen() const;
    void updateZoomLabel();
    void updatePositionLabel(const QPointF& documentPos);
    void updateResponsiveLayout();
    bool isEditorVisible() const;

    SettingsService* m_settings = nullptr;
    I18nService* m_i18n = nullptr;
    std::unique_ptr<PresetStore> m_presetStore;
    std::unique_ptr<TextStyleStore> m_textStyleStore;
    std::unique_ptr<RecentFiles> m_recentFiles;
    std::unique_ptr<CommandStack> m_history;
    std::unique_ptr<DocumentController> m_docController;
    std::unique_ptr<AutosaveService> m_autosave;
    QString m_configDir;

    Document* m_document = nullptr;
    QStackedWidget* m_centralStack = nullptr;
    StartScreen* m_startScreen = nullptr;
    CanvasView* m_canvas = nullptr;
    LayersPanel* m_layersPanel = nullptr;
    TextInspector* m_textInspector = nullptr;
    ShapeInspector* m_shapeInspector = nullptr;
    ImageInspector* m_imageInspector = nullptr;
    HistoryPanel* m_historyPanel = nullptr;
    QDockWidget* m_layersDock = nullptr;
    QDockWidget* m_textDock = nullptr;
    QDockWidget* m_shapeDock = nullptr;
    QDockWidget* m_imageDock = nullptr;
    QDockWidget* m_historyDock = nullptr;
    QDockWidget* m_navigatorDock = nullptr;
    QDockWidget* m_swatchesDock = nullptr;
    NavigatorPanel* m_navigatorPanel = nullptr;
    SwatchPanel* m_swatchesPanel = nullptr;
    LayerId m_selectedId;
    QString m_currentFilePath;
    bool m_overwriteExisting = false;
    /// Sugestão de caminho para o próximo "Salvar Como" (ex.: origem de uma
    /// recuperação aberta como cópia) — pré-preenche o diálogo.
    QString m_suggestedSavePath;
    bool m_modified = false;

    ActionManager* m_actions = nullptr;
    // Paleta de comandos: instância única reaberta (destroyed limpa o ponteiro).
    cc::CommandPalette* m_commandPalette = nullptr;

    // Ajuda a iniciantes: dica centrada quando o documento não tem camadas
    // e barra de dicas dispensável no topo do editor.
    QLabel* m_canvasEmptyHint = nullptr;
    QFrame* m_tipsBar = nullptr;

    // Primeiros passos: painel com quatro objectivos que se marcam sozinhos.
    QDockWidget* m_onboardingDock = nullptr;
    cc::OnboardingPanel* m_onboardingPanel = nullptr;
    cc::PackBrowser* m_packBrowser = nullptr;
    std::unique_ptr<cc::OnboardingTour> m_tour;
    bool m_hasExported = false;
    bool m_onboardingDismissed = false;
    bool m_tourScheduled = false;
    QLabel* m_tipsBarLabel = nullptr;
    QToolButton* m_tipsBarClose = nullptr;
    MenuBuilder* m_menuBuilder = nullptr;

    std::unique_ptr<cc::MiniPaintBridge> m_miniPaintBridge;

    QAction* m_newAction = nullptr;
    QAction* m_openAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_saveAsAction = nullptr;
    QAction* m_importAction = nullptr;
    QAction* m_exportAction = nullptr;
    QAction* m_quitAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_addTextAction = nullptr;
    QAction* m_addFillLayerAction = nullptr;
    QAction* m_addAdjustmentLayerAction = nullptr;
    QMenu* m_addShapeMenu = nullptr;
    QMenu* m_imageMenu = nullptr;
    QMenu* m_selectMenu = nullptr;
    QMenu* m_filterMenu = nullptr;
    QMenu* m_windowMenu = nullptr;
    QAction* m_addRectAction = nullptr;
    QAction* m_addRoundedRectAction = nullptr;
    QAction* m_addEllipseAction = nullptr;
    QAction* m_addLineAction = nullptr;
    QAction* m_addArrowRightAction = nullptr;
    QAction* m_addArrowCurvedAction = nullptr;
    QAction* m_addStarAction = nullptr;
    QAction* m_addBadgeAction = nullptr;
    QAction* m_addSpeechBubbleAction = nullptr;
    QAction* m_addHeartAction = nullptr;    // Shape Power Pack: ações data-driven do submenu de formas
    QList<QAction*> m_addNewShapeActions;
    QMenu* m_alignMenu = nullptr;
    QAction* m_alignLeftAction = nullptr;
    QAction* m_alignCenterXAction = nullptr;
    QAction* m_alignRightAction = nullptr;
    QAction* m_alignTopAction = nullptr;
    QAction* m_alignCenterYAction = nullptr;
    QAction* m_alignBottomAction = nullptr;
    QAction* m_alignCenterBothAction = nullptr;
    QAction* m_distributeHAction = nullptr;
    QAction* m_distributeVAction = nullptr;
    QAction* m_groupAction = nullptr;
    QAction* m_ungroupAction = nullptr;
    QAction* m_mergeSelectedAction = nullptr;
    QAction* m_mergeDownAction = nullptr;
    QAction* m_mergeVisibleAction = nullptr;
    QAction* m_flattenAction = nullptr;
    QAction* m_duplicateLayerAction = nullptr;
    QAction* m_rasterizeLayerAction = nullptr;
    QAction* m_textBehindSubjectAction = nullptr;
    QAction* m_toggleGridAction = nullptr;
    QAction* m_toggleSnapAction = nullptr;
    QAction* m_toggleHistoryAction = nullptr;
    QAction* m_toggleRulersAction = nullptr;
    QAction* m_toggleGuidesAction = nullptr;
    QAction* m_newGuideAction = nullptr;
    QAction* m_clearGuidesAction = nullptr;
    QAction* m_toggleNavigatorAction = nullptr;
    QAction* m_toggleSwatchesAction = nullptr;
    QMenu* m_guidesMenu = nullptr;
    QAction* m_toggleToolsBarAction = nullptr;
    QAction* m_toggleRightPanelAction = nullptr;
    QAction* m_toggleLayersDockAction = nullptr;
    QAction* m_toggleTextDockAction = nullptr;
    QAction* m_toggleShapeDockAction = nullptr;
    QAction* m_toggleImageDockAction = nullptr;
    QAction* m_flipHAction = nullptr;
    QAction* m_flipVAction = nullptr;
    QAction* m_rotateLeftAction = nullptr;
    QAction* m_rotateRightAction = nullptr;
    QAction* m_deleteAction = nullptr;
    QAction* m_lockLayerAction = nullptr;
    QAction* m_createClippingMaskAction = nullptr;
    QAction* m_releaseClippingMaskAction = nullptr;
    QAction* m_selectAllAction = nullptr;
    QAction* m_settingsAction = nullptr;
    QAction* m_startScreenAction = nullptr;
    QAction* m_aboutAction = nullptr;
    QAction* m_aboutQtAction = nullptr;
    QAction* m_removeBgAiAction = nullptr;
    QAction* m_removeBgAiQuickAction = nullptr;
    QAction* m_mockupAction = nullptr;
    QAction* m_editInMiniPaintAction = nullptr;

    QMenu* m_fileMenu = nullptr;
    QMenu* m_openRecentMenu = nullptr;
    QMenu* m_editMenu = nullptr;
    QMenu* m_layerMenu = nullptr;
    QMenu* m_viewMenu = nullptr;
    QMenu* m_panelsMenu = nullptr;
    QMenu* m_safeZoneMenu = nullptr;
    QAction* m_safeNoneAction = nullptr;
    QAction* m_safeYouTubeAction = nullptr;
    QAction* m_safeInstagramAction = nullptr;
    QAction* m_safeTikTokAction = nullptr;
    QMenu* m_settingsMenu = nullptr;
    QMenu* m_helpMenu = nullptr;

    QAction* m_zoomInAction = nullptr;
    QAction* m_zoomOutAction = nullptr;
    QAction* m_zoomFitAction = nullptr;

    // Ferramentas da barra de ferramentas e opções de ferramentas
    QToolBar* m_quickToolBar = nullptr;
    QToolBar* m_toolsBar = nullptr;
    /// Barra horizontal compacta (mesmas actions) usada em janelas estreitas.
    QToolBar* m_toolsBarCompact = nullptr;
    QToolBar* m_toolOptionsBar = nullptr;
    QActionGroup* m_toolGroup = nullptr;
    QAction* m_toolSelectAction = nullptr;
    QAction* m_toolCropAction = nullptr;
    QAction* m_toolScissorsAction = nullptr;
    QAction* m_toolWandAction = nullptr;
    QAction* m_toolCloneAction = nullptr;
    QAction* m_toolPaintAction = nullptr;
    QAction* m_toolFloodAction = nullptr;
    QAction* m_toolEyedropperAction = nullptr;
    QAction* m_toolPenAction = nullptr;
    QAction* m_toolVectorNodesAction = nullptr;
    QAction* m_toolMarqueeAction = nullptr;
    QAction* m_toolGradientAction = nullptr;
    QAction* m_toolRetouchAction = nullptr;
    QAction* m_toolHealingAction = nullptr;
    QMenu* m_toolsMenu = nullptr;
    // Menus de filtro (títulos e ações re-traduzidos em retranslateUi)
    QMenu* m_artisticMenu = nullptr;
    QMenu* m_toneMenu = nullptr;
    QList<QAction*> m_filterActions;
    // Ações do menu Seleção (re-traduzidas em retranslateUi)
    QAction* m_deselectAction = nullptr;
    QAction* m_invertSelectionAction = nullptr;
    QAction* m_reselectAction = nullptr;
    QAction* m_saveSelectionAction = nullptr;
    QMenu* m_loadSelectionMenu = nullptr;
    // Ações do menu Editar sobre a seleção ativa
    QAction* m_fillSelectionAction = nullptr;
    QAction* m_strokeSelectionAction = nullptr;
    QAction* m_clearSelectionAction = nullptr;
    QAction* m_copyAction = nullptr;
    QAction* m_cutAction = nullptr;
    QAction* m_pasteAction = nullptr;
    QAction* m_pasteIntoAction = nullptr;
    QAction* m_canvasSizeAction = nullptr;
    QAction* m_rotateCanvasCwAction = nullptr;
    QAction* m_rotateCanvasCcwAction = nullptr;
    QAction* m_rotateCanvas180Action = nullptr;
    QMenu* m_rotateCanvasMenu = nullptr;
    QAction* m_blurGaussianAction = nullptr;
    QAction* m_blurMotionAction = nullptr;
    QAction* m_noiseAddAction = nullptr;
    QAction* m_distortTwirlAction = nullptr;
    QAction* m_distortPinchAction = nullptr;
    QAction* m_distortRippleAction = nullptr;
    /// Clipboard interno de camadas e respetivos assets.
    cc::LayerClipboard m_layerClipboard;
    /// Identifica a imagem que NÓS colocámos no clipboard do sistema; se
    /// outra aplicação a substituir, o snapshot interno deixa de ter prioridade.
    qint64 m_systemClipboardImageKey = 0;
    QMenu* m_modifyMenu = nullptr;
    QList<QAction*> m_modifyActions;
    QAction* m_colorRangeAction = nullptr;
    QAction* m_selectSubjectAction = nullptr;
    QList<QAction*> m_adjustmentActions;
    QAction* m_layerCompsAction = nullptr;
    QAction* m_webSlicesAction = nullptr;
    QAction* m_toggleAdvancedModeAction = nullptr;
    QAction* m_refineEdgeAction = nullptr;
    QAction* m_selectionOpsAction = nullptr;
    QMenu* m_selectionToolsMenu = nullptr;
    QMenu* m_retouchToolsMenu = nullptr;
    QMenu* m_createToolsMenu = nullptr;

    ToolOptionsBar* m_toolOptionsBarWidget = nullptr;
    TransformBar* m_transformBar = nullptr;

    QLabel* m_zoomLabel = nullptr;
    QLabel* m_positionLabel = nullptr;
    QLabel* m_versionLabel = nullptr;
    QLabel* m_languageLabel = nullptr;

    double m_currentZoom = 1.0;
    QPointF m_lastCursorPos;

    // Responsividade: docks auto-ocultados em janelas estreitas, com o
    // estado escolhido pelo usuário guardado para ser restaurado.
    bool m_suppressStartupInteractions = false;
    bool m_updatingResponsiveLayout = false;

    /// Modo avançado (View ▸ Modo Avançado, persistido). Esconde/exibe as
    /// ferramentas profissionais em toda a UI. Os diálogos leem-no ao serem
    /// construídos — são modais, por isso não precisam de sinal.
    bool advancedMode() const { return m_advancedMode; }

    /// Aplica o modo avançado actual à UI (chamado ao alternar e no arranque).
    void applyAdvancedMode();

private:
    bool m_advancedMode = false;
    std::unique_ptr<DockVisibilityPolicy> m_dockVisibility;
};

} // namespace cc
