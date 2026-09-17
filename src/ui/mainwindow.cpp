#include "mainwindow.h"

#include "core/document/NewDocumentSpec.h"
#include "core/templates/TemplateFactory.h"
#include "core/history/DocumentCommands.h"
#include "core/layers/Layer.h"
#include "core/serialization/ProjectFile.h"
#include "imageio/ImageImporter.h"
#include "imageio/DocumentExporter.h"
#include "rendering/CanvasRenderer.h"
#include "localization/i18nservice.h"
#include "ui/canvasview.h"
#include "ui/exportdialog.h"
#include "ui/layerspanel.h"
#include "ui/newdocumentdialog.h"
#include "ui/settingsdialog.h"
#include "ui/startscreen.h"
#include "services/autosaveservice.h"
#include "services/recentfiles.h"
#include "ui/textinspector.h"
#include "ui/shapeinspector.h"
#include "ui/imageinspector.h"
#include "ui/themeicons.h"

#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStackedWidget>
#include <QToolBar>

namespace cc {
namespace {

QImage generateThumbnail(const Document& doc)
{
    if (doc.width() <= 0 || doc.height() <= 0)
        return {};

    constexpr int maxThumbW = 320;
    constexpr int maxThumbH = 180;
    const double scale = qMin(static_cast<double>(maxThumbW) / doc.width(),
                              static_cast<double>(maxThumbH) / doc.height());
    const int thumbW = qMax(1, static_cast<int>(std::round(doc.width() * scale)));
    const int thumbH = qMax(1, static_cast<int>(std::round(doc.height() * scale)));

    QImage thumb(thumbW, thumbH, QImage::Format_ARGB32_Premultiplied);
    thumb.fill(Qt::transparent);
    {
        QPainter p(&thumb);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        QTransform t;
        t.scale(scale, scale);
        RenderOptions opt;
        opt.drawCheckerboard = false;
        opt.canvasBorder = Qt::transparent;
        renderDocument(doc, &p, t, opt);
    }
    return thumb;
}

} // namespace

MainWindow::MainWindow(SettingsService* settings, I18nService* i18n,
                       QWidget* parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_i18n(i18n)
{
    setMinimumSize(1000, 640);
    resize(1280, 800);
    setWindowIcon(ThemeIcons::appIcon());

    const QString configDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    m_presetStore =
        std::make_unique<PresetStore>(configDir + QStringLiteral("/presets.json"));
    m_history = std::make_unique<CommandStack>();

    const QString recoveryPath =
        configDir + QStringLiteral("/autosave/recovery.creatorcanvas");
    m_autosave = std::make_unique<AutosaveService>(m_settings, recoveryPath, this);
    m_recentFiles = std::make_unique<RecentFiles>(
        configDir + QStringLiteral("/recent.json"));

    createDefaultDocument();
    m_autosave->setDocument(m_document.get());
    buildCentralWidget();
    buildLayersDock();
    connectDocumentSignals();
    buildActions();
    buildToolBars();
    buildMenus();
    buildStatusBar();
    updateWindowTitle();

    connect(m_history.get(), &CommandStack::stateChanged, this, [this] {
        if (!m_modified) {
            m_modified = true;
            updateWindowTitle();
        }
    });

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &MainWindow::retranslateUi);
    retranslateUi();

    checkForRecoveryFile();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!confirmDiscardUnsavedChanges()) {
        event->ignore();
        return;
    }
    event->accept();
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
    m_startScreen = new StartScreen(m_i18n, m_presetStore.get(),
                                    m_recentFiles.get(), this);
    m_canvas = new CanvasView(this);
    m_canvas->setDocument(m_document.get());
    m_canvas->setI18n(m_i18n);

    m_centralStack = new QStackedWidget(this);
    m_centralStack->addWidget(m_startScreen); // page 0: start
    m_centralStack->addWidget(m_canvas);      // page 1: editor

    connect(m_startScreen, &StartScreen::createRequested,
            this, &MainWindow::startFromPreset);
    connect(m_startScreen, &StartScreen::customCreateRequested,
            this, &MainWindow::newDocument);
    connect(m_startScreen, &StartScreen::openRequested,
            this, &MainWindow::openDocument);
    connect(m_startScreen, &StartScreen::recentActivated,
            this, &MainWindow::openFromPath);
    connect(m_startScreen, &StartScreen::templateRequested,
            this, &MainWindow::startFromTemplate);

    connect(m_canvas, &CanvasView::zoomChanged,
            this, &MainWindow::updateZoomLabel);
    connect(m_canvas, &CanvasView::cursorMoved,
            this, &MainWindow::updatePositionLabel);
    connect(m_canvas, &CanvasView::fileDropped,
            this, &MainWindow::importImage);
    connect(m_canvas, &CanvasView::duplicateRequested, this,
            [this](const LayerId& id) {
                if (m_history && m_document)
                    m_history->execute(
                        std::make_unique<DuplicateLayerCommand>(*m_document, id));
            });
    connect(m_canvas, &CanvasView::deleteRequested,
            this, &MainWindow::deleteSelectedLayer);
    connect(m_canvas, &CanvasView::selectionChanged, this,
            [this](const LayerId& id) {
                m_selectedId = id;
                if (m_layersPanel)
                    m_layersPanel->setSelectedLayer(id);
                if (m_textInspector)
                    m_textInspector->setSelectedLayer(id);
                if (m_shapeInspector)
                    m_shapeInspector->setSelectedLayer(id);
                if (m_imageInspector)
                    m_imageInspector->setSelectedLayer(id);

                // Alterna automaticamente a aba ativa do dock de propriedades para o tipo da camada
                if (m_document) {
                    const Layer* layer = m_document->findLayer(id);
                    if (layer && layer->type() == LayerType::Shape && m_shapeDock) {
                        m_shapeDock->raise();
                    } else if (layer && layer->type() == LayerType::Text && m_textDock) {
                        m_textDock->raise();
                    } else if (layer && layer->type() == LayerType::Image && m_imageDock) {
                        m_imageDock->raise();
                    }
                }
            });
    connect(m_canvas, &CanvasView::multiSelectionChanged, this,
            [this](const QList<LayerId>& ids) {
                if (m_layersPanel)
                    m_layersPanel->setSelectedLayers(ids);
            });
    connect(m_canvas, &CanvasView::transformCommitted, this,
            [this](const LayerId& id, const AffineTransform& oldValue,
                   const AffineTransform& newValue) {
                if (m_history && m_document)
                    m_history->execute(std::make_unique<SetLayerTransformCommand>(
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
    connect(m_canvas, &CanvasView::textBoxCommitted, this,
            [this](const LayerId& id, const QSizeF& oldValue, const QSizeF& newValue) {
                if (m_history && m_document)
                    m_history->execute(std::make_unique<SetLayerTextBoxCommand>(
                        *m_document, id, oldValue, newValue));
            });
    connect(m_canvas, &CanvasView::deleteRequested,
            this, &MainWindow::deleteSelectedLayer);
    connect(m_canvas, &CanvasView::imageLayerModified, this,
            [this](const LayerId& id,
                   const LayerId& oldAssetId, int oldWidth, int oldHeight,
                   const AffineTransform& oldTransform,
                   const LayerId& newAssetId, int newWidth, int newHeight,
                   const AffineTransform& newTransform,
                   const QString& actionName) {
                if (m_history && m_document)
                    m_history->execute(std::make_unique<ModifyImageLayerCommand>(
                        *m_document, id,
                        oldAssetId, oldWidth, oldHeight, oldTransform,
                        newAssetId, newWidth, newHeight, newTransform,
                        actionName));
            });
    connect(m_canvas, &CanvasView::statusMessageRequested, this,
            [this](const QString& message) {
                statusBar()->showMessage(message, 3000);
            });
    connect(m_canvas, &CanvasView::toolChanged, this,
            [this](CanvasTool tool) {
                if (m_toolOptionsStack)
                    m_toolOptionsStack->setCurrentIndex(static_cast<int>(tool));
                if (m_toolGroup) {
                    switch (tool) {
                    case CanvasTool::Select:
                        if (m_toolSelectAction) m_toolSelectAction->setChecked(true);
                        break;
                    case CanvasTool::Crop:
                        if (m_toolCropAction) m_toolCropAction->setChecked(true);
                        break;
                    case CanvasTool::Scissors:
                        if (m_toolScissorsAction) m_toolScissorsAction->setChecked(true);
                        break;
                    case CanvasTool::MagicWand:
                        if (m_toolWandAction) m_toolWandAction->setChecked(true);
                        break;
                    case CanvasTool::CloneStamp:
                        if (m_toolCloneAction) m_toolCloneAction->setChecked(true);
                        break;
                    case CanvasTool::Paint:
                        if (m_toolPaintAction) m_toolPaintAction->setChecked(true);
                        break;
                    case CanvasTool::FloodFill:
                        if (m_toolFloodAction) m_toolFloodAction->setChecked(true);
                        break;
                    }
                }
            });

    setCentralWidget(m_centralStack);
}

void MainWindow::buildLayersDock()
{
    m_layersPanel = new LayersPanel(m_i18n, m_history.get(), this);
    m_layersPanel->setDocument(m_document.get());

    m_layersDock = new QDockWidget(QString(), this);
    m_layersDock->setWidget(m_layersPanel);
    m_layersDock->setFeatures(QDockWidget::DockWidgetMovable
                              | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, m_layersDock);
    m_layersDock->hide();

    connect(m_layersPanel, &LayersPanel::selectionRequested, this,
            [this](const LayerId& id) {
                m_selectedId = id;
                m_canvas->setSelectedLayer(id);
            });
    connect(m_layersPanel, &LayersPanel::multiSelectionRequested, this,
            [this](const QList<LayerId>& ids) {
                if (ids.isEmpty()) return;
                m_selectedId = ids.last();
                m_canvas->setMultiSelection(ids);
            });
    connect(m_layersPanel, &LayersPanel::deleteRequested,
            this, &MainWindow::deleteSelectedLayer);
    connect(m_layersPanel, &LayersPanel::duplicateRequested, this,
            [this](const LayerId& id) {
                if (m_history && m_document)
                    m_history->execute(
                        std::make_unique<DuplicateLayerCommand>(*m_document, id));
            });
    connect(m_layersPanel, &LayersPanel::addTextRequested, this, [this] {
        if (m_addTextAction) m_addTextAction->trigger();
    });
    connect(m_layersPanel, &LayersPanel::addShapeRequested, this, [this] {
        if (m_addRectAction) m_addRectAction->trigger();
    });

    m_textInspector = new TextInspector(m_i18n, m_document.get(), this);
    m_textDock = new QDockWidget(QString(), this);
    m_textDock->setWidget(m_textInspector);
    m_textDock->setFeatures(QDockWidget::DockWidgetMovable
                            | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, m_textDock);
    m_textDock->hide();

    // Cria o inspetor de formas e tabifica junto ao inspetor de texto na barra lateral direita
    m_shapeInspector = new ShapeInspector(m_i18n, m_document.get(), m_history.get(), this);
    m_shapeDock = new QDockWidget(QString(), this);
    m_shapeDock->setWidget(m_shapeInspector);
    m_shapeDock->setFeatures(QDockWidget::DockWidgetMovable
                             | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, m_shapeDock);
    tabifyDockWidget(m_textDock, m_shapeDock);
    m_shapeDock->hide();

    // Cria o inspetor de imagens e tabifica junto aos outros inspetores na barra lateral direita
    m_imageInspector = new ImageInspector(m_i18n, m_document.get(), m_history.get(), this);
    m_imageDock = new QDockWidget(QString(), this);
    m_imageDock->setWidget(m_imageInspector);
    m_imageDock->setFeatures(QDockWidget::DockWidgetMovable
                             | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, m_imageDock);
    tabifyDockWidget(m_shapeDock, m_imageDock);
    m_imageDock->hide();
}

void MainWindow::buildActions()
{
    m_newAction = new QAction(this);
    m_newAction->setIcon(ThemeIcons::actionNewDocument());
    m_newAction->setShortcut(QKeySequence::New);
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newDocument);

    m_openAction = new QAction(this);
    m_openAction->setIcon(ThemeIcons::actionOpenFolder());
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openDocument);

    m_saveAction = new QAction(this);
    m_saveAction->setIcon(ThemeIcons::actionSave());
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveDocument);

    m_saveAsAction = new QAction(this);
    m_saveAsAction->setIcon(ThemeIcons::actionSaveAs());
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveDocumentAs);

