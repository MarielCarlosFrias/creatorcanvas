#include "mainwindow.h"

#include "core/document/NewDocumentSpec.h"
#include "core/history/DocumentCommands.h"
#include "core/layers/Layer.h"
#include "imageio/ImageImporter.h"
#include "localization/i18nservice.h"
#include "ui/canvasview.h"
#include "ui/layerspanel.h"
#include "ui/textinspector.h"
#include "ui/newdocumentdialog.h"

#include <QApplication>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStatusBar>

namespace cc {

MainWindow::MainWindow(SettingsService* settings, I18nService* i18n,
                       QWidget* parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_i18n(i18n)
{
    setMinimumSize(1000, 640);
    resize(1280, 800);

    const QString configDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    m_presetStore =
        std::make_unique<PresetStore>(configDir + QStringLiteral("/presets.json"));
    m_history = std::make_unique<CommandStack>();

    createDefaultDocument();
    buildCentralWidget();
    buildLayersDock();
    connectDocumentSignals();
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
    NewDocumentSpec spec;
    spec.backgroundColor = QColor(Qt::white);
    m_document = createDocument(spec);
}

void MainWindow::connectDocumentSignals()
{
    if (!m_document || !m_canvas)
        return;
    connect(m_document.get(), &Document::structureChanged,
            m_canvas, qOverload<>(&QWidget::update));
    connect(m_document.get(), &Document::layerPropertyChanged,
            m_canvas, qOverload<>(&QWidget::update));
    if (m_layersPanel) {
        connect(m_document.get(), &Document::structureChanged,
                m_layersPanel, &LayersPanel::refresh);
        connect(m_document.get(), &Document::layerPropertyChanged,
                m_layersPanel, &LayersPanel::refresh);
    }
}

void MainWindow::buildCentralWidget()
{
    m_canvas = new CanvasView(this);
    m_canvas->setDocument(m_document.get());

    connect(m_canvas, &CanvasView::zoomChanged,
            this, &MainWindow::updateZoomLabel);
    connect(m_canvas, &CanvasView::cursorMoved,
            this, &MainWindow::updatePositionLabel);
    connect(m_canvas, &CanvasView::fileDropped,
            this, &MainWindow::importImage);
    connect(m_canvas, &CanvasView::selectionChanged, this,
            [this](const LayerId& id) {
                m_selectedId = id;
                if (m_layersPanel)
                    m_layersPanel->setSelectedLayer(id);
                if (m_textInspector)
                    m_textInspector->setSelectedLayer(id);
            });
    connect(m_canvas, &CanvasView::transformCommitted, this,
            [this](const LayerId& id, const AffineTransform& oldValue,
                   const AffineTransform& newValue) {
                if (m_history && m_document)
                    m_history->execute(std::make_unique<SetLayerTransformCommand>(
                        *m_document, id, oldValue, newValue));
            });
    connect(m_canvas, &CanvasView::deleteRequested,
            this, &MainWindow::deleteSelectedLayer);
    connect(m_canvas, &CanvasView::textBoxCommitted, this,
            [this](const LayerId& id, const QSizeF& oldValue, const QSizeF& newValue) {
                if (m_history && m_document)
                    m_history->execute(std::make_unique<SetLayerTextBoxCommand>(
                        *m_document, id, oldValue, newValue));
            });
    connect(m_canvas, &CanvasView::textCommitted, this,
            [this](const LayerId& id, const QString& oldValue,
                   const QString& newValue) {
                if (m_history && m_document)
                    m_history->execute(std::make_unique<LayerPropertyCommand<QString>>(
                        *m_document, id, QStringLiteral("layer.text"),
                        &Document::setLayerTextContent, oldValue, newValue));
            });

    setCentralWidget(m_canvas);
}

void MainWindow::buildLayersDock()
{
    m_textInspector = new TextInspector(m_i18n, m_document.get(), this);
    m_textDock = new QDockWidget(QString(), this);
    m_textDock->setWidget(m_textInspector);
    m_textDock->setFeatures(QDockWidget::DockWidgetMovable
                            | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, m_textDock);

    m_layersPanel = new LayersPanel(m_i18n, this);
    m_layersPanel->setDocument(m_document.get());

    m_layersDock = new QDockWidget(QString(), this);
    m_layersDock->setWidget(m_layersPanel);
    m_layersDock->setFeatures(QDockWidget::DockWidgetMovable
                              | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, m_layersDock);

    connect(m_layersPanel, &LayersPanel::selectionRequested, this,
            [this](const LayerId& id) {
                m_selectedId = id;
                m_canvas->setSelectedLayer(id);
            });
    connect(m_layersPanel, &LayersPanel::deleteRequested,
            this, &MainWindow::deleteSelectedLayer);
    connect(m_layersPanel, &LayersPanel::duplicateRequested, this,
            [this](const LayerId& id) {
                if (m_history && m_document)
                    m_history->execute(
                        std::make_unique<DuplicateLayerCommand>(*m_document, id));
            });
    connect(m_layersPanel, &LayersPanel::focusRequested, this,
            [this](const QPointF& documentPos) {
                if (m_canvas)
                    m_canvas->centerOn(documentPos);
            });
}

void MainWindow::buildActions()
{
    m_newAction = new QAction(this);
    m_newAction->setShortcut(QKeySequence::New);
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newDocument);

    m_importAction = new QAction(this);
    m_importAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+I")));
    connect(m_importAction, &QAction::triggered,
            this, &MainWindow::importImageViaDialog);

    m_quitAction = new QAction(this);
    connect(m_quitAction, &QAction::triggered, this, &MainWindow::close);

    m_undoAction = new QAction(this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered,
            this, [this] { m_history->undo(); });

    m_redoAction = new QAction(this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setEnabled(false);
    connect(m_redoAction, &QAction::triggered,
            this, [this] { m_history->redo(); });

    connect(m_history.get(), &CommandStack::canUndoChanged,
            m_undoAction, &QAction::setEnabled);
    connect(m_history.get(), &CommandStack::canRedoChanged,
            m_redoAction, &QAction::setEnabled);

    m_addTextAction = new QAction(this);
    m_addTextAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
    connect(m_addTextAction, &QAction::triggered,
            this, &MainWindow::addText);

    m_flipHAction = new QAction(this);
    connect(m_flipHAction, &QAction::triggered,
            this, [this] { flipLayer(true); });

    m_flipVAction = new QAction(this);
    connect(m_flipVAction, &QAction::triggered,
            this, [this] { flipLayer(false); });

    m_deleteAction = new QAction(this);
    m_deleteAction->setShortcut(QKeySequence::Delete);
    connect(m_deleteAction, &QAction::triggered,
            this, &MainWindow::deleteSelectedLayer);

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
    m_fileMenu->addAction(m_newAction);
    m_fileMenu->addAction(m_importAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_quitAction);

    m_editMenu = menuBar()->addMenu(QString());
    m_editMenu->addAction(m_undoAction);
    m_editMenu->addAction(m_redoAction);

    m_layerMenu = menuBar()->addMenu(QString());
    m_layerMenu->addAction(m_addTextAction);
    m_layerMenu->addAction(m_flipHAction);
    m_layerMenu->addAction(m_flipVAction);
    m_layerMenu->addSeparator();
    m_layerMenu->addAction(m_deleteAction);

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

void MainWindow::newDocument()
{
    NewDocumentDialog dialog(m_i18n, m_presetStore.get(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_document = createDocument(dialog.spec());
    m_history->clear();
    m_selectedId = LayerId();
    m_canvas->setDocument(m_document.get());
    if (m_layersPanel)
        m_layersPanel->setDocument(m_document.get());
    if (m_textInspector)
        m_textInspector->setSelectedLayer(LayerId());
    connectDocumentSignals();
}

void MainWindow::importImageViaDialog()
{
    if (!m_i18n)
        return;
    const QString path = QFileDialog::getOpenFileName(
        this,
        m_i18n->t("editor", "import.dialog.title"),
        QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (!path.isEmpty())
        importImage(path);
}

void MainWindow::importImage(const QString& filePath)
{
    const auto result = importImageFromFile(filePath);
    if (result.status != ImportStatus::Ok) {
        if (!m_i18n)
            return;
        QString reason;
        switch (result.status) {
        case ImportStatus::FileNotFound:
            reason = m_i18n->t("editor", "import.error.notFound"); break;
        case ImportStatus::CannotRead:
            reason = m_i18n->t("editor", "import.error.cannotRead"); break;
        case ImportStatus::TooLarge:
            reason = m_i18n->t("editor", "import.error.tooLarge"); break;
        case ImportStatus::UnsupportedFormat:
            reason = m_i18n->t("editor", "import.error.unsupported"); break;
        case ImportStatus::CorruptImage:
            reason = m_i18n->t("editor", "import.error.corrupt"); break;
        default:
            return;
        }
        QMessageBox::warning(this,
                             m_i18n->t("editor", "import.error.title"), reason);
        return;
    }

    const LayerId assetId = m_document->assets().add(result.encoded, result.format);
    if (assetId.isNull()) {
        if (m_i18n)
            QMessageBox::warning(this,
                                 m_i18n->t("editor", "import.error.title"),
                                 m_i18n->t("editor", "import.error.corrupt"));
        return;
    }

    auto layer = std::make_unique<ImageLayer>();
    layer->name = QFileInfo(filePath).fileName();
    layer->assetId = assetId;
    layer->naturalWidth = result.image.width();
    layer->naturalHeight = result.image.height();
    layer->transform.position = QPointF(m_document->width() / 2.0,
                                        m_document->height() / 2.0);

    m_history->execute(
        std::make_unique<AddLayerCommand>(*m_document, std::move(layer)));
}

void MainWindow::addText()
{
    if (!m_document || !m_i18n)
        return;

    auto layer = std::make_unique<TextLayer>();
    layer->name = QStringLiteral("Text");
    layer->content = m_i18n->t("editor", "text.default");
    layer->transform.position = QPointF(m_document->width() / 2.0,
                                        m_document->height() / 2.0);
    const LayerId id = layer->id();

    m_history->execute(
        std::make_unique<AddLayerCommand>(*m_document, std::move(layer)));
    m_canvas->setSelectedLayer(id);
    m_canvas->beginTextEdit(id);
}

void MainWindow::flipLayer(bool horizontal)
{
    if (!m_document || m_selectedId.isNull())
        return;
    Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer)
        return;

    const AffineTransform oldT = layer->transform;
    AffineTransform newT = oldT;
    if (horizontal)
        newT.scaleX = -newT.scaleX;
    else
        newT.scaleY = -newT.scaleY;

    if (m_history)
        m_history->execute(std::make_unique<SetLayerTransformCommand>(
            *m_document, m_selectedId, oldT, newT));
}

void MainWindow::deleteSelectedLayer()
{
    if (m_selectedId.isNull() || !m_document || !m_history)
        return;
    m_history->execute(
        std::make_unique<RemoveLayerCommand>(*m_document, m_selectedId));
    m_canvas->clearSelection();
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

    m_newAction->setText(m_i18n->t("common", "menu.file.new"));
    m_importAction->setText(m_i18n->t("common", "menu.file.import"));
    m_quitAction->setText(m_i18n->t("common", "menu.file.quit"));
    m_undoAction->setText(m_i18n->t("common", "menu.edit.undo"));
    m_redoAction->setText(m_i18n->t("common", "menu.edit.redo"));
    m_addTextAction->setText(m_i18n->t("common", "menu.layer.addText"));
    m_flipHAction->setText(m_i18n->t("common", "menu.layer.flipH"));
    m_flipVAction->setText(m_i18n->t("common", "menu.layer.flipV"));
    m_deleteAction->setText(m_i18n->t("common", "menu.layer.delete"));
    m_aboutAction->setText(m_i18n->t("common", "menu.help.about"));
    m_aboutQtAction->setText(m_i18n->t("common", "menu.help.aboutQt"));

    m_fileMenu->setTitle(m_i18n->t("common", "menu.file"));
    m_editMenu->setTitle(m_i18n->t("common", "menu.edit"));
    m_layerMenu->setTitle(m_i18n->t("common", "menu.layer"));
    m_helpMenu->setTitle(m_i18n->t("common", "menu.help"));

    if (m_layersDock)
        m_layersDock->setWindowTitle(m_i18n->t("common", "panel.layers"));

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
