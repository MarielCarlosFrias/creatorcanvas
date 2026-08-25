#include "mainwindow.h"

#include "localization/i18nservice.h"
#include "services/settingsservice.h"

#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace cc {

MainWindow::MainWindow(SettingsService *settings, I18nService *i18n,
                       QWidget *parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_i18n(i18n)
{
    setMinimumSize(1000, 640);
    resize(1280, 800);

    buildCentralWidget();
    buildActions();
    buildMenus();
    buildStatusBar();

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &MainWindow::retranslateUi);
    retranslateUi();
}

void MainWindow::buildCentralWidget()
{
    auto *holder = new QWidget(this);
    auto *layout = new QVBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);

    m_placeholderLabel = new QLabel(holder);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setWordWrap(true);
    layout->addWidget(m_placeholderLabel);

    setCentralWidget(holder);
}

void MainWindow::buildActions()
{
    m_quitAction = new QAction(this);
    connect(m_quitAction, &QAction::triggered, this, &MainWindow::close);

    m_aboutAction = new QAction(this);
    connect(m_aboutAction, &QAction::triggered, this, [this] {
        if (!m_i18n)
            return;
        QMessageBox::about(
            this,
            m_i18n->t("common", "menu.help.about"),
            QStringLiteral("<h3>CreatorCanvas %1</h3><p>%2</p>")
                .arg(QApplication::applicationVersion(),
                     m_i18n->t("common", "app.aboutBody").toHtmlEscaped()));
    });

    m_aboutQtAction = new QAction(this);
    connect(m_aboutQtAction, &QAction::triggered, this, [this] {
        QMessageBox::aboutQt(this);
    });
}

void MainWindow::buildMenus()
{
    m_fileMenu = menuBar()->addMenu(QString());
    m_fileMenu->addAction(m_quitAction);

    m_helpMenu = menuBar()->addMenu(QString());
    m_helpMenu->addAction(m_aboutAction);
    m_helpMenu->addAction(m_aboutQtAction);
}

void MainWindow::buildStatusBar()
{
    m_versionLabel = new QLabel(this);
    m_languageLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_languageLabel);
    statusBar()->addPermanentWidget(m_versionLabel);
}

void MainWindow::retranslateUi()
{
    if (!m_i18n)
        return;

    setWindowTitle(m_i18n->t("common", "app.title"));

    m_quitAction->setText(m_i18n->t("common", "menu.file.quit"));
    m_aboutAction->setText(m_i18n->t("common", "menu.help.about"));
    m_aboutQtAction->setText(m_i18n->t("common", "menu.help.aboutQt"));

    m_fileMenu->setTitle(m_i18n->t("common", "menu.file"));
    m_helpMenu->setTitle(m_i18n->t("common", "menu.help"));

    m_placeholderLabel->setText(
        QStringLiteral("<div align='center'><h1>%1</h1><p>%2</p></div>")
            .arg(m_i18n->t("common", "app.title"),
                 m_i18n->t("common", "startup.placeholderBody")
                     .toHtmlEscaped()));

    m_versionLabel->setText(
        m_i18n->t("common", "statusbar.version")
            .arg(QApplication::applicationVersion()));
    m_languageLabel->setText(
        m_i18n->t("common", "statusbar.language")
            .arg(m_i18n->displayName(m_i18n->currentLanguage())));
}

} // namespace cc