    m_importAction = new QAction(this);
    m_importAction->setIcon(ThemeIcons::actionImport());
    m_importAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+I")));
    connect(m_importAction, &QAction::triggered,
            this, &MainWindow::importImageViaDialog);

    m_exportAction = new QAction(this);
    m_exportAction->setIcon(ThemeIcons::actionExport());
    m_exportAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    connect(m_exportAction, &QAction::triggered,
            this, &MainWindow::exportImage);

    m_quitAction = new QAction(this);
    m_quitAction->setIcon(ThemeIcons::actionQuit());
    connect(m_quitAction, &QAction::triggered, this, &MainWindow::close);

    m_undoAction = new QAction(this);
    m_undoAction->setIcon(ThemeIcons::actionUndo());
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered,
            this, [this] { m_history->undo(); });

    m_redoAction = new QAction(this);
    m_redoAction->setIcon(ThemeIcons::actionRedo());
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setEnabled(false);
    connect(m_redoAction, &QAction::triggered,
            this, [this] { m_history->redo(); });

    connect(m_history.get(), &CommandStack::canUndoChanged,
            m_undoAction, &QAction::setEnabled);
    connect(m_history.get(), &CommandStack::canRedoChanged,
            m_redoAction, &QAction::setEnabled);

    m_zoomInAction = new QAction(this);
    m_zoomInAction->setIcon(ThemeIcons::actionZoomIn());
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->zoomIn();
    });

    m_zoomOutAction = new QAction(this);
    m_zoomOutAction->setIcon(ThemeIcons::actionZoomOut());
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->zoomOut();
    });

    m_zoomFitAction = new QAction(this);
    m_zoomFitAction->setIcon(ThemeIcons::actionZoomFit());
    m_zoomFitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(m_zoomFitAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->fitToViewport();
    });

    m_addTextAction = new QAction(this);
    m_addTextAction->setIcon(ThemeIcons::toolText());
    m_addTextAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
    connect(m_addTextAction, &QAction::triggered, this, &MainWindow::addText);

    // Ações para adicionar formas geométricas com atalhos padrão
    m_addRectAction = new QAction(this);
    m_addRectAction->setIcon(ThemeIcons::toolShape());
    m_addRectAction->setShortcut(QKeySequence(QStringLiteral("R")));
    connect(m_addRectAction, &QAction::triggered, this, [this] {
        addShape(ShapeKind::Rectangle);
    });

    m_addRoundedRectAction = new QAction(this);
    m_addRoundedRectAction->setIcon(ThemeIcons::toolShape());
    connect(m_addRoundedRectAction, &QAction::triggered, this, [this] {
        addShape(ShapeKind::RoundedRect);
    });

    m_addEllipseAction = new QAction(this);
    m_addEllipseAction->setIcon(ThemeIcons::toolShape());
    m_addEllipseAction->setShortcut(QKeySequence(QStringLiteral("O")));
    connect(m_addEllipseAction, &QAction::triggered, this, [this] {
        addShape(ShapeKind::Ellipse);
    });

    m_addLineAction = new QAction(this);
    m_addLineAction->setIcon(ThemeIcons::toolShape());
    m_addLineAction->setShortcut(QKeySequence(QStringLiteral("L")));
    connect(m_addLineAction, &QAction::triggered, this, [this] {
        addShape(ShapeKind::Line);
    });

    // Ações para alinhamento rápido da camada selecionada na tela
    m_alignLeftAction = new QAction(this);
    m_alignLeftAction->setIcon(ThemeIcons::actionAlignLeft());
    connect(m_alignLeftAction, &QAction::triggered, this, [this] {
        alignSelectedLayer(AlignTarget::Left);
    });

    m_alignCenterXAction = new QAction(this);
    m_alignCenterXAction->setIcon(ThemeIcons::actionAlignCenter());
    connect(m_alignCenterXAction, &QAction::triggered, this, [this] {
        alignSelectedLayer(AlignTarget::CenterX);
    });

    m_alignRightAction = new QAction(this);
    m_alignRightAction->setIcon(ThemeIcons::actionAlignRight());
    connect(m_alignRightAction, &QAction::triggered, this, [this] {
        alignSelectedLayer(AlignTarget::Right);
    });

    m_alignTopAction = new QAction(this);
    m_alignTopAction->setIcon(ThemeIcons::actionAlignTop());
    connect(m_alignTopAction, &QAction::triggered, this, [this] {
        alignSelectedLayer(AlignTarget::Top);
    });

    m_alignCenterYAction = new QAction(this);
    m_alignCenterYAction->setIcon(ThemeIcons::actionAlignMiddle());
    connect(m_alignCenterYAction, &QAction::triggered, this, [this] {
        alignSelectedLayer(AlignTarget::CenterY);
    });

    m_alignBottomAction = new QAction(this);
    m_alignBottomAction->setIcon(ThemeIcons::actionAlignBottom());
    connect(m_alignBottomAction, &QAction::triggered, this, [this] {
        alignSelectedLayer(AlignTarget::Bottom);
    });

    m_alignCenterBothAction = new QAction(this);
    m_alignCenterBothAction->setIcon(ThemeIcons::actionAlignCenter());
    connect(m_alignCenterBothAction, &QAction::triggered, this, [this] {
        alignSelectedLayer(AlignTarget::CenterBoth);
    });

    m_distributeHAction = new QAction(this);
    m_distributeHAction->setIcon(ThemeIcons::actionDistributeH());
    connect(m_distributeHAction, &QAction::triggered, this, &MainWindow::distributeHorizontally);

    m_distributeVAction = new QAction(this);
    m_distributeVAction->setIcon(ThemeIcons::actionDistributeV());
    connect(m_distributeVAction, &QAction::triggered, this, &MainWindow::distributeVertically);

    m_groupAction = new QAction(this);
    m_groupAction->setIcon(ThemeIcons::layerTypeGroup());
    m_groupAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(m_groupAction, &QAction::triggered, this, &MainWindow::groupSelectedLayers);

    m_toggleGridAction = new QAction(this);
    m_toggleGridAction->setIcon(ThemeIcons::actionGrid());
    m_toggleGridAction->setCheckable(true);
    m_toggleGridAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Apostrophe));
    connect(m_toggleGridAction, &QAction::triggered, this, [this](bool checked) {
        if (m_canvas) m_canvas->setShowGrid(checked);
    });

    m_toggleSnapAction = new QAction(this);
    m_toggleSnapAction->setIcon(ThemeIcons::actionSnap());
    m_toggleSnapAction->setCheckable(true);
    connect(m_toggleSnapAction, &QAction::triggered, this, [this](bool checked) {
        if (m_canvas) m_canvas->setSnapToGrid(checked);
    });

    m_flipHAction = new QAction(this);
    m_flipHAction->setIcon(ThemeIcons::actionFlipH());
    connect(m_flipHAction, &QAction::triggered,
            this, [this] { flipLayer(true); });

    m_flipVAction = new QAction(this);
    m_flipVAction->setIcon(ThemeIcons::actionFlipV());
    connect(m_flipVAction, &QAction::triggered,
            this, [this] { flipLayer(false); });

    m_deleteAction = new QAction(this);
    m_deleteAction->setIcon(ThemeIcons::actionDelete());
    m_deleteAction->setShortcut(QKeySequence::Delete);
    connect(m_deleteAction, &QAction::triggered,
            this, &MainWindow::deleteSelectedLayer);

    m_settingsAction = new QAction(this);
    m_settingsAction->setIcon(ThemeIcons::actionSettings());
    m_settingsAction->setShortcut(QKeySequence::Preferences);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::openSettings);

    m_startScreenAction = new QAction(this);
    m_startScreenAction->setIcon(ThemeIcons::actionNewDocument());
    connect(m_startScreenAction, &QAction::triggered, this, [this] {
        if (confirmDiscardUnsavedChanges())
            showStartScreen();
    });

    m_aboutAction = new QAction(this);
    m_aboutAction->setIcon(ThemeIcons::actionAbout());
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

    m_removeBgAiAction = new QAction(this);
    connect(m_removeBgAiAction, &QAction::triggered, this, [this] {
        if (m_canvas && m_document && !m_selectedId.isNull()) {
            m_canvas->openAiBackgroundRemoval(m_selectedId);
        }
    });

    m_removeBgAiQuickAction = new QAction(this);
    connect(m_removeBgAiQuickAction, &QAction::triggered, this, [this] {
        if (m_canvas && m_document && !m_selectedId.isNull()) {
            m_canvas->removeBackgroundAiQuick(m_selectedId);
        }
    });

    // Grupo de ferramentas exclusivas
    m_toolGroup = new QActionGroup(this);
    m_toolGroup->setExclusive(true);

    m_toolSelectAction = new QAction(this);
    m_toolSelectAction->setCheckable(true);
    m_toolSelectAction->setChecked(true);
    m_toolSelectAction->setShortcut(QKeySequence(QStringLiteral("V")));
    m_toolGroup->addAction(m_toolSelectAction);
    connect(m_toolSelectAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::Select);
    });

    m_toolCropAction = new QAction(this);
    m_toolCropAction->setCheckable(true);
    m_toolCropAction->setShortcut(QKeySequence(QStringLiteral("C")));
    m_toolGroup->addAction(m_toolCropAction);
    connect(m_toolCropAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::Crop);
    });

    m_toolScissorsAction = new QAction(this);
    m_toolScissorsAction->setCheckable(true);
    m_toolScissorsAction->setShortcut(QKeySequence(QStringLiteral("X")));
    m_toolGroup->addAction(m_toolScissorsAction);
    connect(m_toolScissorsAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::Scissors);
    });

    m_toolWandAction = new QAction(this);
    m_toolWandAction->setCheckable(true);
    m_toolWandAction->setShortcut(QKeySequence(QStringLiteral("W")));
    m_toolGroup->addAction(m_toolWandAction);
    connect(m_toolWandAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::MagicWand);
    });

    m_toolCloneAction = new QAction(this);
    m_toolCloneAction->setCheckable(true);
    m_toolCloneAction->setShortcut(QKeySequence(QStringLiteral("S")));
    m_toolGroup->addAction(m_toolCloneAction);
    connect(m_toolCloneAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::CloneStamp);
    });

    m_toolPaintAction = new QAction(this);
    m_toolPaintAction->setCheckable(true);
    m_toolPaintAction->setShortcut(QKeySequence(QStringLiteral("B")));
    m_toolGroup->addAction(m_toolPaintAction);
    connect(m_toolPaintAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::Paint);
    });

    m_toolFloodAction = new QAction(this);
    m_toolFloodAction->setCheckable(true);
    m_toolFloodAction->setShortcut(QKeySequence(QStringLiteral("G")));
    m_toolGroup->addAction(m_toolFloodAction);
    connect(m_toolFloodAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::FloodFill);
    });
}

