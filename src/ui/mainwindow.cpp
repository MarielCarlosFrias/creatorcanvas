#include "mainwindow.h"

#include "core/Document.h"
#include "core/layers/Layer.h"
#include "localization/i18nservice.h"
#include "services/settingsservice.h"
#include "ui/canvasview.h"

#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

namespace cc {

MainWindow::~MainWindow() = default;

MainWindow::MainWindow(SettingsService* settings, I18nService* i18n,
                       QWidget* parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_i18n(i18n)
{
    setMinimumSize(1000, 640);
    resize(1280, 800);

    createDefaultDocument();
    buildCentralWidget();
    buildActions();
    buildMenus();
    buildStatusBar();

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &MainWindow::retranslateUi);
    retranslateUi();
}

void MainWindow::createDefaultDocument()
{
    m_document = std::make_unique<Document>(1280, 720, 96);
    auto background = std::make_unique<BackgroundLayer>();
    background->name = QStringLiteral("Background");
    background->fill = QColor(Qt::white);
    m_document->addLayer(std::move(background));
}

void MainWindow::buildCentralWidget()
{
    m_canvas = new CanvasView(this);
    m_canvas->setDocument(m_document.get());

    if (m_document) {
        connect(m_document.get(), &Document::structureChanged,
                m_canvas, qOverload<>(&QWidget::update));
        connect(m_document.get(), &Document::layerPropertyChanged,
                m_canvas, qOverload<>(&QWidget::update));
    }

    connect(m_canvas, &CanvasView::zoomChanged,
            this, &MainWindow::updateZoomLabel);
    connect(m_canvas, &CanvasView::cursorMoved,
            this, &MainWindow::updatePositionLabel);

    setCentralWidget(m_canvas);
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
    m_zoomLabel = new QLabel(this);
    m_positionLabel = new QLabel(this);
    m_versionLabel = new QLabel(this);
    m_languageLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_zoomLabel);
    statusBar()->addPermanentWidget(m_positionLabel);
    statusBar()->addPermanentWidget(m_languageLabel);
    statusBar()->addPermanentWidget(m_versionLabel);
}

void MainWindow::updateZoomLabel()
{
    if (!m_i18n || !m_zoomLabel)
        return;
    m_zoomLabel->setText(m_i18n->t("common", "statusbar.zoom")
                             .arg(QString::number(m_currentZoom * 100.0,
                                                  'f', 0)));
}

void MainWindow::updatePositionLabel(const QPointF& documentPos)
{
    m_lastCursorPos = documentPos;
    if (!m_i18n || !m_positionLabel)
        return;
    m_positionLabel->setText(m_i18n->t("common", "statusbar.position")
                                 .arg(QString::number(documentPos.x(), 'f', 0),
                                      QString::number(documentPos.y(), 'f', 0)));
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

    updateZoomLabel();
    updatePositionLabel(m_lastCursorPos);

    m_versionLabel->setText(
        m_i18n->t("common", "statusbar.version")
            .arg(QApplication::applicationVersion()));
    m_languageLabel->setText(
        m_i18n->t("common", "statusbar.language")
            .arg(m_i18n->displayName(m_i18n->currentLanguage())));
}

} // namespace cc
