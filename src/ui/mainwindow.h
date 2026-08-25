#pragma once

#include <QMainWindow>
#include <QPointF>
#include <memory>

#include "core/Document.h"

class QLabel;
class QMenu;
class QAction;

namespace cc {

class I18nService;
class SettingsService;
class CanvasView;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(SettingsService* settings,
                        I18nService* i18n,
                        QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void createDefaultDocument();
    void buildCentralWidget();
    void buildActions();
    void buildMenus();
    void buildStatusBar();
    void retranslateUi();
    void updateZoomLabel();
    void updatePositionLabel(const QPointF& documentPos);

    SettingsService* m_settings = nullptr;
    I18nService* m_i18n = nullptr;

    std::unique_ptr<Document> m_document;
    CanvasView* m_canvas = nullptr;

    QAction* m_quitAction = nullptr;
    QAction* m_aboutAction = nullptr;
    QAction* m_aboutQtAction = nullptr;

    QMenu* m_fileMenu = nullptr;
    QMenu* m_helpMenu = nullptr;

    QLabel* m_zoomLabel = nullptr;
    QLabel* m_positionLabel = nullptr;
    QLabel* m_versionLabel = nullptr;
    QLabel* m_languageLabel = nullptr;

    double m_currentZoom = 1.0;
    QPointF m_lastCursorPos;
};

} // namespace cc