void MainWindow::buildToolBars()
{
    // Barra de Ferramentas Principal (Lateral Esquerda, estilo profissional com ícones)
    m_toolsBar = new QToolBar(QStringLiteral("Tools"), this);
    m_toolsBar->setObjectName(QStringLiteral("ToolsToolBar"));
    m_toolsBar->setMovable(false);
    m_toolsBar->setFloatable(false);
    m_toolsBar->setOrientation(Qt::Vertical);
    m_toolsBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolsBar->setIconSize(QSize(24, 24));
    m_toolsBar->setStyleSheet(QStringLiteral(
        "QToolBar {"
        "  background-color: #1e222a;"
        "  border-right: 1px solid #2b303c;"
        "  padding: 4px 2px;"
        "  spacing: 4px;"
        "}"
        "QToolButton {"
        "  background-color: transparent;"
        "  border: 1px solid transparent;"
        "  border-radius: 6px;"
        "  padding: 5px;"
        "}"
        "QToolButton:hover {"
        "  background-color: #2b303c;"
        "  border-color: #3b4252;"
        "}"
        "QToolButton:checked {"
        "  background-color: #2b78e4;"
        "  border-color: #4b8df2;"
        "}"
    ));
    addToolBar(Qt::LeftToolBarArea, m_toolsBar);

    m_toolSelectAction->setIcon(ThemeIcons::toolSelect());
    m_toolCropAction->setIcon(ThemeIcons::toolCrop());
    m_toolScissorsAction->setIcon(ThemeIcons::toolScissors());
    m_toolWandAction->setIcon(ThemeIcons::toolWand());
    m_toolCloneAction->setIcon(ThemeIcons::toolClone());
    m_toolPaintAction->setIcon(ThemeIcons::toolPaint());
    m_toolFloodAction->setIcon(ThemeIcons::toolFlood());
    m_addTextAction->setIcon(ThemeIcons::toolText());
    m_addRectAction->setIcon(ThemeIcons::toolShape());

    m_toolsBar->addAction(m_toolSelectAction);
    m_toolsBar->addAction(m_toolCropAction);
    m_toolsBar->addAction(m_toolScissorsAction);
    m_toolsBar->addAction(m_toolWandAction);
    m_toolsBar->addAction(m_toolCloneAction);
    m_toolsBar->addAction(m_toolPaintAction);
    m_toolsBar->addAction(m_toolFloodAction);
    m_toolsBar->addSeparator();
    m_toolsBar->addAction(m_addTextAction);
    m_toolsBar->addAction(m_addRectAction);

    // Barra Superior de Ações Rápidas (Novo, Abrir, Salvar, Exportar, Desfazer, Zoom, Grade)
    m_quickToolBar = new QToolBar(QStringLiteral("QuickAccess"), this);
    m_quickToolBar->setObjectName(QStringLiteral("QuickAccessToolBar"));
    m_quickToolBar->setMovable(false);
    m_quickToolBar->setFloatable(false);
    m_quickToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_quickToolBar->setIconSize(QSize(20, 20));
    m_quickToolBar->setStyleSheet(QStringLiteral(
        "QToolBar {"
        "  background-color: #1e222a;"
        "  border-bottom: 1px solid #2b303c;"
        "  padding: 3px 6px;"
        "  spacing: 3px;"
        "}"
        "QToolButton {"
        "  background-color: transparent;"
        "  border: 1px solid transparent;"
        "  border-radius: 4px;"
        "  padding: 4px;"
        "}"
        "QToolButton:hover {"
        "  background-color: #2b303c;"
        "  border-color: #3b4252;"
        "}"
        "QToolButton:pressed {"
        "  background-color: #1a1d24;"
        "}"
        "QToolButton:disabled {"
        "  opacity: 0.35;"
        "}"
    ));
    addToolBar(Qt::TopToolBarArea, m_quickToolBar);

    m_quickToolBar->addAction(m_newAction);
    m_quickToolBar->addAction(m_openAction);
    m_quickToolBar->addAction(m_saveAction);
    m_quickToolBar->addAction(m_exportAction);
    m_quickToolBar->addSeparator();
    m_quickToolBar->addAction(m_undoAction);
    m_quickToolBar->addAction(m_redoAction);
    m_quickToolBar->addSeparator();
    m_quickToolBar->addAction(m_zoomOutAction);
    m_quickToolBar->addAction(m_zoomInAction);
    m_quickToolBar->addAction(m_zoomFitAction);
    m_quickToolBar->addSeparator();
    m_quickToolBar->addAction(m_toggleGridAction);
    m_quickToolBar->addAction(m_toggleSnapAction);

    // Barra Superior de Opções de Ferramentas (Tool Options Bar)
    m_toolOptionsBar = new QToolBar(QStringLiteral("ToolOptions"), this);
    m_toolOptionsBar->setObjectName(QStringLiteral("ToolOptionsBar"));
    m_toolOptionsBar->setMovable(false);
    m_toolOptionsBar->setFloatable(false);
    addToolBar(Qt::TopToolBarArea, m_toolOptionsBar);

    m_toolOptionsStack = new QStackedWidget(this);

    // --- Página 0: Seleção (Select) ---
    QWidget* selectPage = new QWidget(this);
    QHBoxLayout* selectLayout = new QHBoxLayout(selectPage);
    selectLayout->setContentsMargins(8, 2, 8, 2);
    m_selectHintLabel = new QLabel(this);
    m_selectHintLabel->setStyleSheet(QStringLiteral("color: #888888; font-size: 11px;"));
    selectLayout->addWidget(m_selectHintLabel);
    selectLayout->addStretch();
    m_toolOptionsStack->addWidget(selectPage);

    // --- Página 1: Corte Retangular/Proporção (Crop) ---
    QWidget* cropPage = new QWidget(this);
    QHBoxLayout* cropLayout = new QHBoxLayout(cropPage);
    cropLayout->setContentsMargins(8, 2, 8, 2);
    cropLayout->setSpacing(8);

    m_cropAspectLabel = new QLabel(this);
    m_cropAspectCombo = new QComboBox(this);
    m_cropAspectCombo->addItem(QStringLiteral("Free"), 0.0);
    m_cropAspectCombo->addItem(QStringLiteral("1:1 (Square)"), 1.0);
    m_cropAspectCombo->addItem(QStringLiteral("16:9 (Widescreen)"), 16.0 / 9.0);
    m_cropAspectCombo->addItem(QStringLiteral("4:3 (Standard)"), 4.0 / 3.0);
    m_cropAspectCombo->addItem(QStringLiteral("9:16 (Stories/Reels)"), 9.0 / 16.0);
    m_cropAspectCombo->addItem(QStringLiteral("4:5 (Instagram Portrait)"), 4.0 / 5.0);
    connect(m_cropAspectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_canvas && index >= 0) {
            double ratio = m_cropAspectCombo->itemData(index).toDouble();
            m_canvas->setCropAspectRatio(ratio);
        }
    });

    m_cropApplyBtn = new QPushButton(this);
    m_cropApplyBtn->setIcon(ThemeIcons::actionCheck());
    m_cropApplyBtn->setIconSize(QSize(16, 16));
    m_cropApplyBtn->setStyleSheet(QStringLiteral("background-color: #2b78e4; color: white; font-weight: bold; padding: 4px 12px; border-radius: 4px;"));
    connect(m_cropApplyBtn, &QPushButton::clicked, this, [this] {
        if (m_canvas) m_canvas->applyCrop();
    });

    m_cropCancelBtn = new QPushButton(this);
    m_cropCancelBtn->setIcon(ThemeIcons::actionCancel());
    m_cropCancelBtn->setIconSize(QSize(16, 16));
    connect(m_cropCancelBtn, &QPushButton::clicked, this, [this] {
        if (m_canvas) m_canvas->cancelCrop();
    });

    cropLayout->addWidget(m_cropAspectLabel);
    cropLayout->addWidget(m_cropAspectCombo);
    cropLayout->addWidget(m_cropApplyBtn);
    cropLayout->addWidget(m_cropCancelBtn);
    cropLayout->addStretch();
    m_toolOptionsStack->addWidget(cropPage);

    // --- Página 2: Corte com Tesoura (Scissors Cut) ---
    QWidget* scissorsPage = new QWidget(this);
    QHBoxLayout* scissorsLayout = new QHBoxLayout(scissorsPage);
    scissorsLayout->setContentsMargins(8, 2, 8, 2);
    scissorsLayout->setSpacing(8);

    m_scissorsModeLabel = new QLabel(this);
    m_scissorsModeCombo = new QComboBox(this);
    m_scissorsModeCombo->addItem(QStringLiteral("Keep Inside (Cutout)"), true);
    m_scissorsModeCombo->addItem(QStringLiteral("Erase Inside (Hole)"), false);
    connect(m_scissorsModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_canvas && index >= 0) {
            bool keep = m_scissorsModeCombo->itemData(index).toBool();
            m_canvas->setScissorsKeepInside(keep);
        }
    });

    m_scissorsAutoCropCheck = new QCheckBox(this);
    m_scissorsAutoCropCheck->setChecked(true);
    connect(m_scissorsAutoCropCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_canvas) m_canvas->setScissorsAutoCrop(checked);
    });

    m_scissorsApplyBtn = new QPushButton(this);
    m_scissorsApplyBtn->setIcon(ThemeIcons::actionCheck(QColor(0, 0, 0)));
    m_scissorsApplyBtn->setIconSize(QSize(16, 16));
    m_scissorsApplyBtn->setStyleSheet(QStringLiteral("background-color: #00bcd4; color: black; font-weight: bold; padding: 4px 12px; border-radius: 4px;"));
    connect(m_scissorsApplyBtn, &QPushButton::clicked, this, [this] {
        if (m_canvas) m_canvas->applyScissorsCut();
    });

    m_scissorsCancelBtn = new QPushButton(this);
    m_scissorsCancelBtn->setIcon(ThemeIcons::actionCancel());
    m_scissorsCancelBtn->setIconSize(QSize(16, 16));
    connect(m_scissorsCancelBtn, &QPushButton::clicked, this, [this] {
        if (m_canvas) m_canvas->cancelScissorsCut();
    });

    m_scissorsHintLabel = new QLabel(this);
    m_scissorsHintLabel->setStyleSheet(QStringLiteral("color: #888888; font-size: 11px;"));

    scissorsLayout->addWidget(m_scissorsModeLabel);
    scissorsLayout->addWidget(m_scissorsModeCombo);
    scissorsLayout->addWidget(m_scissorsAutoCropCheck);
    scissorsLayout->addWidget(m_scissorsApplyBtn);
    scissorsLayout->addWidget(m_scissorsCancelBtn);
    scissorsLayout->addWidget(m_scissorsHintLabel);
    scissorsLayout->addStretch();
    m_toolOptionsStack->addWidget(scissorsPage);

    // --- Página 3: Varinha Mágica (Magic Wand) ---
    QWidget* wandPage = new QWidget(this);
    QHBoxLayout* wandLayout = new QHBoxLayout(wandPage);
    wandLayout->setContentsMargins(8, 2, 8, 2);
    wandLayout->setSpacing(8);

    m_wandTolLabel = new QLabel(this);
    m_wandTolSlider = new QSlider(Qt::Horizontal, this);
    m_wandTolSlider->setRange(0, 100);
    m_wandTolSlider->setValue(25);
    m_wandTolSlider->setFixedWidth(120);
    m_wandTolValueLabel = new QLabel(QStringLiteral("25%"), this);
    m_wandTolValueLabel->setFixedWidth(36);

    connect(m_wandTolSlider, &QSlider::valueChanged, this, [this](int val) {
        m_wandTolValueLabel->setText(QString::number(val) + QStringLiteral("%"));
        if (m_canvas) m_canvas->setWandTolerance(val);
    });

    m_wandContiguousCheck = new QCheckBox(this);
    m_wandContiguousCheck->setChecked(true);
    connect(m_wandContiguousCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_canvas) m_canvas->setWandContiguous(checked);
    });

    m_wandHintLabel = new QLabel(this);
    m_wandHintLabel->setStyleSheet(QStringLiteral("color: #888888; font-size: 11px;"));

    wandLayout->addWidget(m_wandTolLabel);
    wandLayout->addWidget(m_wandTolSlider);
    wandLayout->addWidget(m_wandTolValueLabel);
    wandLayout->addWidget(m_wandContiguousCheck);
    wandLayout->addWidget(m_wandHintLabel);
    wandLayout->addStretch();
    m_toolOptionsStack->addWidget(wandPage);

    // --- Página 4: Carimbo de Clonagem (Clone Stamp) ---
    QWidget* clonePage = new QWidget(this);
    QHBoxLayout* cloneLayout = new QHBoxLayout(clonePage);
    cloneLayout->setContentsMargins(8, 2, 8, 2);
    cloneLayout->setSpacing(8);

    m_cloneRadiusLabel = new QLabel(this);
    m_cloneRadiusSlider = new QSlider(Qt::Horizontal, this);
    m_cloneRadiusSlider->setRange(1, 100);
    m_cloneRadiusSlider->setValue(20);
    m_cloneRadiusSlider->setFixedWidth(100);
    m_cloneRadiusValueLabel = new QLabel(QStringLiteral("20px"), this);
    m_cloneRadiusValueLabel->setFixedWidth(36);

    connect(m_cloneRadiusSlider, &QSlider::valueChanged, this, [this](int val) {
        m_cloneRadiusValueLabel->setText(QString::number(val) + QStringLiteral("px"));
        if (m_canvas) m_canvas->setCloneRadius(val);
    });

    m_cloneHardnessLabel = new QLabel(this);
    m_cloneHardnessSlider = new QSlider(Qt::Horizontal, this);
    m_cloneHardnessSlider->setRange(0, 100);
    m_cloneHardnessSlider->setValue(80);
    m_cloneHardnessSlider->setFixedWidth(100);
    m_cloneHardnessValueLabel = new QLabel(QStringLiteral("80%"), this);
    m_cloneHardnessValueLabel->setFixedWidth(36);

    connect(m_cloneHardnessSlider, &QSlider::valueChanged, this, [this](int val) {
        m_cloneHardnessValueLabel->setText(QString::number(val) + QStringLiteral("%"));
        if (m_canvas) m_canvas->setCloneHardness(val / 100.0);
    });

    m_cloneOpacityLabel = new QLabel(this);
    m_cloneOpacitySlider = new QSlider(Qt::Horizontal, this);
    m_cloneOpacitySlider->setRange(10, 100);
    m_cloneOpacitySlider->setValue(100);
    m_cloneOpacitySlider->setFixedWidth(100);
    m_cloneOpacityValueLabel = new QLabel(QStringLiteral("100%"), this);
    m_cloneOpacityValueLabel->setFixedWidth(40);

    connect(m_cloneOpacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_cloneOpacityValueLabel->setText(QString::number(val) + QStringLiteral("%"));
        if (m_canvas) m_canvas->setCloneOpacity(val / 100.0);
    });

    m_cloneHintLabel = new QLabel(this);
    m_cloneHintLabel->setStyleSheet(QStringLiteral("color: #888888; font-size: 11px;"));

    cloneLayout->addWidget(m_cloneRadiusLabel);
    cloneLayout->addWidget(m_cloneRadiusSlider);
    cloneLayout->addWidget(m_cloneRadiusValueLabel);
    cloneLayout->addWidget(m_cloneHardnessLabel);
    cloneLayout->addWidget(m_cloneHardnessSlider);
    cloneLayout->addWidget(m_cloneHardnessValueLabel);
    cloneLayout->addWidget(m_cloneOpacityLabel);
    cloneLayout->addWidget(m_cloneOpacitySlider);
    cloneLayout->addWidget(m_cloneOpacityValueLabel);
    cloneLayout->addWidget(m_cloneHintLabel);
    cloneLayout->addStretch();
    m_toolOptionsStack->addWidget(clonePage);

    // --- Página 5: Pintura (Paint Tool) ---
    QWidget* paintPage = new QWidget(this);
    QHBoxLayout* paintLayout = new QHBoxLayout(paintPage);
    paintLayout->setContentsMargins(8, 2, 8, 2);
    paintLayout->setSpacing(8);

    m_paintBrushLabel = new QLabel(this);
    m_paintBrushCombo = new QComboBox(this);
    m_paintBrushCombo->addItem(QStringLiteral("Pincel"), 0);
    m_paintBrushCombo->addItem(QStringLiteral("Lápis"), 1);
    m_paintBrushCombo->addItem(QStringLiteral("Marca-Texto"), 2);
    m_paintBrushCombo->addItem(QStringLiteral("Aerógrafo"), 3);
    m_paintBrushCombo->addItem(QStringLiteral("Borracha"), 4);
    connect(m_paintBrushCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_canvas && idx >= 0) m_canvas->setPaintBrush(idx);
    });

    m_paintColorBtn = new QPushButton(this);
    m_paintColorBtn->setFixedSize(28, 24);
    m_paintColorBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_currentPaintColor.name()));
    connect(m_paintColorBtn, &QPushButton::clicked, this, [this] {
        const QColor chosen = QColorDialog::getColor(m_currentPaintColor, this);
        if (chosen.isValid()) {
            m_currentPaintColor = chosen;
            m_paintColorBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(chosen.name()));
            if (m_floodColorBtn)
                m_floodColorBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(chosen.name()));
            if (m_canvas) m_canvas->setPaintColor(chosen);
        }
    });

    m_paintSizeLabel = new QLabel(this);
    m_paintSizeSlider = new QSlider(Qt::Horizontal, this);
    m_paintSizeSlider->setRange(1, 200);
    m_paintSizeSlider->setValue(20);
    m_paintSizeSlider->setFixedWidth(100);
    m_paintSizeValueLabel = new QLabel(QStringLiteral("20px"), this);
    m_paintSizeValueLabel->setFixedWidth(36);
    connect(m_paintSizeSlider, &QSlider::valueChanged, this, [this](int val) {
        m_paintSizeValueLabel->setText(QString::number(val) + QStringLiteral("px"));
        if (m_canvas) m_canvas->setPaintSize(val);
    });

    m_paintOpacityLabel = new QLabel(this);
    m_paintOpacitySlider = new QSlider(Qt::Horizontal, this);
    m_paintOpacitySlider->setRange(10, 100);
    m_paintOpacitySlider->setValue(100);
    m_paintOpacitySlider->setFixedWidth(100);
    m_paintOpacityValueLabel = new QLabel(QStringLiteral("100%"), this);
    m_paintOpacityValueLabel->setFixedWidth(40);
    connect(m_paintOpacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_paintOpacityValueLabel->setText(QString::number(val) + QStringLiteral("%"));
        if (m_canvas) m_canvas->setPaintOpacity(val / 100.0);
    });

    paintLayout->addWidget(m_paintBrushLabel);
    paintLayout->addWidget(m_paintBrushCombo);
    paintLayout->addWidget(m_paintColorBtn);
    paintLayout->addWidget(m_paintSizeLabel);
    paintLayout->addWidget(m_paintSizeSlider);
    paintLayout->addWidget(m_paintSizeValueLabel);
    paintLayout->addWidget(m_paintOpacityLabel);
    paintLayout->addWidget(m_paintOpacitySlider);
    paintLayout->addWidget(m_paintOpacityValueLabel);
    paintLayout->addStretch();
    m_toolOptionsStack->addWidget(paintPage);

    // --- Página 6: Preenchimento (Flood Fill Tool) ---
    QWidget* floodPage = new QWidget(this);
    QHBoxLayout* floodLayout = new QHBoxLayout(floodPage);
    floodLayout->setContentsMargins(8, 2, 8, 2);
    floodLayout->setSpacing(8);

    m_floodColorBtn = new QPushButton(this);
    m_floodColorBtn->setFixedSize(28, 24);
    m_floodColorBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_currentPaintColor.name()));
    connect(m_floodColorBtn, &QPushButton::clicked, this, [this] {
        const QColor chosen = QColorDialog::getColor(m_currentPaintColor, this);
        if (chosen.isValid()) {
            m_currentPaintColor = chosen;
            m_floodColorBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(chosen.name()));
            if (m_paintColorBtn)
                m_paintColorBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(chosen.name()));
            if (m_canvas) m_canvas->setPaintColor(chosen);
        }
    });

    m_floodTolLabel = new QLabel(this);
    m_floodTolSlider = new QSlider(Qt::Horizontal, this);
    m_floodTolSlider->setRange(0, 100);
    m_floodTolSlider->setValue(25);
    m_floodTolSlider->setFixedWidth(120);
    m_floodTolValueLabel = new QLabel(QStringLiteral("25%"), this);
    m_floodTolValueLabel->setFixedWidth(36);
    connect(m_floodTolSlider, &QSlider::valueChanged, this, [this](int val) {
        m_floodTolValueLabel->setText(QString::number(val) + QStringLiteral("%"));
        if (m_canvas) m_canvas->setWandTolerance(val);
    });

    floodLayout->addWidget(m_floodColorBtn);
    floodLayout->addWidget(m_floodTolLabel);
    floodLayout->addWidget(m_floodTolSlider);
    floodLayout->addWidget(m_floodTolValueLabel);
    floodLayout->addStretch();
    m_toolOptionsStack->addWidget(floodPage);

    m_toolOptionsBar->addWidget(m_toolOptionsStack);

    m_quickToolBar->hide();
    m_toolsBar->hide();
    m_toolOptionsBar->hide();
}

