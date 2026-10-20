#pragma once

#include <QMainWindow>
#include <QList>
#include <QPointF>
#include <QString>

#include <memory>

#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/document/NewDocumentSpec.h"
#include "services/presetstore.h"
#include "services/autosaveservice.h"
#include "services/recentfiles.h"

class QCloseEvent;
class QResizeEvent;
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

namespace cc {

class I18nService;
class SettingsService;
class DocumentController;
class CanvasView;
class LayersPanel;
class TextInspector;
class ShapeInspector;
class ImageInspector;
class HistoryPanel;
class StartScreen;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(SettingsService* settings,
                        I18nService* i18n,
                        QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Ferramenta de desenvolvimento (--ui-snapshot): seleciona a
    /// ferramenta pelo nome usando a mesma action da UI.
    void selectToolByName(const QString& name);

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

private:
    void createDefaultDocument();
    void connectDocumentSignals();
    void buildCentralWidget();
    void buildLayersDock();
    void buildActions();
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
    void quickExportLayerPng(const LayerId& id);
    void flipLayer(bool horizontal);
    void rotateLayer(double degrees);
    void addText();
    void addShape(ShapeKind kind);
    enum class AlignTarget {
        Left, CenterX, Right,
        Top, CenterY, Bottom,
        CenterBoth
    };
    void alignSelectedLayer(AlignTarget target);
    void alignMultipleLayers(AlignTarget target);
    void distributeHorizontally();
    void distributeVertically();
    void groupSelectedLayers();
    void ungroupSelectedLayer();
    void mergeSelectedLayers();
    void mergeVisibleLayers();
    void flattenDocument();
    void textBehindSubject();
    void deleteSelectedLayer();
    bool saveDocument();
    bool saveDocumentAs();
    void openDocument();
    void refreshRecentMenu();
    bool confirmDiscardUnsavedChanges();
    void openSettings();
    void checkForRecoveryFile();
    void updateWindowTitle();
    void retranslateUi();
    void updateZoomLabel();
    void updatePositionLabel(const QPointF& documentPos);

    SettingsService* m_settings = nullptr;
    I18nService* m_i18n = nullptr;
    std::unique_ptr<PresetStore> m_presetStore;
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
    LayerId m_selectedId;
    QString m_currentFilePath;
    /// Sugestão de caminho para o próximo "Salvar Como" (ex.: origem de uma
    /// recuperação aberta como cópia) — pré-preenche o diálogo.
    QString m_suggestedSavePath;
    bool m_modified = false;

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
    QAction* m_mergeVisibleAction = nullptr;
    QAction* m_flattenAction = nullptr;
    QAction* m_textBehindSubjectAction = nullptr;
    QAction* m_toggleGridAction = nullptr;
    QAction* m_toggleSnapAction = nullptr;
    QAction* m_toggleHistoryAction = nullptr;
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
    QAction* m_settingsAction = nullptr;
    QAction* m_startScreenAction = nullptr;
    QAction* m_aboutAction = nullptr;
    QAction* m_aboutQtAction = nullptr;
    QAction* m_removeBgAiAction = nullptr;
    QAction* m_removeBgAiQuickAction = nullptr;
    QAction* m_mockupAction = nullptr;

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
    QAction* m_toolMarqueeAction = nullptr;
    QAction* m_toolGradientAction = nullptr;
    QAction* m_toolRetouchAction = nullptr;
    QAction* m_toolHealingAction = nullptr;
    QMenu* m_toolsMenu = nullptr;
    QMenu* m_selectionToolsMenu = nullptr;
    QMenu* m_retouchToolsMenu = nullptr;
    QMenu* m_createToolsMenu = nullptr;

    QStackedWidget* m_toolOptionsStack = nullptr;
    QScrollArea* m_toolOptionsScroll = nullptr;
    QLabel* m_selectHintLabel = nullptr;

    QLabel* m_cropAspectLabel = nullptr;
    QComboBox* m_cropAspectCombo = nullptr;
    QPushButton* m_cropApplyBtn = nullptr;
    QPushButton* m_cropCancelBtn = nullptr;

    QLabel* m_scissorsModeLabel = nullptr;
    QComboBox* m_scissorsModeCombo = nullptr;
    QCheckBox* m_scissorsAutoCropCheck = nullptr;
    QPushButton* m_scissorsApplyBtn = nullptr;
    QPushButton* m_scissorsCancelBtn = nullptr;
    QLabel* m_scissorsHintLabel = nullptr;

    QLabel* m_wandTolLabel = nullptr;
    QSlider* m_wandTolSlider = nullptr;
    QLabel* m_wandTolValueLabel = nullptr;
    QCheckBox* m_wandContiguousCheck = nullptr;
    QLabel* m_wandHintLabel = nullptr;

    QLabel* m_cloneRadiusLabel = nullptr;
    QSlider* m_cloneRadiusSlider = nullptr;
    QLabel* m_cloneRadiusValueLabel = nullptr;
    QLabel* m_cloneHardnessLabel = nullptr;
    QSlider* m_cloneHardnessSlider = nullptr;
    QLabel* m_cloneHardnessValueLabel = nullptr;
    QLabel* m_cloneOpacityLabel = nullptr;
    QSlider* m_cloneOpacitySlider = nullptr;
    QLabel* m_cloneOpacityValueLabel = nullptr;
    QLabel* m_cloneHintLabel = nullptr;

    QLabel* m_paintBrushLabel = nullptr;
    QComboBox* m_paintBrushCombo = nullptr;
    QPushButton* m_paintColorBtn = nullptr;
    QLabel* m_paintSizeLabel = nullptr;
    QSlider* m_paintSizeSlider = nullptr;
    QLabel* m_paintSizeValueLabel = nullptr;
    QLabel* m_paintOpacityLabel = nullptr;
    QSlider* m_paintOpacitySlider = nullptr;
    QLabel* m_paintOpacityValueLabel = nullptr;
    QColor m_currentPaintColor{230, 50, 50};

    QLabel* m_floodTolLabel = nullptr;
    QSlider* m_floodTolSlider = nullptr;
    QLabel* m_floodTolValueLabel = nullptr;
    QPushButton* m_floodColorBtn = nullptr;

    // --- Seleção retangular/elíptica (Marquee) ---
    QLabel* m_marqueeShapeLabel = nullptr;
    QComboBox* m_marqueeShapeCombo = nullptr;
    QLabel* m_marqueeFeatherLabel = nullptr;
    QSlider* m_marqueeFeatherSlider = nullptr;
    QLabel* m_marqueeFeatherValueLabel = nullptr;

    // --- Gradiente (Gradient) ---
    QLabel* m_gradientTypeLabel = nullptr;
    QComboBox* m_gradientTypeCombo = nullptr;
    QPushButton* m_gradientStartColorBtn = nullptr;
    QPushButton* m_gradientEndColorBtn = nullptr;
    QLabel* m_gradientOpacityLabel = nullptr;
    QSlider* m_gradientOpacitySlider = nullptr;
    QLabel* m_gradientOpacityValueLabel = nullptr;
    QColor m_gradientStartColor{61, 126, 255};
    QColor m_gradientEndColor{0, 0, 0, 0};

    // --- Retoque (Retouch) ---
    QLabel* m_retouchModeLabel = nullptr;
    QComboBox* m_retouchModeCombo = nullptr;
    QLabel* m_retouchSizeLabel = nullptr;
    QSlider* m_retouchSizeSlider = nullptr;
    QLabel* m_retouchSizeValueLabel = nullptr;
    QLabel* m_retouchStrengthLabel = nullptr;
    QSlider* m_retouchStrengthSlider = nullptr;
    QLabel* m_retouchStrengthValueLabel = nullptr;

    // --- Reparo (Healing) ---
    QLabel* m_healingModeLabel = nullptr;
    QComboBox* m_healingModeCombo = nullptr;
    QLabel* m_healingRadiusLabel = nullptr;
    QSlider* m_healingRadiusSlider = nullptr;
    QLabel* m_healingRadiusValueLabel = nullptr;
    QLabel* m_healingStrengthLabel = nullptr;
    QSlider* m_healingStrengthSlider = nullptr;
    QLabel* m_healingStrengthValueLabel = nullptr;

    QLabel* m_zoomLabel = nullptr;
    QLabel* m_positionLabel = nullptr;
    QLabel* m_versionLabel = nullptr;
    QLabel* m_languageLabel = nullptr;

    double m_currentZoom = 1.0;
    QPointF m_lastCursorPos;

    // Responsividade: docks auto-ocultados em janelas estreitas, com o
    // estado escolhido pelo usuário guardado para ser restaurado.
    bool m_docksAutoHidden = false;
    QList<QPair<QDockWidget*, bool>> m_savedDockVisibility;
};

} // namespace cc
