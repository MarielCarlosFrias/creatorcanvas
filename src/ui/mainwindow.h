#pragma once

#include <QMainWindow>
#include <QPointF>

#include <memory>

#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "services/presetstore.h"
#include "services/autosaveservice.h"

class QDockWidget;
class QLabel;
class QMenu;
class QAction;

namespace cc {

class I18nService;
class SettingsService;
class CanvasView;
class LayersPanel;
class TextInspector;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(SettingsService* settings,
                        I18nService* i18n,
                        QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createDefaultDocument();
    void connectDocumentSignals();
    void buildCentralWidget();
    void buildLayersDock();
    void buildActions();
    void buildMenus();
    void buildStatusBar();
    void newDocument();
    void importImageViaDialog();
    void exportImage();
    void importImage(const QString& filePath);
    void flipLayer(bool horizontal);
    void addText();
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
    std::unique_ptr<CommandStack> m_history;
    std::unique_ptr<AutosaveService> m_autosave;

    std::unique_ptr<Document> m_document;
    CanvasView* m_canvas = nullptr;
    LayersPanel* m_layersPanel = nullptr;
    QDockWidget* m_layersDock = nullptr;
    TextInspector* m_textInspector = nullptr;
    QDockWidget* m_textDock = nullptr;
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
    QAction* m_flipHAction = nullptr;
    QAction* m_flipVAction = nullptr;
    QAction* m_deleteAction = nullptr;
    QAction* m_settingsAction = nullptr;
    QAction* m_aboutAction = nullptr;
    QAction* m_aboutQtAction = nullptr;

    QMenu* m_fileMenu = nullptr;
    QMenu* m_editMenu = nullptr;
    QMenu* m_layerMenu = nullptr;
    QMenu* m_settingsMenu = nullptr;
    QMenu* m_helpMenu = nullptr;

    QLabel* m_zoomLabel = nullptr;
    QLabel* m_positionLabel = nullptr;
    QLabel* m_versionLabel = nullptr;
    QLabel* m_languageLabel = nullptr;

    double m_currentZoom = 1.0;
    QPointF m_lastCursorPos;
};

} // namespace cc