void MainWindow::buildMenus()
{
    m_fileMenu = menuBar()->addMenu(QString());
    m_fileMenu->addAction(m_newAction);
    m_fileMenu->addAction(m_openAction);
    m_fileMenu->addAction(m_saveAction);
    m_fileMenu->addAction(m_saveAsAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_importAction);
    m_fileMenu->addAction(m_exportAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_quitAction);

    m_editMenu = menuBar()->addMenu(QString());
    m_editMenu->addAction(m_undoAction);
    m_editMenu->addAction(m_redoAction);

    m_layerMenu = menuBar()->addMenu(QString());
    m_layerMenu->addAction(m_addTextAction);

    // Submenu para adicionar diferentes tipos de formas geométricas
    m_addShapeMenu = m_layerMenu->addMenu(QString());
    m_addShapeMenu->addAction(m_addRectAction);
    m_addShapeMenu->addAction(m_addRoundedRectAction);
    m_addShapeMenu->addAction(m_addEllipseAction);
    m_addShapeMenu->addAction(m_addLineAction);

    // Submenu de alinhamento rápido da camada em relação à tela
    m_alignMenu = m_layerMenu->addMenu(QString());
    m_alignMenu->addAction(m_alignLeftAction);
    m_alignMenu->addAction(m_alignCenterXAction);
    m_alignMenu->addAction(m_alignRightAction);
    m_alignMenu->addSeparator();
    m_alignMenu->addAction(m_alignTopAction);
    m_alignMenu->addAction(m_alignCenterYAction);
    m_alignBottomAction->setIcon(ThemeIcons::actionAlignBottom());
    m_alignMenu->addAction(m_alignBottomAction);
    m_alignMenu->addSeparator();
    m_alignMenu->addAction(m_alignCenterBothAction);
    m_alignMenu->addSeparator();
    m_alignMenu->addAction(m_distributeHAction);
    m_alignMenu->addAction(m_distributeVAction);

    m_layerMenu->addAction(m_groupAction);
    m_layerMenu->addSeparator();
    m_layerMenu->addAction(m_flipHAction);
    m_layerMenu->addAction(m_flipVAction);
    m_layerMenu->addSeparator();
    m_layerMenu->addAction(m_removeBgAiAction);
    m_layerMenu->addAction(m_removeBgAiQuickAction);
    m_layerMenu->addSeparator();
    m_layerMenu->addAction(m_deleteAction);

    m_viewMenu = menuBar()->addMenu(QString());
    m_viewMenu->addAction(m_zoomInAction);
    m_viewMenu->addAction(m_zoomOutAction);
    m_viewMenu->addAction(m_zoomFitAction);
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_toggleGridAction);
    m_viewMenu->addAction(m_toggleSnapAction);
    m_viewMenu->addSeparator();
    m_safeZoneMenu = m_viewMenu->addMenu(QString());
    m_safeNoneAction = m_safeZoneMenu->addAction(QString());
    m_safeNoneAction->setCheckable(true);
    m_safeNoneAction->setChecked(true);
    m_safeYouTubeAction = m_safeZoneMenu->addAction(QString());
    m_safeYouTubeAction->setCheckable(true);
    m_safeInstagramAction = m_safeZoneMenu->addAction(QString());
    m_safeInstagramAction->setCheckable(true);
    m_safeTikTokAction = m_safeZoneMenu->addAction(QString());
    m_safeTikTokAction->setCheckable(true);

    auto* safeGroup = new QActionGroup(this);
    safeGroup->addAction(m_safeNoneAction);
    safeGroup->addAction(m_safeYouTubeAction);
    safeGroup->addAction(m_safeInstagramAction);
    safeGroup->addAction(m_safeTikTokAction);

    connect(m_safeNoneAction, &QAction::triggered, this, [this]{ if(m_canvas) m_canvas->setSafeZoneMode(0); });
    connect(m_safeYouTubeAction, &QAction::triggered, this, [this]{ if(m_canvas) m_canvas->setSafeZoneMode(1); });
    connect(m_safeInstagramAction, &QAction::triggered, this, [this]{ if(m_canvas) m_canvas->setSafeZoneMode(2); });
    connect(m_safeTikTokAction, &QAction::triggered, this, [this]{ if(m_canvas) m_canvas->setSafeZoneMode(3); });

    m_toolsMenu = menuBar()->addMenu(QString());
    m_toolsMenu->addAction(m_toolSelectAction);
    m_toolsMenu->addAction(m_toolCropAction);
    m_toolsMenu->addAction(m_toolScissorsAction);
    m_toolsMenu->addAction(m_toolWandAction);
    m_toolsMenu->addAction(m_toolCloneAction);
    m_toolsMenu->addAction(m_toolPaintAction);
    m_toolsMenu->addAction(m_toolFloodAction);

    m_settingsMenu = menuBar()->addMenu(QString());
    m_settingsMenu->addAction(m_settingsAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_startScreenAction);

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

