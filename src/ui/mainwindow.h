#pragma once

#include <QMainWindow>
#include <QPointF>

#include <memory>

#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "services/presetstore.h"

class QDockWidget;
class QLabel;
class QMenu;
class QAction;

namespace cc {

class I18nService;
class SettingsService;
class CanvasView;
class LayersPanel;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(SettingsService* settings,
                        I18nService* i18n,
                        QWidget* parent = nullptr);

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
    void importImage(const QString& filePath);
    void flipLayer(bool horizontal);
    void deleteSelectedLayer();
    void retranslateUi();
    void updateZoomLabel();
    void updatePositionLabel(const QPointF& documentPos);

    SettingsService* m_settings = nullptr;
    I18nService* m_i18n = nullptr;
    std::unique_ptr<PresetStore> m_presetStore;
    std::unique_ptr<CommandStack> m_history;

    std::unique_ptr<Document> m_document;
    CanvasView* m_canvas = nullptr;
    LayersPanel* m_layersPanel = nullptr;
    QDockWidget* m_layersDock = nullptr;
    LayerId m_selectedId;

    QAction* m_newAction = nullptr;
    QAction* m_importAction = nullptr;
    QAction* m_quitAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_flipHAction = nullptr;
    QAction* m_flipVAction = nullptr;
    QAction* m_deleteAction = nullptr;
    QAction* m_aboutAction = nullptr;
    QAction* m_aboutQtAction = nullptr;

    QMenu* m_fileMenu = nullptr;
    QMenu* m_editMenu = nullptr;
    QMenu* m_layerMenu = nullptr;
    QMenu* m_helpMenu = nullptr;

    QLabel* m_zoomLabel = nullptr;
    QLabel* m_positionLabel = nullptr;
    QLabel* m_versionLabel = nullptr;
    QLabel* m_languageLabel = nullptr;

    double m_currentZoom = 1.0;
    QPointF m_lastCursorPos;
};

} // namespace cc
