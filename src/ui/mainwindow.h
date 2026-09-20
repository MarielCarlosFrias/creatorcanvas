#pragma once

#include <QMainWindow>
#include <QPointF>
#include <QString>

#include <memory>

#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/document/NewDocumentSpec.h"
#include "services/presetstore.h"
#include "services/autosaveservice.h"
#include "services/recentfiles.h"
#include "services/autosaveservice.h"
#include "services/recentfiles.h"

class QCloseEvent;
class QDockWidget;
class QLabel;
class QMenu;
class QAction;
class QActionGroup;
class QStackedWidget;
class QToolBar;
class QComboBox;
class QCheckBox;
class QPushButton;
class QSlider;

namespace cc {

class I18nService;
class SettingsService;
class CanvasView;
class LayersPanel;
class TextInspector;
class ShapeInspector;
class ImageInspector;
class StartScreen;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(SettingsService* settings,
                        I18nService* i18n,
                        QWidget* parent = nullptr);

    void openFromPath(const QString& path);

protected:
    void closeEvent(QCloseEvent*) override;

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
    void startFromPreset(const NewDocumentSpec& spec);
    void startFromTemplate(int templateKind);
    void newDocument();
    void importImageViaDialog();
    void importImage(const QString& filePath);
    void exportImage();
    void flipLayer(bool horizontal);
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
    void deleteSelectedLayer();
    bool saveDocument();
    bool saveDocumentAs();
    void openDocument();
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
    std::unique_ptr<AutosaveService> m_autosave;
    QString m_configDir;

    std::unique_ptr<Document> m_document;
    QStackedWidget* m_centralStack = nullptr;
    StartScreen* m_startScreen = nullptr;
    CanvasView* m_canvas = nullptr;
    LayersPanel* m_layersPanel = nullptr;
    TextInspector* m_textInspector = nullptr;
    ShapeInspector* m_shapeInspector = nullptr;
    ImageInspector* m_imageInspector = nullptr;
    QDockWidget* m_layersDock = nullptr;
    QDockWidget* m_textDock = nullptr;
    QDockWidget* m_shapeDock = nullptr;
    QDockWidget* m_imageDock = nullptr;
    LayerId m_selectedId;
    QString m_currentFilePath;
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
    QAction* m_addRectAction = nullptr;
    QAction* m_addRoundedRectAction = nullptr;
    QAction* m_addEllipseAction = nullptr;
    QAction* m_addLineAction = nullptr;
    QAction* m_addArrowRightAction = nullptr;
    QAction* m_addArrowCurvedAction = nullptr;
    QAction* m_addStarAction = nullptr;
    QAction* m_addBadgeAction = nullptr;
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
    QAction* m_toggleGridAction = nullptr;
    QAction* m_toggleSnapAction = nullptr;
    QAction* m_flipHAction = nullptr;
    QAction* m_flipVAction = nullptr;
    QAction* m_deleteAction = nullptr;
    QAction* m_settingsAction = nullptr;
    QAction* m_startScreenAction = nullptr;
    QAction* m_aboutAction = nullptr;
    QAction* m_aboutQtAction = nullptr;
    QAction* m_removeBgAiAction = nullptr;
    QAction* m_removeBgAiQuickAction = nullptr;

    QMenu* m_fileMenu = nullptr;
    QMenu* m_editMenu = nullptr;
    QMenu* m_layerMenu = nullptr;
    QMenu* m_viewMenu = nullptr;
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
    QToolBar* m_toolOptionsBar = nullptr;
    QActionGroup* m_toolGroup = nullptr;
    QAction* m_toolSelectAction = nullptr;
    QAction* m_toolCropAction = nullptr;
    QAction* m_toolScissorsAction = nullptr;
    QAction* m_toolWandAction = nullptr;
    QAction* m_toolCloneAction = nullptr;
    QAction* m_toolPaintAction = nullptr;
    QAction* m_toolFloodAction = nullptr;
    QMenu* m_toolsMenu = nullptr;

    QStackedWidget* m_toolOptionsStack = nullptr;
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

    QLabel* m_zoomLabel = nullptr;
    QLabel* m_positionLabel = nullptr;
    QLabel* m_versionLabel = nullptr;
    QLabel* m_languageLabel = nullptr;

    double m_currentZoom = 1.0;
    QPointF m_lastCursorPos;
};

} // namespace cc