void MainWindow::showStartScreen()
{
    m_centralStack->setCurrentWidget(m_startScreen);
    if (m_layersDock) m_layersDock->hide();
    if (m_textDock) m_textDock->hide();
    if (m_shapeDock) m_shapeDock->hide();
    if (m_imageDock) m_imageDock->hide();
    if (m_quickToolBar) m_quickToolBar->hide();
    if (m_toolsBar) m_toolsBar->hide();
    if (m_toolOptionsBar) m_toolOptionsBar->hide();
    if (m_startScreen) m_startScreen->refreshRecents();
}

void MainWindow::enterEditor()
{
    m_centralStack->setCurrentWidget(m_canvas);
    if (m_layersDock) m_layersDock->show();
    if (m_textDock) m_textDock->show();
    if (m_shapeDock) m_shapeDock->show();
    if (m_imageDock) m_imageDock->show();
    if (m_quickToolBar) m_quickToolBar->show();
    if (m_toolsBar) m_toolsBar->show();
    if (m_toolOptionsBar) m_toolOptionsBar->show();
    updateWindowTitle();
}

void MainWindow::startFromPreset(const NewDocumentSpec& spec)
{
    if (!confirmDiscardUnsavedChanges())
        return;

    m_document = createDocument(spec);
    m_history->clear();
    m_selectedId = LayerId();
    m_currentFilePath.clear();
    m_modified = false;
    if (m_autosave)
        m_autosave->setDocument(m_document.get());
    m_canvas->setDocument(m_document.get());
    if (m_layersPanel)
        m_layersPanel->setDocument(m_document.get());
    if (m_textInspector)
        m_textInspector->setDocument(m_document.get());
    if (m_shapeInspector)
        m_shapeInspector->setDocument(m_document.get());
    if (m_imageInspector)
        m_imageInspector->setDocument(m_document.get());
    connectDocumentSignals();
    enterEditor();
    updateWindowTitle();
}

void MainWindow::openFromPath(const QString& path)
{
    if (!confirmDiscardUnsavedChanges())
        return;

    QString error;
    auto loaded = loadDocument(path, &error);
    if (!loaded) {
        if (m_i18n)
            QMessageBox::warning(this,
                                 m_i18n->t("common", "dialog.openError.title"),
                                 error);
        return;
    }

    m_document = std::move(loaded);
    m_history->clear();
    m_selectedId = LayerId();
    m_currentFilePath = path;
    m_modified = false;
    if (m_autosave) {
        m_autosave->discardRecovery();
        m_autosave->setDocument(m_document.get());
    }
    m_canvas->setDocument(m_document.get());
    if (m_layersPanel)
        m_layersPanel->setDocument(m_document.get());
    if (m_textInspector)
        m_textInspector->setDocument(m_document.get());
    if (m_shapeInspector)
        m_shapeInspector->setDocument(m_document.get());
    if (m_imageInspector)
        m_imageInspector->setDocument(m_document.get());
    connectDocumentSignals();
    if (m_recentFiles)
        m_recentFiles->push(path);
    enterEditor();
    updateWindowTitle();
}

