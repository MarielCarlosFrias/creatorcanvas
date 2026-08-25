#pragma once

#include <QMainWindow>

class QLabel;
class QMenu;
class QAction;

namespace cc {

class I18nService;
class SettingsService;

/// Top-level window. Milestone M0 scope: shell, menus (File/Help),
/// status bar, live language switching. Editor surfaces arrive in later
/// milestones; nothing decorative is placed here.
class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(SettingsService *settings,
                        I18nService *i18n,
                        QWidget *parent = nullptr);

private:
    void buildCentralWidget();
    void buildActions();
    void buildMenus();
    void buildStatusBar();
    void retranslateUi();

    SettingsService *m_settings = nullptr;
    I18nService *m_i18n = nullptr;

    QAction *m_quitAction = nullptr;
    QAction *m_aboutAction = nullptr;
    QAction *m_aboutQtAction = nullptr;

    QMenu *m_fileMenu = nullptr;
    QMenu *m_helpMenu = nullptr;

    QLabel *m_placeholderLabel = nullptr;
    QLabel *m_versionLabel = nullptr;
    QLabel *m_languageLabel = nullptr;
};

} // namespace cc
