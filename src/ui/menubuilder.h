#pragma once

#include <QMenu>
#include <QMenuBar>
#include <QObject>

namespace cc {

class ActionManager;
class I18nService;

/// Constrói e gerencia a barra de menus principal do CreatorCanvas.
/// Desacopla a hierarquia e re-tradução dos títulos de menus da MainWindow.
class MenuBuilder final : public QObject {
    Q_OBJECT
public:
    MenuBuilder(QMenuBar* menuBar, ActionManager* actions, QObject* parent = nullptr);
    ~MenuBuilder() override = default;

    void buildMenus(I18nService* i18n);
    void retranslate(I18nService* i18n);

    // Menus principais e submenus
    QMenu* fileMenu = nullptr;
    QMenu* openRecentMenu = nullptr;
    QMenu* editMenu = nullptr;
    QMenu* imageMenu = nullptr;
    QMenu* filterMenu = nullptr;
    QMenu* artisticMenu = nullptr;
    QMenu* toneMenu = nullptr;
    QMenu* selectMenu = nullptr;
    QMenu* modifyMenu = nullptr;
    QMenu* layerMenu = nullptr;
    QMenu* addShapeMenu = nullptr;
    QMenu* alignMenu = nullptr;
    QMenu* viewMenu = nullptr;
    QMenu* safeZoneMenu = nullptr;
    QMenu* panelsMenu = nullptr;
    QMenu* toolsMenu = nullptr;
    QMenu* selectionToolsMenu = nullptr;
    QMenu* retouchToolsMenu = nullptr;
    QMenu* createToolsMenu = nullptr;
    QMenu* windowMenu = nullptr;
    QMenu* settingsMenu = nullptr;
    QMenu* helpMenu = nullptr;

private:
    QMenuBar* m_menuBar = nullptr;
    ActionManager* m_actions = nullptr;
};

} // namespace cc