void MainWindow::newDocument()
{
    if (!confirmDiscardUnsavedChanges())
        return;

    NewDocumentDialog dialog(m_i18n, m_presetStore.get(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_document = createDocument(dialog.spec());
    m_history->clear();
    m_selectedId = LayerId();
    m_currentFilePath.clear();
    m_modified = false;
    if (m_autosave) {
        m_autosave->discardRecovery();
        m_autosave->setDocument(m_document.get());
    }
    m_canvas->setDocument(m_document.get());
    if (m_layersPanel)
        m_layersPanel->setDocument(m_document.get());
    if (m_textInspector)
        m_textInspector->setDocument(m_document.get());
    if (m_shapeInspector)
        m_shapeInspector->setDocument(m_document.get());
    connectDocumentSignals();
    updateWindowTitle();
    enterEditor();
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

void MainWindow::addShape(ShapeKind kind)
{
    if (!m_document || !m_i18n)
        return;

    auto layer = std::make_unique<ShapeLayer>();
    layer->kind = kind;

    // Dimensões do documento para calcular proporções adequadas
    const double docW = m_document->width();
    const double docH = m_document->height();

    // Centraliza a nova forma no meio do documento
    layer->transform.position = QPointF(docW / 2.0, docH / 2.0);

    switch (kind) {
    case ShapeKind::Rectangle: {
        // Remove '&' do mnemônico da tradução para o nome da camada ficar limpo
        layer->name = m_i18n->t("common", "menu.layer.shape.rect").remove('&');
        const double w = std::clamp(docW * 0.3, 100.0, 400.0);
        const double h = std::clamp(docH * 0.25, 80.0, 300.0);
        layer->points = QPolygonF{
            QPointF(0, 0), QPointF(w, 0), QPointF(w, h), QPointF(0, h)
        };
        layer->fill = QColor(64, 128, 255);
        layer->stroke = QColor(30, 30, 30);
        layer->strokeWidth = 2.0;
        break;
    }
    case ShapeKind::RoundedRect: {
        layer->name = m_i18n->t("common", "menu.layer.shape.roundedRect").remove('&');
        const double w = std::clamp(docW * 0.3, 100.0, 400.0);
        const double h = std::clamp(docH * 0.25, 80.0, 300.0);
        layer->points = QPolygonF{
            QPointF(0, 0), QPointF(w, 0), QPointF(w, h), QPointF(0, h)
        };
        layer->cornerRadius = 16.0;
        layer->fill = QColor(64, 128, 255);
        layer->stroke = QColor(30, 30, 30);
        layer->strokeWidth = 2.0;
        break;
    }
    case ShapeKind::Ellipse: {
        layer->name = m_i18n->t("common", "menu.layer.shape.ellipse").remove('&');
        const double size = std::clamp(std::min(docW, docH) * 0.25, 100.0, 300.0);
        layer->points = QPolygonF{
            QPointF(0, 0), QPointF(size, 0), QPointF(size, size), QPointF(0, size)
        };
        layer->fill = QColor(255, 105, 180);
        layer->stroke = QColor(30, 30, 30);
        layer->strokeWidth = 2.0;
        break;
    }
    case ShapeKind::Line: {
        layer->name = m_i18n->t("common", "menu.layer.shape.line").remove('&');
        const double len = std::clamp(docW * 0.35, 100.0, 500.0);
        layer->points = QPolygonF{
            QPointF(0, 0), QPointF(len, 0)
        };
        layer->stroke = QColor(30, 30, 30);
        layer->strokeWidth = 4.0;
        layer->fill = Qt::transparent;
        break;
    }
    default:
        break;
    }

    const LayerId id = layer->id();

    // Adiciona a forma através do histórico (suporte a Desfazer/Refazer)
    m_history->execute(
        std::make_unique<AddLayerCommand>(*m_document, std::move(layer)));

    // Seleciona a camada recém-criada
    m_canvas->setSelectedLayer(id);
}

void MainWindow::alignSelectedLayer(AlignTarget target)
{
    if (!m_document)
        return;

    if (m_canvas && m_canvas->selectedLayers().size() > 1) {
        alignMultipleLayers(target);
        return;
    }

    if (m_selectedId.isNull())
        return;

    Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer || layer->locked)
        return;

    const QRectF local = layer->contentBounds();
    if (local.isEmpty())
        return;

    const QRectF docBounds = layer->transform.matrix(local).mapRect(local);
    const double docW = m_document->width();
    const double docH = m_document->height();

    double dx = 0.0;
    double dy = 0.0;

    switch (target) {
    case AlignTarget::Left:
        dx = -docBounds.left();
        break;
    case AlignTarget::CenterX:
        dx = (docW / 2.0) - docBounds.center().x();
        break;
    case AlignTarget::Right:
        dx = docW - docBounds.right();
        break;
    case AlignTarget::Top:
        dy = -docBounds.top();
        break;
    case AlignTarget::CenterY:
        dy = (docH / 2.0) - docBounds.center().y();
        break;
    case AlignTarget::Bottom:
        dy = docH - docBounds.bottom();
        break;
    case AlignTarget::CenterBoth:
        dx = (docW / 2.0) - docBounds.center().x();
        dy = (docH / 2.0) - docBounds.center().y();
        break;
    }

    if (qFuzzyIsNull(dx) && qFuzzyIsNull(dy))
        return;

    const AffineTransform oldT = layer->transform;
    AffineTransform newT = oldT;
    newT.position += QPointF(dx, dy);

    if (m_history) {
        m_history->execute(std::make_unique<SetLayerTransformCommand>(
            *m_document, m_selectedId, oldT, newT));
    }
}

void MainWindow::alignMultipleLayers(AlignTarget target)
{
    if (!m_document || !m_canvas)
        return;
    const QList<LayerId> ids = m_canvas->selectedLayers();
    if (ids.size() < 2)
        return;

    struct LayerInfo {
        LayerId id;
        QRectF docBounds;
        AffineTransform oldTransform;
    };
    QVector<LayerInfo> infos;
    QRectF unionRect;

    for (const auto& lid : ids) {
        Layer* layer = m_document->findLayer(lid);
        if (!layer || layer->locked)
            continue;
        const QRectF local = layer->contentBounds();
        if (local.isEmpty())
            continue;
        const QRectF docBounds = layer->transform.matrix(local).mapRect(local);
        infos.append({lid, docBounds, layer->transform});
        unionRect = unionRect.isNull() ? docBounds : unionRect.united(docBounds);
    }
    if (infos.size() < 2 || unionRect.isEmpty())
        return;

    for (const auto& info : infos) {
        double dx = 0.0;
        double dy = 0.0;

        switch (target) {
        case AlignTarget::Left:
            dx = unionRect.left() - info.docBounds.left();
            break;
        case AlignTarget::CenterX:
            dx = unionRect.center().x() - info.docBounds.center().x();
            break;
        case AlignTarget::Right:
            dx = unionRect.right() - info.docBounds.right();
            break;
        case AlignTarget::Top:
            dy = unionRect.top() - info.docBounds.top();
            break;
        case AlignTarget::CenterY:
            dy = unionRect.center().y() - info.docBounds.center().y();
            break;
        case AlignTarget::Bottom:
            dy = unionRect.bottom() - info.docBounds.bottom();
            break;
        case AlignTarget::CenterBoth:
            dx = unionRect.center().x() - info.docBounds.center().x();
            dy = unionRect.center().y() - info.docBounds.center().y();
            break;
        }

        if (!qFuzzyIsNull(dx) || !qFuzzyIsNull(dy)) {
            AffineTransform newT = info.oldTransform;
            newT.position += QPointF(dx, dy);
            if (m_history) {
                m_history->execute(std::make_unique<SetLayerTransformCommand>(
                    *m_document, info.id, info.oldTransform, newT));
            } else {
                m_document->setLayerTransform(info.id, newT);
            }
        }
    }
    m_canvas->update();
}

void MainWindow::distributeHorizontally()
{
    if (!m_document || !m_canvas)
        return;
    const QList<LayerId> ids = m_canvas->selectedLayers();
    if (ids.size() < 3)
        return;

    struct Info {
        LayerId id;
        QRectF docBounds;
        AffineTransform oldTransform;
        double centerX;
    };
    QVector<Info> infos;
    for (const auto& lid : ids) {
        Layer* layer = m_document->findLayer(lid);
        if (!layer || layer->locked)
            continue;
        const QRectF local = layer->contentBounds();
        if (local.isEmpty())
            continue;
        const QRectF db = layer->transform.matrix(local).mapRect(local);
        infos.append({lid, db, layer->transform, db.center().x()});
    }
    if (infos.size() < 3)
        return;

    std::sort(infos.begin(), infos.end(), [](const Info& a, const Info& b) {
        return a.centerX < b.centerX;
    });

    const double firstCenter = infos.first().centerX;
    const double lastCenter = infos.last().centerX;
    const double step = (lastCenter - firstCenter) / (infos.size() - 1);

    for (int i = 1; i < infos.size() - 1; ++i) {
        const double targetX = firstCenter + step * i;
        const double dx = targetX - infos[i].centerX;
        if (!qFuzzyIsNull(dx)) {
            AffineTransform newT = infos[i].oldTransform;
            newT.position += QPointF(dx, 0.0);
            if (m_history) {
                m_history->execute(std::make_unique<SetLayerTransformCommand>(
                    *m_document, infos[i].id, infos[i].oldTransform, newT));
            } else {
                m_document->setLayerTransform(infos[i].id, newT);
            }
        }
    }
    m_canvas->update();
}

void MainWindow::distributeVertically()
{
    if (!m_document || !m_canvas)
        return;
    const QList<LayerId> ids = m_canvas->selectedLayers();
    if (ids.size() < 3)
        return;

    struct Info {
        LayerId id;
        QRectF docBounds;
        AffineTransform oldTransform;
        double centerY;
    };
    QVector<Info> infos;
    for (const auto& lid : ids) {
        Layer* layer = m_document->findLayer(lid);
        if (!layer || layer->locked)
            continue;
        const QRectF local = layer->contentBounds();
        if (local.isEmpty())
            continue;
        const QRectF db = layer->transform.matrix(local).mapRect(local);
        infos.append({lid, db, layer->transform, db.center().y()});
    }
    if (infos.size() < 3)
        return;

    std::sort(infos.begin(), infos.end(), [](const Info& a, const Info& b) {
        return a.centerY < b.centerY;
    });

    const double firstCenter = infos.first().centerY;
    const double lastCenter = infos.last().centerY;
    const double step = (lastCenter - firstCenter) / (infos.size() - 1);

    for (int i = 1; i < infos.size() - 1; ++i) {
        const double targetY = firstCenter + step * i;
        const double dy = targetY - infos[i].centerY;
        if (!qFuzzyIsNull(dy)) {
            AffineTransform newT = infos[i].oldTransform;
            newT.position += QPointF(0.0, dy);
            if (m_history) {
                m_history->execute(std::make_unique<SetLayerTransformCommand>(
                    *m_document, infos[i].id, infos[i].oldTransform, newT));
            } else {
                m_document->setLayerTransform(infos[i].id, newT);
            }
        }
    }
    m_canvas->update();
}

void MainWindow::groupSelectedLayers()
{
    if (!m_document || !m_canvas)
        return;
    const QList<LayerId> ids = m_canvas->selectedLayers();
    if (ids.size() < 2)
        return;

    auto group = std::make_unique<GroupLayer>();
    group->name = m_i18n ? m_i18n->t("editor", "layer.group") : QStringLiteral("Group");
    const LayerId groupId = group->id();

    for (const auto& lid : ids) {
        if (auto taken = m_document->takeLayer(lid))
            group->children.push_back(std::move(taken));
    }

    m_document->addLayer(std::move(group));
    m_canvas->setSelectedLayer(groupId);
    m_canvas->update();
    if (m_layersPanel)
        m_layersPanel->refresh();
}

void MainWindow::startFromTemplate(int templateKind)
{
    if (!confirmDiscardUnsavedChanges())
        return;

    auto kind = static_cast<TemplateKind>(templateKind);
    auto doc = TemplateFactory::createTemplate(kind, m_i18n);
    if (!doc)
        return;

    m_document = std::move(doc);
    m_currentFilePath.clear();
    m_modified = false;
    m_selectedId = LayerId();
    if (m_history)
        m_history->clear();
    if (m_autosave)
        m_autosave->setDocument(m_document.get());
    m_canvas->setDocument(m_document.get());
    if (m_layersPanel)
        m_layersPanel->setDocument(m_document.get());
    if (m_textInspector)
        m_textInspector->setDocument(m_document.get());
    if (m_shapeInspector)
        m_shapeInspector->setDocument(m_document.get());
    if (m_imageInspector)
        m_imageInspector->setDocument(m_document.get());
    connectDocumentSignals();
    m_canvas->fitToViewport();
    enterEditor();
    updateWindowTitle();
}

void MainWindow::exportImage()
{
    if (!m_i18n || !m_document)
        return;

    ExportDialog dialog(m_i18n, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const ExportSettings settings = dialog.settings();
    QString filter = QStringLiteral("PNG (*.png)");
    if (settings.format == QLatin1String("jpeg"))
        filter = QStringLiteral("JPEG (*.jpg *.jpeg)");
    else if (settings.format == QLatin1String("webp"))
        filter = QStringLiteral("WebP (*.webp)");

    QString path = QFileDialog::getSaveFileName(
        this, m_i18n->t("editor", "export.dialog.title"),
        QStringLiteral("export"), filter);
    if (path.isEmpty())
        return;

    const QString suffix = QStringLiteral(".") + settings.format;
    if (!path.endsWith(suffix))
        path += suffix;

    QString error;
    if (!exportDocumentToImage(*m_document, path, settings.format,
                               settings.quality, settings.scale, &error)) {
        QMessageBox::warning(this,
                             m_i18n->t("editor", "export.error.title"), error);
        return;
    }
    QMessageBox::information(
        this, m_i18n->t("editor", "export.success.title"),
        m_i18n->t("editor", "export.success").arg(path));
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

bool MainWindow::saveDocument()
{
    if (m_currentFilePath.isEmpty())
        return saveDocumentAs();

    QString error;
    const QImage thumb = generateThumbnail(*m_document);
    if (!cc::saveDocument(*m_document, m_currentFilePath, &error, &thumb)) {
        if (m_i18n)
            QMessageBox::warning(this,
                                 m_i18n->t("common", "dialog.saveError.title"),
                                 error);
        return false;
    }
    m_modified = false;
    if (m_autosave)
        m_autosave->discardRecovery();
    updateWindowTitle();
    return true;
}

bool MainWindow::saveDocumentAs()
{
    if (!m_i18n)
        return false;
    QString path = QFileDialog::getSaveFileName(
        this,
        m_i18n->t("common", "dialog.saveAs.title"),
        QStringLiteral("untitled.creatorcanvas"),
        QStringLiteral("CreatorCanvas (*.creatorcanvas)"));
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QLatin1String(".creatorcanvas")))
        path += QLatin1String(".creatorcanvas");

    QString error;
    const QImage thumb = generateThumbnail(*m_document);
    if (!cc::saveDocument(*m_document, path, &error, &thumb)) {
        QMessageBox::warning(this,
                             m_i18n->t("common", "dialog.saveError.title"), error);
        return false;
    }
    m_currentFilePath = path;
    m_modified = false;
    if (m_autosave)
        m_autosave->discardRecovery();
    updateWindowTitle();
    return true;
}

void MainWindow::openDocument()
{
    if (!m_i18n)
        return;
    const QString path = QFileDialog::getOpenFileName(
        this,
        m_i18n->t("common", "dialog.open.title"),
        QString(),
        QStringLiteral("CreatorCanvas (*.creatorcanvas)"));
    if (path.isEmpty())
        return;
    openFromPath(path);
}

bool MainWindow::confirmDiscardUnsavedChanges()
{
    if (!m_modified || !m_i18n)
        return true;

    QMessageBox box(this);
    box.setWindowTitle(m_i18n->t("common", "dialog.unsaved.title"));
    box.setText(m_i18n->t("common", "dialog.unsaved.body"));
    QAbstractButton* saveButton =
        box.addButton(m_i18n->t("common", "dialog.unsaved.save"),
                      QMessageBox::AcceptRole);
    QAbstractButton* discardButton =
        box.addButton(m_i18n->t("common", "dialog.unsaved.discard"),
                      QMessageBox::DestructiveRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();

    if (box.clickedButton() == saveButton)
        return saveDocument();
    return box.clickedButton() == discardButton;
}

void MainWindow::checkForRecoveryFile()
{
    if (!m_autosave || !m_autosave->hasRecoveryFile())
        return;
    if (!m_i18n) {
        m_autosave->discardRecovery();
        return;
    }

    QMessageBox box(this);
    box.setWindowTitle(m_i18n->t("common", "dialog.recovery.title"));
    box.setText(m_i18n->t("common", "dialog.recovery.body"));
    auto* recoverButton =
        box.addButton(m_i18n->t("common", "dialog.recovery.recover"),
                      QMessageBox::AcceptRole);
    box.addButton(m_i18n->t("common", "dialog.recovery.discard"),
                  QMessageBox::DestructiveRole);
    box.exec();

    if (box.clickedButton() != recoverButton) {
        m_autosave->discardRecovery();
        return;
    }

    QString error;
    auto loaded = loadDocument(m_autosave->recoveryFilePath(), &error);
    if (!loaded) {
        QMessageBox::warning(this,
                             m_i18n->t("common", "dialog.openError.title"), error);
        m_autosave->discardRecovery();
        return;
    }

    m_document = std::move(loaded);
    m_history->clear();
    m_selectedId = LayerId();
    m_currentFilePath.clear();
    m_modified = true; // recovered session stays unsaved until explicitly saved
    m_canvas->setDocument(m_document.get());
    if (m_layersPanel)
        m_layersPanel->setDocument(m_document.get());
    if (m_textInspector)
        m_textInspector->setDocument(m_document.get());
    if (m_shapeInspector)
        m_shapeInspector->setDocument(m_document.get());
    m_autosave->setDocument(m_document.get());
    connectDocumentSignals();
    updateWindowTitle();
    enterEditor();
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(m_i18n, m_settings, this);
    dialog.exec();
}

void MainWindow::updateWindowTitle()
{
    QString title = m_i18n ? m_i18n->t("common", "app.title")
                           : QStringLiteral("CreatorCanvas");
    if (!m_currentFilePath.isEmpty())
        title += QStringLiteral(" - ")
                 + QFileInfo(m_currentFilePath).fileName();
    if (m_modified)
        title += QStringLiteral(" *");
    setWindowTitle(title);
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

    m_newAction->setText(m_i18n->t("common", "menu.file.new"));
    m_openAction->setText(m_i18n->t("common", "menu.file.open"));
    m_saveAction->setText(m_i18n->t("common", "menu.file.save"));
    m_saveAsAction->setText(m_i18n->t("common", "menu.file.saveAs"));
    m_importAction->setText(m_i18n->t("common", "menu.file.import"));
    m_exportAction->setText(m_i18n->t("common", "menu.file.export"));
    m_quitAction->setText(m_i18n->t("common", "menu.file.quit"));
    m_undoAction->setText(m_i18n->t("common", "menu.edit.undo"));
    m_redoAction->setText(m_i18n->t("common", "menu.edit.redo"));
    m_addTextAction->setText(m_i18n->t("common", "menu.layer.addText"));
    if (m_addShapeMenu)
        m_addShapeMenu->setTitle(m_i18n->t("common", "menu.layer.addShape"));
    if (m_addRectAction)
        m_addRectAction->setText(m_i18n->t("common", "menu.layer.shape.rect"));
    if (m_addRoundedRectAction)
        m_addRoundedRectAction->setText(m_i18n->t("common", "menu.layer.shape.roundedRect"));
    if (m_addEllipseAction)
        m_addEllipseAction->setText(m_i18n->t("common", "menu.layer.shape.ellipse"));
    if (m_addLineAction)
        m_addLineAction->setText(m_i18n->t("common", "menu.layer.shape.line"));
    if (m_alignMenu)
        m_alignMenu->setTitle(m_i18n->t("common", "menu.layer.align"));
    if (m_alignLeftAction)
        m_alignLeftAction->setText(m_i18n->t("common", "menu.layer.align.left"));
    if (m_alignCenterXAction)
        m_alignCenterXAction->setText(m_i18n->t("common", "menu.layer.align.centerX"));
    if (m_alignRightAction)
        m_alignRightAction->setText(m_i18n->t("common", "menu.layer.align.right"));
    if (m_alignTopAction)
        m_alignTopAction->setText(m_i18n->t("common", "menu.layer.align.top"));
    if (m_alignCenterYAction)
        m_alignCenterYAction->setText(m_i18n->t("common", "menu.layer.align.centerY"));
    if (m_alignBottomAction)
        m_alignBottomAction->setText(m_i18n->t("common", "menu.layer.align.bottom"));
    if (m_alignCenterBothAction)
        m_alignCenterBothAction->setText(m_i18n->t("common", "menu.layer.align.centerBoth"));
    if (m_distributeHAction)
        m_distributeHAction->setText(m_i18n->t("common", "menu.layer.distribute.horizontal"));
    if (m_distributeVAction)
        m_distributeVAction->setText(m_i18n->t("common", "menu.layer.distribute.vertical"));
    if (m_groupAction)
        m_groupAction->setText(m_i18n->t("common", "menu.layer.group"));

    if (m_viewMenu)
        m_viewMenu->setTitle(m_i18n->t("common", "menu.view"));
    if (m_zoomInAction) {
        const QString text = m_i18n->currentLanguage() == QStringLiteral("pt-BR")
            ? QStringLiteral("Aumentar Zoom") : QStringLiteral("Zoom In");
        m_zoomInAction->setText(text);
        m_zoomInAction->setToolTip(text + QStringLiteral(" (Ctrl++)"));
    }
    if (m_zoomOutAction) {
        const QString text = m_i18n->currentLanguage() == QStringLiteral("pt-BR")
            ? QStringLiteral("Diminuir Zoom") : QStringLiteral("Zoom Out");
        m_zoomOutAction->setText(text);
        m_zoomOutAction->setToolTip(text + QStringLiteral(" (Ctrl+-)"));
    }
    if (m_zoomFitAction) {
        const QString text = m_i18n->currentLanguage() == QStringLiteral("pt-BR")
            ? QStringLiteral("Ajustar à Janela") : QStringLiteral("Fit to Viewport");
        m_zoomFitAction->setText(text);
        m_zoomFitAction->setToolTip(text + QStringLiteral(" (Ctrl+0)"));
    }
    m_newAction->setToolTip(m_newAction->text() + QStringLiteral(" (Ctrl+N)"));
    m_openAction->setToolTip(m_openAction->text() + QStringLiteral(" (Ctrl+O)"));
    m_saveAction->setToolTip(m_saveAction->text() + QStringLiteral(" (Ctrl+S)"));
    m_exportAction->setToolTip(m_exportAction->text() + QStringLiteral(" (Ctrl+E)"));
    m_undoAction->setToolTip(m_undoAction->text() + QStringLiteral(" (Ctrl+Z)"));
    m_redoAction->setToolTip(m_redoAction->text() + QStringLiteral(" (Ctrl+Y)"));
    if (m_toggleGridAction) {
        m_toggleGridAction->setText(m_i18n->t("common", "menu.view.showGrid"));
        m_toggleGridAction->setToolTip(m_toggleGridAction->text() + QStringLiteral(" (Ctrl+')"));
    }
    if (m_toggleSnapAction) {
        m_toggleSnapAction->setText(m_i18n->t("common", "menu.view.snapToGrid"));
        m_toggleSnapAction->setToolTip(m_toggleSnapAction->text());
    }
    if (m_safeZoneMenu)
        m_safeZoneMenu->setTitle(m_i18n->t("common", "menu.view.safeZones"));
    if (m_safeNoneAction)
        m_safeNoneAction->setText(m_i18n->t("common", "menu.view.safe.none"));
    if (m_safeYouTubeAction)
        m_safeYouTubeAction->setText(m_i18n->t("common", "menu.view.safe.youtube"));
    if (m_safeInstagramAction)
        m_safeInstagramAction->setText(m_i18n->t("common", "menu.view.safe.instagram"));
    if (m_safeTikTokAction)
        m_safeTikTokAction->setText(m_i18n->t("common", "menu.view.safe.tiktok"));

    m_flipHAction->setText(m_i18n->t("common", "menu.layer.flipH"));
    m_flipVAction->setText(m_i18n->t("common", "menu.layer.flipV"));
    m_deleteAction->setText(m_i18n->t("common", "menu.layer.delete"));
    m_settingsAction->setText(m_i18n->t("common", "menu.settings.open"));
    m_startScreenAction->setText(m_i18n->t("common", "start.screenAction"));
    m_aboutAction->setText(m_i18n->t("common", "menu.help.about"));
    m_aboutQtAction->setText(m_i18n->t("common", "menu.help.aboutQt"));

    if (m_removeBgAiAction) {
        m_removeBgAiAction->setText(m_i18n->currentLanguage() == QStringLiteral("pt-BR")
            ? QStringLiteral("Remover Fundo com IA...")
            : QStringLiteral("Remove Background with AI..."));
    }
    if (m_removeBgAiQuickAction) {
        m_removeBgAiQuickAction->setText(m_i18n->currentLanguage() == QStringLiteral("pt-BR")
            ? QStringLiteral("Remover Fundo Rápido (1-Clique)")
            : QStringLiteral("Quick Remove Background (1-Click)"));
    }

    m_fileMenu->setTitle(m_i18n->t("common", "menu.file"));
    m_editMenu->setTitle(m_i18n->t("common", "menu.edit"));
    m_layerMenu->setTitle(m_i18n->t("common", "menu.layer"));
    m_settingsMenu->setTitle(m_i18n->t("common", "menu.settings"));
    m_helpMenu->setTitle(m_i18n->t("common", "menu.help"));

    if (m_layersDock)
        m_layersDock->setWindowTitle(m_i18n->t("common", "panel.layers"));
    if (m_textDock)
        m_textDock->setWindowTitle(m_i18n->t("editor", "text.title"));
    if (m_shapeDock)
        m_shapeDock->setWindowTitle(m_i18n->t("editor", "shape.title"));
    if (m_imageDock)
        m_imageDock->setWindowTitle(m_i18n->t("common", "panel.imageProperties"));

    // Retradução das ferramentas da barra e opções com tooltips contendo atalhos
    if (m_toolSelectAction) {
        m_toolSelectAction->setText(m_i18n->t("editor", "tools.select"));
        m_toolSelectAction->setToolTip(QStringLiteral("%1 (V)").arg(m_i18n->t("editor", "tools.select")));
    }
    if (m_toolCropAction) {
        m_toolCropAction->setText(m_i18n->t("editor", "tools.crop"));
        m_toolCropAction->setToolTip(QStringLiteral("%1 (C)").arg(m_i18n->t("editor", "tools.crop")));
    }
    if (m_toolScissorsAction) {
        m_toolScissorsAction->setText(m_i18n->t("editor", "tools.scissors"));
        m_toolScissorsAction->setToolTip(QStringLiteral("%1 (X)").arg(m_i18n->t("editor", "tools.scissors")));
    }
    if (m_toolWandAction) {
        m_toolWandAction->setText(m_i18n->t("editor", "tools.wand"));
        m_toolWandAction->setToolTip(QStringLiteral("%1 (W)").arg(m_i18n->t("editor", "tools.wand")));
    }
    if (m_toolCloneAction) {
        m_toolCloneAction->setText(m_i18n->t("editor", "tools.clone"));
        m_toolCloneAction->setToolTip(QStringLiteral("%1 (S)").arg(m_i18n->t("editor", "tools.clone")));
    }
    if (m_toolPaintAction) {
        m_toolPaintAction->setText(m_i18n->t("editor", "tools.paintName"));
        m_toolPaintAction->setToolTip(QStringLiteral("%1 (B)").arg(m_i18n->t("editor", "tools.paintName")));
    }
    if (m_toolFloodAction) {
        m_toolFloodAction->setText(m_i18n->t("editor", "tools.floodName"));
        m_toolFloodAction->setToolTip(QStringLiteral("%1 (G)").arg(m_i18n->t("editor", "tools.floodName")));
    }
    if (m_addTextAction) {
        m_addTextAction->setToolTip(QStringLiteral("%1 (T)").arg(m_i18n->t("common", "menu.layer.addText")));
    }
    if (m_addRectAction) {
        m_addRectAction->setToolTip(QStringLiteral("%1 (U)").arg(m_i18n->t("common", "menu.layer.shape.rect")));
    }

    if (m_toolsMenu)
        m_toolsMenu->setTitle(m_i18n->t("editor", "tools.title"));

    if (m_selectHintLabel)
        m_selectHintLabel->setText(m_i18n->t("editor", "tools.selectHint"));

    if (m_cropAspectLabel)
        m_cropAspectLabel->setText(m_i18n->t("editor", "tools.crop.aspect") + QStringLiteral(":"));
    if (m_cropApplyBtn)
        m_cropApplyBtn->setText(m_i18n->t("editor", "tools.crop.apply") + QStringLiteral(" (Enter)"));
    if (m_cropCancelBtn)
        m_cropCancelBtn->setText(m_i18n->t("editor", "tools.crop.cancel") + QStringLiteral(" (Esc)"));

    if (m_cropAspectCombo && m_cropAspectCombo->count() >= 6) {
        m_cropAspectCombo->setItemText(0, m_i18n->t("editor", "tools.crop.free"));
        m_cropAspectCombo->setItemText(1, m_i18n->t("editor", "tools.crop.square"));
        m_cropAspectCombo->setItemText(2, m_i18n->t("editor", "tools.crop.widescreen"));
        m_cropAspectCombo->setItemText(3, m_i18n->t("editor", "tools.crop.standard"));
        m_cropAspectCombo->setItemText(4, m_i18n->t("editor", "tools.crop.portrait"));
        m_cropAspectCombo->setItemText(5, QStringLiteral("4:5 (Instagram)"));
    }

    if (m_scissorsModeLabel)
        m_scissorsModeLabel->setText(m_i18n->t("editor", "tools.scissors.mode") + QStringLiteral(":"));
    if (m_scissorsModeCombo && m_scissorsModeCombo->count() >= 2) {
        m_scissorsModeCombo->setItemText(0, m_i18n->t("editor", "tools.scissors.keepInside"));
        m_scissorsModeCombo->setItemText(1, m_i18n->t("editor", "tools.scissors.eraseInside"));
    }
    if (m_scissorsAutoCropCheck)
        m_scissorsAutoCropCheck->setText(m_i18n->t("editor", "tools.scissors.autoCrop"));
    if (m_scissorsApplyBtn)
        m_scissorsApplyBtn->setText(m_i18n->t("editor", "tools.scissors.apply") + QStringLiteral(" (Enter)"));
    if (m_scissorsCancelBtn)
        m_scissorsCancelBtn->setText(m_i18n->t("editor", "tools.scissors.cancel") + QStringLiteral(" (Esc)"));
    if (m_scissorsHintLabel)
        m_scissorsHintLabel->setText(m_i18n->currentLanguage() == QStringLiteral("pt-BR")
            ? QStringLiteral("Clique ou arraste para contornar. Dê dois cliques ou Enter para cortar.")
            : QStringLiteral("Click or drag to outline. Double-click or press Enter to cut."));

    if (m_wandTolLabel)
        m_wandTolLabel->setText(m_i18n->t("editor", "tools.wand.tolerance") + QStringLiteral(":"));
    if (m_wandContiguousCheck)
        m_wandContiguousCheck->setText(m_i18n->t("editor", "tools.wand.contiguous"));
    if (m_wandHintLabel)
        m_wandHintLabel->setText(m_i18n->currentLanguage() == QStringLiteral("pt-BR")
            ? QStringLiteral("Clique em uma cor na imagem para torná-la transparente.")
            : QStringLiteral("Click a color on the image to make it transparent."));

    if (m_cloneRadiusLabel)
        m_cloneRadiusLabel->setText(m_i18n->t("editor", "tools.clone.radius") + QStringLiteral(":"));
    if (m_cloneHardnessLabel)
        m_cloneHardnessLabel->setText(m_i18n->t("editor", "tools.clone.hardness") + QStringLiteral(":"));
    if (m_cloneOpacityLabel)
        m_cloneOpacityLabel->setText(m_i18n->t("editor", "tools.clone.opacity") + QStringLiteral(":"));
    if (m_cloneHintLabel)
        m_cloneHintLabel->setText(m_i18n->currentLanguage() == QStringLiteral("pt-BR")
            ? QStringLiteral("Botão Direito ou Shift+Clique define a origem. Arraste para clonar.")
            : QStringLiteral("Right-Click or Shift+Click sets source. Drag to clone."));

    if (m_paintBrushLabel)
        m_paintBrushLabel->setText(m_i18n->currentLanguage() == QStringLiteral("pt-BR") ? QStringLiteral("Pincel:") : QStringLiteral("Brush:"));
    if (m_paintSizeLabel)
        m_paintSizeLabel->setText(m_i18n->currentLanguage() == QStringLiteral("pt-BR") ? QStringLiteral("Tamanho:") : QStringLiteral("Size:"));
    if (m_paintOpacityLabel)
        m_paintOpacityLabel->setText(m_i18n->currentLanguage() == QStringLiteral("pt-BR") ? QStringLiteral("Opacidade:") : QStringLiteral("Opacity:"));
    if (m_floodTolLabel)
        m_floodTolLabel->setText(m_i18n->currentLanguage() == QStringLiteral("pt-BR") ? QStringLiteral("Tolerância:") : QStringLiteral("Tolerance:"));

    updateZoomLabel();
    updatePositionLabel(m_lastCursorPos);

    m_versionLabel->setText(
        m_i18n->t("common", "statusbar.version")
            .arg(QApplication::applicationVersion()));
    m_languageLabel->setText(
        m_i18n->t("common", "statusbar.language")
            .arg(m_i18n->displayName(m_i18n->currentLanguage())));

    updateWindowTitle();
}

} // namespace cc
