#include "mainwindow.h"

#include "core/document/NewDocumentSpec.h"
#include "core/DocumentController.h"
#include "core/selection/SelectionEngine.h"
#include "core/templates/TemplateFactory.h"
#include "core/history/DocumentCommands.h"
#include "core/commands/LayerPropertyCommands.h"
#include "core/layers/Layer.h"
#include "core/serialization/ProjectFile.h"
#include "imageio/ImageImporter.h"
#include "imageio/DocumentExporter.h"
#include "core/operations/LayerOperations.h"
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
#include "services/settingsservice.h"
#include "ui/textinspector.h"
#include "ui/shapeinspector.h"
#include "ui/imageinspector.h"
#include "ui/historypanel.h"
#include "ui/designtokens.h"
#include "ui/recoverydialog.h"
#include "ui/themeicons.h"
#include "ui/tooloptionsbar.h"
#include "ui/actionmanager.h"
#include "ui/menubuilder.h"

#include <QActionGroup>
#include <QApplication>
#include <QGuiApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStackedWidget>
#include <QToolBar>
#include <QToolButton>
#include <QStyle>
#include <QScrollArea>
#include <QTabWidget>
#include <QTabBar>

#include <functional>

namespace cc {
namespace {




/// Diagnóstico: atalhos duplicados no mesmo QMainWindow viram atalhos
/// AMBÍGUOS e nenhum dos dois dispara. Foi exatamente o caso de "R"
/// (Retângulo) × Retoque e Ctrl+E (Export) × Merge Selected.
void warnDuplicateShortcuts(const QWidget* window)
{
    if (!window)
        return;
    QHash<QString, QAction*> seen;
    for (QAction* action : window->findChildren<QAction*>()) {
        if (!action || action->shortcut().isEmpty())
            continue;
        const QString key = action->shortcut().toString(QKeySequence::PortableText);
        if (key.isEmpty())
            continue;
        if (seen.contains(key) && seen.value(key) != action) {
            qWarning() << "[Shortcuts] atalho ambíguo" << key
                       << "em" << seen.value(key)->text()
                       << "e" << action->text();
        } else {
            seen.insert(key, action);
        }
    }
}

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

MainWindow::~MainWindow()
{
    // Docks emitem visibilityChanged ao serem destruídos/ocultados; as
    // lambdas capturam "this" cru e acessariam membros já destruídos
    // (UBSan: "mainwindow.cpp: member access ... does not point to an
    // object of type 'MainWindow'"). Desconectar aqui é o ponto seguro.
    for (QDockWidget* dock : {m_layersDock, m_textDock, m_shapeDock,
                              m_imageDock, m_historyDock}) {
        if (dock)
            dock->disconnect(this);
    }
    if (m_canvas)
        m_canvas->disconnect(this);
    if (m_startScreen)
        m_startScreen->disconnect(this);
}

MainWindow::MainWindow(SettingsService* settings, I18nService* i18n,
                       QWidget* parent, bool suppressStartupInteractions)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_i18n(i18n)
{
    // Allow compact laptop windows while keeping enough room for the canvas
    // and the docked properties panels.
    setMinimumSize(800, 520);
    resize(1280, 800);
    setWindowIcon(ThemeIcons::appIcon());

    const QString configDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    m_presetStore =
        std::make_unique<PresetStore>(configDir + QStringLiteral("/presets.json"));
    m_history = std::make_unique<CommandStack>(
        m_settings ? m_settings->historyLimit() : 100);
    // GC de assets: quando comandos saem da pilha (limite, redo invalidado,
    // clear), os bitmaps que SÓ eles sustentavam são removidos do store,
    // desde que nenhuma camada atual os referencie. Sem isto, cada filtro
    // destrutivo/pincel deixava uma cópia inteira da imagem gravada para
    // sempre no arquivo salvo.
    connect(m_history.get(), &CommandStack::assetsOrphaned, this,
            [this](const QSet<QUuid>& orphanedIds) {
                if (orphanedIds.isEmpty() || !m_document)
                    return;
                const QSet<LayerId> referenced = m_document->referencedAssetIds();
                for (const auto& id : orphanedIds) {
                    if (!referenced.contains(id))
                        m_document->assets().remove(id);
                }
            });
    if (m_settings) {
        connect(m_settings, &SettingsService::changed, this, [this] {
            if (m_settings && m_history)
                m_history->setLimit(m_settings->historyLimit());
        });
    }

    m_suppressStartupInteractions = suppressStartupInteractions;
    const QString recoveryPath =
        configDir + QStringLiteral("/autosave/recovery.creatorcanvas");
    if (!m_suppressStartupInteractions)
        m_autosave = std::make_unique<AutosaveService>(m_settings, recoveryPath, this);
    m_recentFiles = std::make_unique<RecentFiles>(
        configDir + QStringLiteral("/recent.json"));

    m_docController = std::make_unique<DocumentController>(this);
    if (m_autosave) {
        m_autosave->setDocumentController(m_docController.get());
        m_docController->registerObserver(m_autosave.get());
    }

    buildCentralWidget();
    buildLayersDock();

    m_docController->registerObserver(m_canvas);
    m_docController->registerObserver(m_layersPanel);
    m_docController->registerObserver(m_textInspector);
    m_docController->registerObserver(m_shapeInspector);
    m_docController->registerObserver(m_imageInspector);

    createDefaultDocument();

    connect(m_docController.get(), &DocumentController::documentModifiedChanged, this, [this](bool mod) {
        m_modified = mod;
        updateWindowTitle();
    });

    connectDocumentSignals();
    buildActions();
    buildToolBars();
    buildMenus();
    buildStatusBar();
    updateWindowTitle();

    connect(m_history.get(), &CommandStack::stateChanged, this, [this] {
        if (!m_modified) {
            m_modified = true;
            if (m_docController) m_docController->setModified(true);
            updateWindowTitle();
        }
    });

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &MainWindow::retranslateUi);
    retranslateUi();
    restoreWindowLayout();

    if (!m_suppressStartupInteractions)
        checkForRecoveryFile();
    else
        qInfo() << "[MainWindow] interações de startup suprimidas (modo automatizado)";
}

void MainWindow::restoreWindowLayout()
{
    if (m_suppressStartupInteractions || !m_settings)
        return;

    const QByteArray geometry = m_settings->windowGeometry();
    const QByteArray state = m_settings->windowState();

    bool restored = false;
    if (!geometry.isEmpty())
        restored = restoreGeometry(geometry);
    if (!state.isEmpty())
        restored = restoreState(state) || restored;

    // Restaurar apenas se o layout couber em alguma tela disponível.
    // Caso contrário (monitor removido, coordenadas inválidas, etc.) usamos
    // o layout padrão seguro do tamanho mínimo da aplicação.
    if (!restored || !windowFitsOnScreen()) {
        restoreGeometry(QByteArray());
        restoreState(QByteArray());
        resize(Design::EditorMinWidth, Design::EditorMinHeight);
    }
}

bool MainWindow::windowFitsOnScreen() const
{
    const QRect rect = geometry();
    if (rect.isNull() || rect.isEmpty())
        return false;
    for (QScreen* screen : QGuiApplication::screens()) {
        const QRect available = screen->availableGeometry();
        if (!available.intersected(rect).isEmpty())
            return true;
    }
    return false;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!confirmDiscardUnsavedChanges()) {
        event->ignore();
        return;
    }

    // Persistir o workspace do usuário (geometria + estado dos docks/
    // barras de ferramentas) para restaurar na próxima abertura. Nunca
    // gravamos em modo suprimido (testes/capturas automatizadas).
    if (!m_suppressStartupInteractions && m_settings) {
        m_settings->setWindowGeometry(saveGeometry());
        m_settings->setWindowState(saveState());
    }

    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    // At laptop widths keep the inspector usable, but give the canvas more
    // room than the desktop minimum dock width allows.
    const bool compact = width() < Design::BreakSM;
    // The application minimum window width is 800 px, so the XS breakpoint
    // is not reachable in practice. Treat the 800 px layout as compact too.
    const bool ultraCompact = width() <= Design::UltraCompactWidth;
    const int dock = Design::dockWidth(width());
    for (QDockWidget* panel : {m_layersDock, m_textDock, m_shapeDock,
                               m_imageDock, m_historyDock}) {
        if (panel)
            panel->setMinimumWidth(ultraCompact ? 180 : (compact ? 200 : dock));
    }

    // In the narrowest layout a tabified stack of five inspectors consumes
    // the canvas. Keep Layers as the single compact panel; the other panels
    // remain available from Window > Panels when the window is enlarged.
    if (ultraCompact) {
        if (m_layersDock)
            m_layersDock->setVisible(true);
        if (!m_docksAutoHidden) {
            // Guarda o estado escolhido pelo usuário para devolver ao
            // alargar a janela (antes os docks sumiam de vez e o usuário
            // tinha que reabrir tudo pelo menu Window > Panels).
            m_savedDockVisibility.clear();
            for (QDockWidget* panel : {m_textDock, m_shapeDock, m_imageDock,
                                       m_historyDock}) {
                // !isHidden() (e não isVisible()): durante a construção a
                // janela ainda não foi mostrada e isVisible() é sempre
                // false — guardar isso esconderia os docks para sempre.
                if (panel)
                    m_savedDockVisibility.append({panel, !panel->isHidden()});
            }
            m_docksAutoHidden = true;
        }
        for (QDockWidget* panel : {m_textDock, m_shapeDock, m_imageDock,
                                   m_historyDock}) {
            if (panel && panel->isVisible())
                panel->hide();
        }
    } else if (m_docksAutoHidden) {
        for (const auto& entry : m_savedDockVisibility) {
            if (entry.first)
                entry.first->setVisible(entry.second);
        }
        m_savedDockVisibility.clear();
        m_docksAutoHidden = false;
    }

    if (m_languageLabel)
        m_languageLabel->setVisible(!compact);
    if (m_versionLabel)
        m_versionLabel->setVisible(!compact);

    if (m_toolsBar) {
        // Em janelas pequenas a barra lateral some e uma barra horizontal
        // compacta (mesmas actions) aparece no topo. Antes a própria barra
        // era movida com addToolBarBreak a cada mudança de breakpoint, o
        // que acumulava quebras e deixava faixas vazias após redimensionar.
        // Também troca quando a ALTURA não comporta 14 ferramentas na
        // vertical (ex.: 1024x700 cortava as últimas e o usuário não via
        // gradiente/reparo).
        const bool narrow = width() < Design::BreakSM
                            || (height() < 760 && width() < 1300);
        const bool toolsEnabled = !m_toggleToolsBarAction
                                  || m_toggleToolsBarAction->isChecked();
        m_toolsBar->setVisible(!narrow && toolsEnabled);
        if (m_toolsBarCompact)
            m_toolsBarCompact->setVisible(narrow && toolsEnabled);
        // Ícone um pouco menor quando a altura é limitada, para as 14
        // ferramentas caberem sem corte/scroll.
        const int verticalIcon = height() < 800
                                     ? qBound(16, height() / 40, 20)
                                     : Design::iconSize(width());
        m_toolsBar->setIconSize(QSize(verticalIcon, verticalIcon));
        if (m_toolsBarCompact)
            m_toolsBarCompact->setIconSize(QSize(qBound(18, Design::iconSize(width()) - 2, 24),
                                                 qBound(18, Design::iconSize(width()) - 2, 24)));
    }
    if (m_quickToolBar) {
        const int quickIcon = qBound(18, Design::iconSize(width()), 24);
        m_quickToolBar->setIconSize(QSize(quickIcon, quickIcon));
    }

    applyToolOptionsCompact(width() < 1100);
    if (m_toolOptionsBar)
        m_toolOptionsBar->setVisible(width() >= Design::EditorMinWidth
                                     && m_toolOptionsBarWidget
                                     && m_toolOptionsBarWidget->currentToolIndex() != 0);
    // NOTA: o CanvasView cuida do próprio resize (re-ajusta em modo fit,
    // preserva o zoom manual). Antes um fitToViewport() aqui zerava o zoom
    // do usuário a cada redimensionamento de janela.
}

void MainWindow::attachDocument(std::unique_ptr<Document> doc, const QString& filePath)
{
    m_selectedId = LayerId();
    m_currentFilePath = filePath;
    m_modified = false;
    // Proveniência para o sidecar do autosave (de que projeto veio).
    if (m_autosave)
        m_autosave->setSourcePath(filePath);
    if (m_history)
        m_history->clear();

    if (m_docController) {
        // Ponto único central: notifica todos os observadores (Canvas, LayersPanel, todos os Inspetores, Autosave)
        m_docController->setDocument(std::move(doc), filePath);
        m_document = m_docController->currentDocument();
    }
    connectDocumentSignals();
}

void MainWindow::createDefaultDocument()
{
    NewDocumentSpec spec;
    spec.backgroundColor = QColor(Qt::white);
    auto doc = createDocument(spec);
    attachDocument(std::move(doc));
}

void MainWindow::connectDocumentSignals()
{
    if (!m_document || !m_canvas)
        return;
    connect(m_document, &Document::structureChanged,
            m_canvas, qOverload<>(&QWidget::update), Qt::UniqueConnection);
    // M2: undo/redo destrói camadas — sem esta poda, m_selectedId ficava
    // pendurado e deleteSelectedLayer crashava (A1) no comando seguinte.
    // (UniqueConnection não é permitido com lambda; attachDocument troca de
    // Document a cada chamada e as conexões antigas morrem com o objeto.)
    connect(m_document, &Document::structureChanged,
            this, [this]() {
                if (!m_selectedId.isNull()
                    && m_document && !m_document->findLayer(m_selectedId)) {
                    m_selectedId = LayerId();
                }
                if (m_canvas)
                    m_canvas->pruneSelection();
                updateSelectionMenuEnabled();
            });
    connect(m_document, &Document::layerPropertyChanged,
            m_canvas, qOverload<>(&QWidget::update), Qt::UniqueConnection);
    if (m_layersPanel) {
        connect(m_document, &Document::structureChanged,
                m_layersPanel, &LayersPanel::refresh, Qt::UniqueConnection);
        connect(m_document, &Document::layerPropertyChanged,
                m_layersPanel, &LayersPanel::refreshProperties, Qt::UniqueConnection);
    }
}

void MainWindow::buildCentralWidget()
{
    m_startScreen = new StartScreen(m_i18n, m_presetStore.get(),
                                    m_recentFiles.get(), this);
    m_canvas = new CanvasView(this);
    m_canvas->setI18n(m_i18n);
    m_canvas->setHistory(m_history.get());

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
                if (m_textBehindSubjectAction)
                    m_textBehindSubjectAction->setEnabled(false);
                const Layer* selected = m_document ? m_document->findLayer(id) : nullptr;
                const bool imageSelected = selected && selected->type() == LayerType::Image;
                if (m_removeBgAiAction) m_removeBgAiAction->setEnabled(imageSelected);
                if (m_removeBgAiQuickAction) m_removeBgAiQuickAction->setEnabled(imageSelected);
                if (m_mockupAction) m_mockupAction->setEnabled(imageSelected);
                bool canMergeDown = false;
                if (m_document && selected && selected->type() != LayerType::Background) {
                    GroupLayer* p = m_document->parentOf(id);
                    if (p) {
                        const int idx = m_document->indexOf(id);
                        if (idx > 0 && idx < static_cast<int>(p->children.size())) {
                            const Layer* below = p->children[static_cast<std::size_t>(idx - 1)].get();
                            canMergeDown = (below && below->type() != LayerType::Background);
                        }
                    }
                }
                if (m_mergeSelectedAction) m_mergeSelectedAction->setEnabled(canMergeDown);
                if (m_mergeVisibleAction) m_mergeVisibleAction->setEnabled(m_document != nullptr);
                if (m_flattenAction) m_flattenAction->setEnabled(m_document != nullptr);

                // Auto-ativar a aba do inspetor correspondente ao tipo de camada
                // selecionada — apenas em seleção ÚNICA. Em seleção múltipla
                // os tipos são mistos: pular de aba só confunde o utilizador.
                if (selected
                    && (!m_canvas || m_canvas->selectedLayers().size() <= 1)) {
                    switch (selected->type()) {
                    case LayerType::Text:
                        if (m_textDock && m_textDock->isVisible()) m_textDock->raise();
                        break;
                    case LayerType::Shape:
                        if (m_shapeDock && m_shapeDock->isVisible()) m_shapeDock->raise();
                        break;
                    case LayerType::Image:
                        if (m_imageDock && m_imageDock->isVisible()) m_imageDock->raise();
                        break;
                    default:
                        break;
                    }
                }
            });
    connect(m_canvas, &CanvasView::multiSelectionChanged, this,
            [this](const QList<LayerId>& ids) {
                if (m_layersPanel)
                    m_layersPanel->setSelectedLayers(ids);
                if (m_textBehindSubjectAction) {
                    int textCount = 0;
                    int imageCount = 0;
                    bool sameParent = ids.size() == 2;
                    GroupLayer* parent = nullptr;
                    if (m_document && sameParent) {
                        parent = m_document->parentOf(ids.first());
                        sameParent = parent && parent == m_document->parentOf(ids.last());
                    }
                    if (m_document && sameParent) {
                        for (const LayerId& id : ids) {
                            const Layer* layer = m_document->findLayer(id);
                            if (!layer) continue;
                            textCount += layer->type() == LayerType::Text;
                            imageCount += layer->type() == LayerType::Image;
                        }
                    }
                    m_textBehindSubjectAction->setEnabled(textCount == 1 && imageCount == 1);
                    if (m_mockupAction)
                        m_mockupAction->setEnabled(imageCount >= 2);

                    bool mergeable = ids.size() >= 2 && m_document;
                    GroupLayer* mergeParent = mergeable ? m_document->parentOf(ids.first()) : nullptr;
                    for (const LayerId& id : ids) {
                        const Layer* layer = m_document ? m_document->findLayer(id) : nullptr;
                        if (!layer || layer->type() == LayerType::Background
                            || m_document->parentOf(id) != mergeParent) {
                            mergeable = false;
                            break;
                        }
                    }
                    if (m_mergeSelectedAction)
                        m_mergeSelectedAction->setEnabled(mergeable);
                    if (m_mergeVisibleAction)
                        m_mergeVisibleAction->setEnabled(m_document != nullptr);
                    if (m_flattenAction)
                        m_flattenAction->setEnabled(m_document != nullptr);
                }
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
    connect(m_canvas, &CanvasView::statusMessageRequested, this,
            [this](const QString& message) {
                statusBar()->showMessage(message, 3000);
            });
    connect(m_canvas, &CanvasView::toolChanged, this,
            [this](CanvasTool tool) {
                if (m_toolOptionsBarWidget)
                    m_toolOptionsBarWidget->setCurrentTool(tool);
                if (m_toolOptionsBar)
                    m_toolOptionsBar->setVisible(tool != CanvasTool::Select);
                // Trocar de ferramenta NÃO deve mexer no zoom do canvas
                // (era um dos comportamentos mais confusos do app).
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
                    case CanvasTool::Eyedropper:
                        if (m_toolEyedropperAction) m_toolEyedropperAction->setChecked(true);
                        break;
                    case CanvasTool::Marquee:
                        if (m_toolMarqueeAction) m_toolMarqueeAction->setChecked(true);
                        break;
                    case CanvasTool::Gradient:
                        if (m_toolGradientAction) m_toolGradientAction->setChecked(true);
                        break;
                    case CanvasTool::Retouch:
                        if (m_toolRetouchAction) m_toolRetouchAction->setChecked(true);
                        break;
                    case CanvasTool::Healing:
                        if (m_toolHealingAction) m_toolHealingAction->setChecked(true);
                        break;
                    }
                }
            });

    connect(m_canvas, &CanvasView::colorPicked, this, [this](const QColor& color) {
        if (!color.isValid())
            return;
        if (m_toolOptionsBarWidget)
            m_toolOptionsBarWidget->setPaintColor(color);
        else if (m_canvas)
            m_canvas->setPaintColor(color);

        // If a TextLayer or ShapeLayer is selected, update its color
        if (m_document && !m_selectedId.isNull()) {
            if (auto* layer = m_document->findLayer(m_selectedId)) {
                if (layer->type() == LayerType::Text) {
                    if (m_history) {
                        m_history->execute(std::make_unique<SetTextColorCommand>(
                            *m_document, m_selectedId, QStringLiteral("text.color"),
                            &Document::setLayerTextColor, static_cast<TextLayer*>(layer)->color, color));
                    } else {
                        m_document->setLayerTextColor(m_selectedId, color);
                    }
                    if (m_textInspector) m_textInspector->refresh();
                } else if (layer->type() == LayerType::Shape) {
                    if (m_history) {
                        m_history->execute(std::make_unique<SetShapeFillCommand>(
                            *m_document, m_selectedId, QStringLiteral("shape.fill"),
                            &Document::setShapeFill, static_cast<ShapeLayer*>(layer)->fill, color));
                    } else {
                        m_document->setShapeFill(m_selectedId, color);
                    }
                } else if (layer->type() == LayerType::Image) {
                    if (m_imageInspector) {
                        // Passa a cor para o Chroma Key, mas NÃO troca de
                        // ferramenta sozinho: mudar de ferramenta sem o
                        // usuário pedir era desorientador.
                        m_imageInspector->setChromaKeyColor(color);
                    }
                }
            }
        }

        const QString msg = m_i18n
            ? m_i18n->t("editor", "canvas.status.colorPicked",
                        {color.name(), color.red(), color.green(), color.blue()})
            : QStringLiteral("Color picked: %1 (R:%2 G:%3 B:%4)")
                  .arg(color.name()).arg(color.red())
                  .arg(color.green()).arg(color.blue());
        statusBar()->showMessage(msg, 3000);
    });

    setCentralWidget(m_centralStack);
}

void MainWindow::buildLayersDock()
{
    m_layersPanel = new LayersPanel(m_i18n, m_history.get(), this);
    m_layersDock = new QDockWidget(QString(), this);
    m_layersDock->setObjectName(QStringLiteral("LayersDock"));
    m_layersDock->setMinimumWidth(Design::dockWidth(width()));
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
    connect(m_layersPanel, &LayersPanel::quickExportRequested,
            this, &MainWindow::quickExportLayerPng);
    connect(m_layersPanel, &LayersPanel::addTextRequested, this, [this] {
        if (m_addTextAction) m_addTextAction->trigger();
    });
    connect(m_layersPanel, &LayersPanel::addShapeRequested, this, [this] {
        if (m_addRectAction) m_addRectAction->trigger();
    });
    connect(m_layersPanel, &LayersPanel::mergeRequested, this, &MainWindow::mergeSelectedLayers);
    connect(m_layersPanel, &LayersPanel::flattenRequested, this, &MainWindow::flattenDocument);

    m_textInspector = new TextInspector(m_i18n, m_document, m_history.get(), this);
    m_textDock = new QDockWidget(QString(), this);
    m_textDock->setObjectName(QStringLiteral("TextDock"));
    m_textDock->setMinimumWidth(Design::dockWidth(width()));
    m_textDock->setWidget(m_textInspector);
    m_textDock->setFeatures(QDockWidget::DockWidgetMovable
                            | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, m_textDock);
    m_textDock->hide();

    // Cria o inspetor de formas e tabifica junto ao inspetor de texto na barra lateral direita
    m_shapeInspector = new ShapeInspector(m_i18n, m_document, m_history.get(), this);
    m_shapeDock = new QDockWidget(QString(), this);
    m_shapeDock->setObjectName(QStringLiteral("ShapeDock"));
    m_shapeDock->setMinimumWidth(Design::dockWidth(width()));
    m_shapeDock->setWidget(m_shapeInspector);
    m_shapeDock->setFeatures(QDockWidget::DockWidgetMovable
                             | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, m_shapeDock);
    m_shapeDock->hide();

    // Cria o inspetor de imagens e tabifica junto aos outros inspetores na barra lateral direita
    m_imageInspector = new ImageInspector(m_i18n, m_document, m_history.get(), this);
    m_imageDock = new QDockWidget(QString(), this);
    m_imageDock->setObjectName(QStringLiteral("ImageDock"));
    m_imageDock->setMinimumWidth(Design::dockWidth(width()));
    m_imageDock->setWidget(m_imageInspector);
    m_imageDock->setFeatures(QDockWidget::DockWidgetMovable
                             | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, m_imageDock);
    m_imageDock->hide();

    // Mockup: the inspector owns the logo/settings; CanvasView owns the
    // direct four-corner interaction on top of the artwork.
    connect(m_imageInspector, &ImageInspector::mockupPlacementRequested,
            m_canvas, &CanvasView::beginMockupPlacement);
    connect(m_imageInspector, &ImageInspector::eyedropperRequested, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::Eyedropper);
        if (m_toolEyedropperAction) m_toolEyedropperAction->setChecked(true);
        statusBar()->showMessage(QStringLiteral("Pipeta ativada: Clique sobre o fundo verde na imagem para capturar a cor exata."), 5000);
    });
    connect(m_imageInspector, &ImageInspector::imagePreviewReady,
            m_canvas, &CanvasView::setImagePreview);
    connect(m_imageInspector, &ImageInspector::imagePreviewCleared,
            m_canvas, &CanvasView::clearImagePreview);
    connect(m_canvas, &CanvasView::mockupAreaSelected,
            m_imageInspector, &ImageInspector::setMockupArea);
    connect(m_canvas, &CanvasView::mockupPlacementCancelled,
            m_imageInspector, &ImageInspector::clearPendingMockup);
    connect(m_imageInspector, &ImageInspector::layerSelectedChanged,
            m_canvas, &CanvasView::setSelectedLayer);

    // Cria o painel de Histórico (Undo/Redo visual estilo Photoshop)
    m_historyPanel = new HistoryPanel(m_i18n, m_history.get(), this);
    m_historyDock = new QDockWidget(QString(), this);
    m_historyDock->setObjectName(QStringLiteral("HistoryDock"));
    m_historyDock->setMinimumWidth(Design::dockWidth(width()));
    m_historyDock->setWidget(m_historyPanel);
    m_historyDock->setFeatures(QDockWidget::DockWidgetMovable
                              | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, m_historyDock);
    m_historyDock->hide();

    // Build one explicit tab group for all right-side panels.  Tabifying each
    // dock against the same anchor avoids Qt creating a second tab row when
    // some panels start hidden.
    tabifyDockWidget(m_layersDock, m_textDock);
    tabifyDockWidget(m_layersDock, m_shapeDock);
    tabifyDockWidget(m_layersDock, m_imageDock);
    tabifyDockWidget(m_layersDock, m_historyDock);
    setTabPosition(Qt::RightDockWidgetArea, QTabWidget::North);

    // Tabs de dock: scroll buttons + elide (tooltips nativos do Qt cobrem titulo truncado)
    for (QTabBar* tabs : findChildren<QTabBar*>()) {
        tabs->setUsesScrollButtons(true);
        tabs->setElideMode(Qt::ElideRight);
    }

    // O CanvasView cuida do próprio resize (re-ajusta em modo fit,
    // preserva o zoom manual). Antes um timer de 150 ms refazia o fit a
    // cada mudança de visibilidade de dock — isso zerava o zoom de quem
    // tinha dado zoom manual e disparava callbacks durante o teardown.

    connect(m_layersDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_toggleLayersDockAction && m_toggleLayersDockAction->isChecked() != visible)
            m_toggleLayersDockAction->setChecked(visible);
    });
    connect(m_textDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_toggleTextDockAction && m_toggleTextDockAction->isChecked() != visible)
            m_toggleTextDockAction->setChecked(visible);
    });
    connect(m_shapeDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_toggleShapeDockAction && m_toggleShapeDockAction->isChecked() != visible)
            m_toggleShapeDockAction->setChecked(visible);
    });
    connect(m_imageDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_toggleImageDockAction && m_toggleImageDockAction->isChecked() != visible)
            m_toggleImageDockAction->setChecked(visible);
    });
    connect(m_historyDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_toggleHistoryAction && m_toggleHistoryAction->isChecked() != visible) {
            m_toggleHistoryAction->setChecked(visible);
        }
    });
}

void MainWindow::buildActions()
{
    m_actions = new ActionManager(this);
    m_actions->createActions(m_i18n);

    m_newAction = m_actions->newAction;
    m_openAction = m_actions->openAction;
    m_saveAction = m_actions->saveAction;
    m_saveAsAction = m_actions->saveAsAction;
    m_importAction = m_actions->importAction;
    m_exportAction = m_actions->exportAction;
    m_quitAction = m_actions->quitAction;
    m_undoAction = m_actions->undoAction;
    m_redoAction = m_actions->redoAction;
    m_zoomInAction = m_actions->zoomInAction;
    m_zoomOutAction = m_actions->zoomOutAction;
    m_zoomFitAction = m_actions->zoomFitAction;
    m_toggleGridAction = m_actions->toggleGridAction;
    m_toggleSnapAction = m_actions->toggleSnapAction;
    m_toggleHistoryAction = m_actions->toggleHistoryAction;
    m_toggleToolsBarAction = m_actions->toggleToolsBarAction;
    m_toggleRightPanelAction = m_actions->toggleRightPanelAction;
    m_toggleLayersDockAction = m_actions->toggleLayersDockAction;
    m_toggleTextDockAction = m_actions->toggleTextDockAction;
    m_toggleShapeDockAction = m_actions->toggleShapeDockAction;
    m_toggleImageDockAction = m_actions->toggleImageDockAction;
    m_safeNoneAction = m_actions->safeNoneAction;
    m_safeYouTubeAction = m_actions->safeYouTubeAction;
    m_safeInstagramAction = m_actions->safeInstagramAction;
    m_safeTikTokAction = m_actions->safeTikTokAction;
    m_addTextAction = m_actions->addTextAction;
    m_addFillLayerAction = m_actions->addFillLayerAction;
    m_addAdjustmentLayerAction = m_actions->addAdjustmentLayerAction;
    m_addRectAction = m_actions->addRectAction;
    m_addRoundedRectAction = m_actions->addRoundedRectAction;
    m_addEllipseAction = m_actions->addEllipseAction;
    m_addLineAction = m_actions->addLineAction;
    m_addArrowRightAction = m_actions->addArrowRightAction;
    m_addArrowCurvedAction = m_actions->addArrowCurvedAction;
    m_addStarAction = m_actions->addStarAction;
    m_addBadgeAction = m_actions->addBadgeAction;
    m_addSpeechBubbleAction = m_actions->addSpeechBubbleAction;
    m_addHeartAction = m_actions->addHeartAction;
    m_addNewShapeActions = m_actions->addNewShapeActions;
    m_alignLeftAction = m_actions->alignLeftAction;
    m_alignCenterXAction = m_actions->alignCenterXAction;
    m_alignRightAction = m_actions->alignRightAction;
    m_alignTopAction = m_actions->alignTopAction;
    m_alignCenterYAction = m_actions->alignCenterYAction;
    m_alignBottomAction = m_actions->alignBottomAction;
    m_alignCenterBothAction = m_actions->alignCenterBothAction;
    m_distributeHAction = m_actions->distributeHAction;
    m_distributeVAction = m_actions->distributeVAction;
    m_groupAction = m_actions->groupAction;
    m_ungroupAction = m_actions->ungroupAction;
    m_mergeSelectedAction = m_actions->mergeSelectedAction;
    m_mergeVisibleAction = m_actions->mergeVisibleAction;
    m_flattenAction = m_actions->flattenAction;
    m_textBehindSubjectAction = m_actions->textBehindSubjectAction;
    m_flipHAction = m_actions->flipHAction;
    m_flipVAction = m_actions->flipVAction;
    m_rotateLeftAction = m_actions->rotateLeftAction;
    m_rotateRightAction = m_actions->rotateRightAction;
    m_deleteAction = m_actions->deleteAction;
    m_lockLayerAction = m_actions->lockLayerAction;
    m_selectAllAction = m_actions->selectAllAction;
    m_deselectAction = m_actions->deselectAction;
    m_invertSelectionAction = m_actions->invertSelectionAction;
    m_selectionOpsAction = m_actions->selectionOpsAction;
    m_colorRangeAction = m_actions->colorRangeAction;
    m_modifyActions = m_actions->modifyActions;
    m_toolGroup = m_actions->toolGroup;
    m_toolSelectAction = m_actions->toolSelectAction;
    m_toolCropAction = m_actions->toolCropAction;
    m_toolScissorsAction = m_actions->toolScissorsAction;
    m_toolWandAction = m_actions->toolWandAction;
    m_toolCloneAction = m_actions->toolCloneAction;
    m_toolPaintAction = m_actions->toolPaintAction;
    m_toolFloodAction = m_actions->toolFloodAction;
    m_toolEyedropperAction = m_actions->toolEyedropperAction;
    m_toolMarqueeAction = m_actions->toolMarqueeAction;
    m_toolGradientAction = m_actions->toolGradientAction;
    m_toolRetouchAction = m_actions->toolRetouchAction;
    m_toolHealingAction = m_actions->toolHealingAction;
    m_removeBgAiAction = m_actions->removeBgAiAction;
    m_removeBgAiQuickAction = m_actions->removeBgAiQuickAction;
    m_mockupAction = m_actions->mockupAction;
    m_filterActions = m_actions->filterActions;
    m_settingsAction = m_actions->settingsAction;
    m_startScreenAction = m_actions->startScreenAction;
    m_aboutAction = m_actions->aboutAction;
    m_aboutQtAction = m_actions->aboutQtAction;

    connectActionSignals();
}

void MainWindow::connectActionSignals()
{
    // --- Arquivo ---
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newDocument);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openDocument);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveDocument);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveDocumentAs);
    connect(m_importAction, &QAction::triggered, this, &MainWindow::importImageViaDialog);
    connect(m_exportAction, &QAction::triggered, this, &MainWindow::exportImage);
    connect(m_quitAction, &QAction::triggered, this, &MainWindow::close);

    // --- Edição & Histórico ---
    connect(m_undoAction, &QAction::triggered, this, [this] { if (m_history) m_history->undo(); });
    connect(m_redoAction, &QAction::triggered, this, [this] { if (m_history) m_history->redo(); });
    if (m_history) {
        connect(m_history.get(), &CommandStack::canUndoChanged, m_undoAction, &QAction::setEnabled);
        connect(m_history.get(), &CommandStack::canRedoChanged, m_redoAction, &QAction::setEnabled);
    }

    // --- Zoom & Visualização ---
    connect(m_zoomInAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->zoomIn(); });
    connect(m_zoomOutAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->zoomOut(); });
    connect(m_zoomFitAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->fitToViewport(); });
    connect(m_toggleGridAction, &QAction::triggered, this, [this](bool checked) {
        if (m_canvas) m_canvas->setShowGrid(checked);
    });
    connect(m_toggleSnapAction, &QAction::triggered, this, [this](bool checked) {
        if (m_canvas) m_canvas->setSnapToGrid(checked);
    });
    connect(m_toggleHistoryAction, &QAction::triggered, this, [this](bool checked) {
        if (m_historyDock) {
            m_historyDock->setVisible(checked);
            if (checked) m_historyDock->raise();
        }
    });
    connect(m_toggleToolsBarAction, &QAction::triggered, this, [this](bool checked) {
        const bool narrow = width() < Design::BreakSM;
        if (m_toolsBar) m_toolsBar->setVisible(checked && !narrow);
        if (m_toolsBarCompact) m_toolsBarCompact->setVisible(checked && narrow);
        if (m_toolOptionsBar)
            m_toolOptionsBar->setVisible(checked && m_toolOptionsBarWidget
                                         && m_toolOptionsBarWidget->currentToolIndex() != 0);
        QTimer::singleShot(150, this, [this] { if (m_canvas) m_canvas->fitToViewport(); });
    });
    connect(m_toggleRightPanelAction, &QAction::triggered, this, [this](bool checked) {
        const QList<QDockWidget*> panels = {m_layersDock, m_textDock, m_shapeDock,
                                            m_imageDock, m_historyDock};
        for (auto* panel : panels)
            if (panel) panel->setVisible(checked);
        QTimer::singleShot(150, this, [this] { if (m_canvas) m_canvas->fitToViewport(); });
    });
    connect(m_toggleLayersDockAction, &QAction::triggered, this, [this](bool checked) {
        if (m_layersDock) {
            m_layersDock->setVisible(checked);
            if (checked) m_layersDock->raise();
        }
    });
    connect(m_toggleTextDockAction, &QAction::triggered, this, [this](bool checked) {
        if (m_textDock) {
            m_textDock->setVisible(checked);
            if (checked) m_textDock->raise();
        }
    });
    connect(m_toggleShapeDockAction, &QAction::triggered, this, [this](bool checked) {
        if (m_shapeDock) {
            m_shapeDock->setVisible(checked);
            if (checked) m_shapeDock->raise();
        }
    });
    connect(m_toggleImageDockAction, &QAction::triggered, this, [this](bool checked) {
        if (m_imageDock) {
            m_imageDock->setVisible(checked);
            if (checked) m_imageDock->raise();
        }
    });

    // --- Zonas Seguras ---
    connect(m_safeNoneAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setSafeZoneMode(0); });
    connect(m_safeYouTubeAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setSafeZoneMode(1); });
    connect(m_safeInstagramAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setSafeZoneMode(2); });
    connect(m_safeTikTokAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setSafeZoneMode(3); });

    // --- Camadas: Criação ---
    connect(m_addTextAction, &QAction::triggered, this, &MainWindow::addText);
    connect(m_addRectAction, &QAction::triggered, this, [this] { addShape(ShapeKind::Rectangle); });
    connect(m_addRoundedRectAction, &QAction::triggered, this, [this] { addShape(ShapeKind::RoundedRect); });
    connect(m_addEllipseAction, &QAction::triggered, this, [this] { addShape(ShapeKind::Ellipse); });
    connect(m_addLineAction, &QAction::triggered, this, [this] { addShape(ShapeKind::Line); });
    connect(m_addArrowRightAction, &QAction::triggered, this, [this] { addShape(ShapeKind::ArrowRight); });
    connect(m_addArrowCurvedAction, &QAction::triggered, this, [this] { addShape(ShapeKind::ArrowCurved); });
    connect(m_addStarAction, &QAction::triggered, this, [this] { addShape(ShapeKind::Star); });
    connect(m_addBadgeAction, &QAction::triggered, this, [this] { addShape(ShapeKind::Badge); });
    connect(m_addSpeechBubbleAction, &QAction::triggered, this, [this] { addShape(ShapeKind::SpeechBubble); });
    connect(m_addHeartAction, &QAction::triggered, this, [this] { addShape(ShapeKind::Heart); });

    for (int i = 0; i < m_addNewShapeActions.size() && i < kNewShapeCount; ++i) {
        const ShapeKind kind = kNewShapeDefs[i].kind;
        connect(m_addNewShapeActions[i], &QAction::triggered, this, [this, kind] { addShape(kind); });
    }

    // Camada de Preenchimento
    connect(m_addFillLayerAction, &QAction::triggered, this, [this] {
        if (!m_document) return;
        const QColor chosen = QColorDialog::getColor(
            QColor(61, 126, 255), this,
            m_i18n ? m_i18n->t("editor", "layer.newFillLayer.colorTitle")
                   : QStringLiteral("Fill Layer Color"));
        if (!chosen.isValid()) return;
        auto layer = std::make_unique<FillLayer>();
        layer->name = m_i18n ? m_i18n->t("editor", "layer.newFillLayer.name")
                             : QStringLiteral("Fill Layer");
        layer->kind = FillType::Solid;
        layer->color = chosen;
        if (m_history) {
            m_history->execute(std::make_unique<AddLayerCommand>(
                *m_document, std::move(layer), m_document->rootGroup(), -1));
        } else {
            m_document->addLayer(std::move(layer));
        }
    });

    // Camada de Ajuste
    connect(m_addAdjustmentLayerAction, &QAction::triggered, this, [this] {
        if (!m_document) return;
        QMenu menu(this);
        static const struct { AdjustmentType type; const char* key; const char* fallback; } kAdjustKinds[] = {
            { AdjustmentType::BrightnessContrast, "layer.adjust.brightnessContrast", "Brightness / Contrast" },
            { AdjustmentType::HueSaturation,      "layer.adjust.hueSaturation",      "Hue / Saturation" },
            { AdjustmentType::ColorBalance,       "layer.adjust.colorBalance",       "Color Balance" },
            { AdjustmentType::PhotoFilter,        "layer.adjust.photoFilter",        "Photo Filter" },
            { AdjustmentType::Invert,             "layer.adjust.invert",             "Invert" },
            { AdjustmentType::Posterize,          "layer.adjust.posterize",          "Posterize" },
            { AdjustmentType::Threshold,          "layer.adjust.threshold",          "Threshold" },
            { AdjustmentType::GradientMap,        "layer.adjust.gradientMap",        "Gradient Map" },
        };
        for (const auto& def : kAdjustKinds) {
            QAction* act = menu.addAction(m_i18n
                ? m_i18n->t("editor", QString::fromLatin1(def.key))
                : QString::fromLatin1(def.fallback));
            connect(act, &QAction::triggered, this, [this, def] {
                auto layer = std::make_unique<AdjustmentLayer>();
                layer->name = m_i18n
                    ? m_i18n->t("editor", QString::fromLatin1(def.key))
                    : QString::fromLatin1(def.fallback);
                layer->kind = def.type;
                switch (def.type) {
                case AdjustmentType::BrightnessContrast:
                    layer->parameters = {{QStringLiteral("brightness"), 10.0},
                                         {QStringLiteral("contrast"), 10.0}};
                    break;
                case AdjustmentType::HueSaturation:
                    layer->parameters = {{QStringLiteral("hue"), 0.0},
                                         {QStringLiteral("saturation"), 0.0}};
                    break;
                case AdjustmentType::PhotoFilter:
                case AdjustmentType::ColorBalance:
                    layer->parameters = {{QStringLiteral("temperature"), 20.0}};
                    break;
                default:
                    break;
                }
                if (m_history) {
                    m_history->execute(std::make_unique<AddLayerCommand>(
                        *m_document, std::move(layer), m_document->rootGroup(), -1));
                } else {
                    m_document->addLayer(std::move(layer));
                }
            });
        }
        menu.exec(QCursor::pos());
    });

    // --- Camadas: Alinhamento & Distribuição ---
    connect(m_alignLeftAction, &QAction::triggered, this, [this] { alignSelectedLayer(AlignTarget::Left); });
    connect(m_alignCenterXAction, &QAction::triggered, this, [this] { alignSelectedLayer(AlignTarget::CenterX); });
    connect(m_alignRightAction, &QAction::triggered, this, [this] { alignSelectedLayer(AlignTarget::Right); });
    connect(m_alignTopAction, &QAction::triggered, this, [this] { alignSelectedLayer(AlignTarget::Top); });
    connect(m_alignCenterYAction, &QAction::triggered, this, [this] { alignSelectedLayer(AlignTarget::CenterY); });
    connect(m_alignBottomAction, &QAction::triggered, this, [this] { alignSelectedLayer(AlignTarget::Bottom); });
    connect(m_alignCenterBothAction, &QAction::triggered, this, [this] { alignSelectedLayer(AlignTarget::CenterBoth); });
    connect(m_distributeHAction, &QAction::triggered, this, &MainWindow::distributeHorizontally);
    connect(m_distributeVAction, &QAction::triggered, this, &MainWindow::distributeVertically);

    // --- Camadas: Operações ---
    connect(m_groupAction, &QAction::triggered, this, &MainWindow::groupSelectedLayers);
    connect(m_ungroupAction, &QAction::triggered, this, &MainWindow::ungroupSelectedLayer);
    connect(m_mergeSelectedAction, &QAction::triggered, this, &MainWindow::mergeSelectedLayers);
    connect(m_mergeVisibleAction, &QAction::triggered, this, &MainWindow::mergeVisibleLayers);
    connect(m_flattenAction, &QAction::triggered, this, &MainWindow::flattenDocument);
    connect(m_textBehindSubjectAction, &QAction::triggered, this, &MainWindow::textBehindSubject);
    connect(m_flipHAction, &QAction::triggered, this, [this] { flipLayer(true); });
    connect(m_flipVAction, &QAction::triggered, this, [this] { flipLayer(false); });
    connect(m_rotateLeftAction, &QAction::triggered, this, [this] { rotateLayer(-90.0); });
    connect(m_rotateRightAction, &QAction::triggered, this, [this] { rotateLayer(90.0); });
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::deleteSelectedLayer);
    connect(m_lockLayerAction, &QAction::triggered, this, [this] {
        if (!m_document || m_selectedId.isNull()) return;
        Layer* l = m_document->findLayer(m_selectedId);
        if (!l) return;
        const bool current = l->locked || l->locks.all;
        const bool newLock = !current;
        if (m_history) {
            m_history->execute(std::make_unique<LayerLockCommand>(
                *m_document, m_selectedId, QStringLiteral("layer.lock"),
                &Document::setLayerLocked, current, newLock));
        } else {
            m_document->setLayerLocked(m_selectedId, newLock);
        }
    });

    // --- Seleção de Pixels ---
    connect(m_selectAllAction, &QAction::triggered, this, [this] {
        if (m_document) {
            QImage full(m_document->width(), m_document->height(), QImage::Format_Alpha8);
            full.fill(255);
            executeSetPixelSelection(*m_document, m_history.get(), full,
                                     QStringLiteral("selection.selectAll"));
        }
    });
    connect(m_deselectAction, &QAction::triggered, this, [this] {
        if (m_document && !m_document->pixelSelection().isNull())
            executeSetPixelSelection(*m_document, m_history.get(), QImage(),
                                     QStringLiteral("selection.deselect"));
    });
    connect(m_invertSelectionAction, &QAction::triggered, this, [this] {
        if (!m_document || m_document->pixelSelection().isNull()) return;
        QImage mask = m_document->pixelSelection().convertToFormat(QImage::Format_Alpha8);
        for (int y = 0; y < mask.height(); ++y) {
            uchar* row = mask.scanLine(y);
            for (int x = 0; x < mask.width(); ++x) row[x] = 255 - row[x];
        }
        executeSetPixelSelection(*m_document, m_history.get(), mask,
                                 QStringLiteral("selection.invert"));
    });
    connect(m_selectionOpsAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->openSelectionActionsMenu();
    });

    auto modifySelection = [this](const std::function<QImage(const QImage&)>& op) {
        if (!m_document || m_document->pixelSelection().isNull())
            return;
        const QImage current = m_document->pixelSelection();
        const QImage modified = op(current);
        executeSetPixelSelection(*m_document, m_history.get(), modified,
                                 QStringLiteral("selection.modify"));
    };
    auto askRadius = [this](const QString& title, double def) -> double {
        bool ok = false;
        const double v = QInputDialog::getDouble(
            this, title,
            m_i18n ? m_i18n->t("editor", "selection.modify.radius")
                   : QStringLiteral("Radius (px):"),
            def, 1.0, 200.0, 1, &ok);
        return ok ? v : -1.0;
    };
    if (m_modifyActions.size() >= 5) {
        connect(m_modifyActions[0], &QAction::triggered, this, [modifySelection, askRadius, this] {
            const double r = askRadius(
                m_i18n ? m_i18n->t("editor", "selection.modify.expand")
                       : QStringLiteral("Expand Selection"), 2.0);
            if (r > 0)
                modifySelection([r](const QImage& s) { return SelectionEngine::expand(s, float(r)); });
        });
        connect(m_modifyActions[1], &QAction::triggered, this, [modifySelection, askRadius, this] {
            const double r = askRadius(
                m_i18n ? m_i18n->t("editor", "selection.modify.contract")
                       : QStringLiteral("Contract Selection"), 2.0);
            if (r > 0)
                modifySelection([r](const QImage& s) { return SelectionEngine::contract(s, float(r)); });
        });
        connect(m_modifyActions[2], &QAction::triggered, this, [modifySelection, askRadius, this] {
            const double r = askRadius(
                m_i18n ? m_i18n->t("editor", "selection.modify.border")
                       : QStringLiteral("Border Selection"), 4.0);
            if (r > 0)
                modifySelection([r](const QImage& s) { return SelectionEngine::border(s, float(r)); });
        });
        connect(m_modifyActions[3], &QAction::triggered, this, [modifySelection, askRadius, this] {
            const double r = askRadius(
                m_i18n ? m_i18n->t("editor", "selection.modify.smooth")
                       : QStringLiteral("Smooth Selection"), 2.0);
            if (r > 0)
                modifySelection([r](const QImage& s) { return SelectionEngine::smooth(s, float(r)); });
        });
        connect(m_modifyActions[4], &QAction::triggered, this, [modifySelection, askRadius, this] {
            const double r = askRadius(
                m_i18n ? m_i18n->t("editor", "selection.modify.feather")
                       : QStringLiteral("Feather Selection"), 5.0);
            if (r > 0)
                modifySelection([r](const QImage& s) { return SelectionEngine::feather(s, float(r)); });
        });
    }

    connect(m_colorRangeAction, &QAction::triggered, this, [this] {
        if (!m_document || m_selectedId.isNull())
            return;
        Layer* layer = m_document->findLayer(m_selectedId);
        if (!layer || layer->type() != LayerType::Image)
            return;

        bool ok = false;
        const double fuzziness = QInputDialog::getDouble(
            this,
            m_i18n ? m_i18n->t("editor", "selection.colorRange.title")
                   : QStringLiteral("Color Range"),
            m_i18n ? m_i18n->t("editor", "selection.colorRange.fuzziness")
                   : QStringLiteral("Fuzziness:"),
            40.0, 0.0, 200.0, 1, &ok);
        if (!ok)
            return;

        const QColor target = m_toolOptionsBarWidget
                                  ? m_toolOptionsBarWidget->currentPaintColor()
                                  : QColor(230, 50, 50);
        const QImage bitmap = m_document->assets().decodedImage(
            static_cast<ImageLayer*>(layer)->assetId);
        if (bitmap.isNull())
            return;

        const QImage mask = SelectionEngine::colorRange(
            bitmap, bitmap.rect(), target, float(fuzziness));
        executeSetPixelSelection(*m_document, m_history.get(), mask,
                                 QStringLiteral("selection.colorRange"));
    });

    // --- Ferramentas da Barra ---
    connect(m_toolSelectAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setTool(CanvasTool::Select); });
    connect(m_toolCropAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setTool(CanvasTool::Crop); });
    connect(m_toolScissorsAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setTool(CanvasTool::Scissors); });
    connect(m_toolWandAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setTool(CanvasTool::MagicWand); });
    connect(m_toolCloneAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setTool(CanvasTool::CloneStamp); });
    connect(m_toolPaintAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setTool(CanvasTool::Paint); });
    connect(m_toolFloodAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setTool(CanvasTool::FloodFill); });
    connect(m_toolEyedropperAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setTool(CanvasTool::Eyedropper); });
    connect(m_toolMarqueeAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setTool(CanvasTool::Marquee); });
    connect(m_toolGradientAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setTool(CanvasTool::Gradient); });
    connect(m_toolRetouchAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setTool(CanvasTool::Retouch); });
    connect(m_toolHealingAction, &QAction::triggered, this, [this] { if (m_canvas) m_canvas->setTool(CanvasTool::Healing); });

    // --- IA & Filtros ---
    connect(m_removeBgAiAction, &QAction::triggered, this, [this] {
        if (m_canvas && m_document && !m_selectedId.isNull())
            m_canvas->openAiBackgroundRemoval(m_selectedId);
    });
    connect(m_removeBgAiQuickAction, &QAction::triggered, this, [this] {
        if (m_canvas && m_document && !m_selectedId.isNull())
            m_canvas->removeBackgroundAiQuick(m_selectedId);
    });
    connect(m_mockupAction, &QAction::triggered, this, [this] {
        if (m_imageInspector) m_imageInspector->openMockupDialog();
    });

    for (int i = 0; i < m_filterActions.size(); ++i) {
        connect(m_filterActions[i], &QAction::triggered, this, [this, i] {
            if (m_imageInspector) {
                m_imageInspector->applyPreset(i);
                if (m_imageDock && m_imageDock->isVisible())
                    m_imageDock->raise();
            }
        });
    }

    // --- Janela, Ajuda & Preferências ---
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::openSettings);
    connect(m_startScreenAction, &QAction::triggered, this, [this] {
        if (confirmDiscardUnsavedChanges())
            showStartScreen();
    });
    connect(m_aboutAction, &QAction::triggered, this, [this] {
        if (!m_i18n) return;
        QMessageBox::about(
            this,
            m_i18n->t("common", "menu.help.about"),
            QStringLiteral("<h3>CreatorCanvas %1</h3><p>%2</p>")
                .arg(QApplication::applicationVersion(),
                     m_i18n->t("common", "app.aboutBody").toHtmlEscaped()));
    });
    connect(m_aboutQtAction, &QAction::triggered, this, [this] {
        QMessageBox::aboutQt(this);
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
    m_toolsBar->setIconSize(QSize(22, 22));
    addToolBar(Qt::LeftToolBarArea, m_toolsBar);

    m_toolSelectAction->setIcon(ThemeIcons::toolSelect());
    m_toolMarqueeAction->setIcon(ThemeIcons::toolMarquee());
    m_toolCropAction->setIcon(ThemeIcons::toolCrop());
    m_toolScissorsAction->setIcon(ThemeIcons::toolScissors());
    m_toolWandAction->setIcon(ThemeIcons::toolWand());
    m_toolCloneAction->setIcon(ThemeIcons::toolClone());
    m_toolRetouchAction->setIcon(ThemeIcons::toolRetouch());
    m_toolHealingAction->setIcon(ThemeIcons::toolHealing());
    m_toolPaintAction->setIcon(ThemeIcons::toolPaint());
    m_toolGradientAction->setIcon(ThemeIcons::toolGradient());
    m_toolFloodAction->setIcon(ThemeIcons::toolFlood());
    m_toolEyedropperAction->setIcon(ThemeIcons::toolEyedropper());
    m_addTextAction->setIcon(ThemeIcons::toolText());
    m_addRectAction->setIcon(ThemeIcons::toolShape());

    // Agrupadas por função, com separadores — 12 botões idênticos em fila
    // sem agrupamento era o principal ponto de confusão da barra.
    m_toolsBar->addAction(m_toolSelectAction);   // seleção/mover
    m_toolsBar->addAction(m_toolMarqueeAction);  // seleção retangular/elíptica
    m_toolsBar->addAction(m_toolScissorsAction); // seleção à mão livre
    m_toolsBar->addAction(m_toolWandAction);     // seleção por cor
    m_toolsBar->addSeparator();
    m_toolsBar->addAction(m_toolCropAction);     // corte
    m_toolsBar->addSeparator();
    m_toolsBar->addAction(m_toolPaintAction);    // pintura
    m_toolsBar->addAction(m_toolGradientAction); // gradiente
    m_toolsBar->addAction(m_toolFloodAction);    // balde
    m_toolsBar->addSeparator();
    m_toolsBar->addAction(m_toolCloneAction);    // retoque
    m_toolsBar->addAction(m_toolRetouchAction);
    m_toolsBar->addAction(m_toolHealingAction);
    m_toolsBar->addSeparator();
    m_toolsBar->addAction(m_toolEyedropperAction); // util
    m_toolsBar->addSeparator();
    m_toolsBar->addAction(m_addTextAction);        // criação
    m_toolsBar->addAction(m_addRectAction);

    // Barra Superior de Ações Rápidas (Novo, Abrir, Salvar, Exportar, Desfazer, Zoom, Grade)
    m_quickToolBar = new QToolBar(QStringLiteral("QuickAccess"), this);
    m_quickToolBar->setObjectName(QStringLiteral("QuickAccessToolBar"));
    m_quickToolBar->setMovable(false);
    m_quickToolBar->setFloatable(false);
    m_quickToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_quickToolBar->setIconSize(QSize(20, 20));
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
    // Barra compacta (horizontal) para janelas estreitas: as MESMAS actions,
    // então o estado "checked" fica sincronizado automaticamente.
    m_toolsBarCompact = new QToolBar(QStringLiteral("ToolsCompact"), this);
    m_toolsBarCompact->setObjectName(QStringLiteral("ToolsCompactToolBar"));
    m_toolsBarCompact->setMovable(false);
    m_toolsBarCompact->setFloatable(false);
    m_toolsBarCompact->setOrientation(Qt::Horizontal);
    m_toolsBarCompact->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolsBarCompact->setIconSize(QSize(20, 20));
    for (QAction* a : {m_toolSelectAction, m_toolMarqueeAction,
                       m_toolScissorsAction, m_toolWandAction,
                       m_toolCropAction, m_toolPaintAction,
                       m_toolGradientAction, m_toolFloodAction,
                       m_toolCloneAction, m_toolRetouchAction,
                       m_toolHealingAction, m_toolEyedropperAction,
                       static_cast<QAction*>(nullptr), m_addTextAction,
                       m_addRectAction}) {
        if (!a)
            m_toolsBarCompact->addSeparator();
        else
            m_toolsBarCompact->addAction(a);
    }
    // Compartilha a linha do QuickAccess: em janelas estreitas as duas
    // cabem juntas (≈330px + ≈360px) e isso evita uma terceira faixa de
    // chrome vertical. A barra de opções ganha a linha de baixo.
    addToolBar(Qt::TopToolBarArea, m_toolsBarCompact);
    m_toolsBarCompact->hide();

    m_toolOptionsBar = new QToolBar(QStringLiteral("ToolOptions"), this);
    m_toolOptionsBar->setObjectName(QStringLiteral("ToolOptionsBar"));
    m_toolOptionsBar->setMovable(false);
    m_toolOptionsBar->setFloatable(false);
    // Linha própria para as opções: sem esta quebra, em janelas estreitas a
    // barra de opções era anexada ao fim da barra compacta e o conteúdo
    // (slider/valor) ficava cortado na borda direita.
    addToolBarBreak(Qt::TopToolBarArea);
    m_toolOptionsBarWidget = new ToolOptionsBar(this);
    m_toolOptionsBarWidget->setCanvas(m_canvas);
    m_toolOptionsBarWidget->setI18n(m_i18n);
    m_toolOptionsBar->addWidget(m_toolOptionsBarWidget);

    m_quickToolBar->hide();
    m_toolsBar->hide();
    m_toolOptionsBar->hide();
}

void MainWindow::buildMenus()
{
    m_menuBuilder = new MenuBuilder(menuBar(), m_actions, this);
    m_menuBuilder->buildMenus(m_i18n);

    m_fileMenu = m_menuBuilder->fileMenu;
    m_openRecentMenu = m_menuBuilder->openRecentMenu;
    m_editMenu = m_menuBuilder->editMenu;
    m_imageMenu = m_menuBuilder->imageMenu;
    m_filterMenu = m_menuBuilder->filterMenu;
    m_artisticMenu = m_menuBuilder->artisticMenu;
    m_toneMenu = m_menuBuilder->toneMenu;
    m_selectMenu = m_menuBuilder->selectMenu;
    m_modifyMenu = m_menuBuilder->modifyMenu;
    m_layerMenu = m_menuBuilder->layerMenu;
    m_addShapeMenu = m_menuBuilder->addShapeMenu;
    m_alignMenu = m_menuBuilder->alignMenu;
    m_viewMenu = m_menuBuilder->viewMenu;
    m_safeZoneMenu = m_menuBuilder->safeZoneMenu;
    m_panelsMenu = m_menuBuilder->panelsMenu;
    m_toolsMenu = m_menuBuilder->toolsMenu;
    m_selectionToolsMenu = m_menuBuilder->selectionToolsMenu;
    m_retouchToolsMenu = m_menuBuilder->retouchToolsMenu;
    m_createToolsMenu = m_menuBuilder->createToolsMenu;
    m_windowMenu = m_menuBuilder->windowMenu;
    m_settingsMenu = m_menuBuilder->settingsMenu;
    m_helpMenu = m_menuBuilder->helpMenu;

    if (m_openRecentMenu) {
        connect(m_openRecentMenu, &QMenu::aboutToShow, this, [this] { refreshRecentMenu(); });
    }

    warnDuplicateShortcuts(this);
}

void MainWindow::createDemoDocumentForSnapshot()
{
    createDefaultDocument();
    enterEditor();
}

void MainWindow::selectToolByName(const QString& name)
{
    if (m_actions)
        m_actions->selectToolByName(name);
}

void MainWindow::applyToolOptionsCompact(bool compact)
{
    if (m_toolOptionsBarWidget)
        m_toolOptionsBarWidget->applyCompact(compact);
}

void MainWindow::updateSelectionMenuEnabled()
{
    // Menu Selecionar: Modify/Color Range só fazem sentido com uma seleção
    // ativa (antes ficavam habilitados e eram no-op silencioso — confuso).
    const bool hasSelection = m_document && !m_document->pixelSelection().isNull();
    const bool hasImageLayer = m_selectedId.isNull() ? false : [&] {
        Layer* l = m_document ? m_document->findLayer(m_selectedId) : nullptr;
        return l && l->type() == LayerType::Image;
    }();
    for (QAction* act : m_modifyActions) {
        if (act)
            act->setEnabled(hasSelection);
    }
    if (m_deselectAction)
        m_deselectAction->setEnabled(hasSelection);
    if (m_invertSelectionAction)
        m_invertSelectionAction->setEnabled(hasSelection);
    if (m_selectionOpsAction)
        m_selectionOpsAction->setEnabled(hasSelection);
    if (m_colorRangeAction)
        m_colorRangeAction->setEnabled(hasImageLayer);
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
    if (m_historyDock) m_historyDock->hide();
    if (m_quickToolBar) m_quickToolBar->hide();
    if (m_toolsBar) m_toolsBar->hide();
    if (m_toolsBarCompact) m_toolsBarCompact->hide();
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
    if (m_historyDock) m_historyDock->show();
    if (m_toggleHistoryAction) m_toggleHistoryAction->setChecked(true);
    if (m_quickToolBar) m_quickToolBar->show();
    // Barra vertical ou compacta conforme a largura atual.
    const bool narrow = width() < Design::BreakSM;
    if (m_toolsBar) m_toolsBar->setVisible(!narrow);
    if (m_toolsBarCompact) m_toolsBarCompact->setVisible(narrow);
    // Selection has no contextual controls, so keep this row hidden until a
    // tool that needs options is selected.
    if (m_toolOptionsBar) m_toolOptionsBar->hide();
    updateWindowTitle();
}

void MainWindow::startFromPreset(const NewDocumentSpec& spec)
{
    if (!confirmDiscardUnsavedChanges())
        return;

    auto doc = createDocument(spec);
    attachDocument(std::move(doc));
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

    if (m_autosave) {
        m_autosave->discardRecovery();
    }
    attachDocument(std::move(loaded), path);
    if (m_recentFiles)
        m_recentFiles->push(path);
    enterEditor();
    updateWindowTitle();
}

void MainWindow::refreshRecentMenu()
{
    if (!m_openRecentMenu || !m_recentFiles)
        return;
    m_openRecentMenu->clear();
    const QStringList paths = m_recentFiles->paths();
    if (paths.isEmpty()) {
        QAction* empty = m_openRecentMenu->addAction(QStringLiteral("—"));
        empty->setEnabled(false);
        return;
    }
    for (const QString& path : paths) {
        QAction* act = m_openRecentMenu->addAction(QFileInfo(path).fileName());
        act->setToolTip(path);
        act->setStatusTip(path);
        connect(act, &QAction::triggered, this, [this, path] {
            openFromPath(path);
        });
    }
    m_openRecentMenu->addSeparator();
    QAction* clearAct = m_openRecentMenu->addAction(
        m_i18n ? m_i18n->t("common", "menu.file.clearRecents")
               : QStringLiteral("Clear Recent List"));
    connect(clearAct, &QAction::triggered, this, [this] {
        m_recentFiles->clear();
        if (m_startScreen)
            m_startScreen->refreshRecents();
    });
}

void MainWindow::newDocument()
{
    if (!confirmDiscardUnsavedChanges())
        return;

    NewDocumentDialog dialog(m_i18n, m_presetStore.get(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    if (m_autosave) {
        m_autosave->discardRecovery();
    }
    auto doc = createDocument(dialog.spec());
    attachDocument(std::move(doc));
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
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    const auto result = importImageFromFile(filePath);
    QGuiApplication::restoreOverrideCursor();
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

    if (!m_document) {
        if (m_autosave)
            m_autosave->discardRecovery();
        auto doc = std::make_unique<Document>(result.image.width(), result.image.height());
        attachDocument(std::move(doc));
        updateWindowTitle();
        enterEditor();
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
    // Importa a imagem inteira dentro da área de trabalho, preservando a
    // proporção. Imagens pequenas não são ampliadas automaticamente.
    const double margin = 0.92;
    const double fitScale = std::min(
        (m_document->width() * margin) / std::max(1, result.image.width()),
        (m_document->height() * margin) / std::max(1, result.image.height()));
    const double scale = std::min(1.0, fitScale);
    layer->transform.scaleX = scale;
    layer->transform.scaleY = scale;
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
    case ShapeKind::ArrowRight: {
        layer->name = m_i18n->t("common", "menu.layer.shape.arrowRight").remove('&');
        const double w = std::clamp(docW * 0.25, 120.0, 360.0);
        const double h = w * 0.5;
        layer->points = QPolygonF{
            QPointF(0, 0), QPointF(w, 0), QPointF(w, h), QPointF(0, h)
        };
        layer->fill = QColor(255, 68, 68); // Vermelho vibrante de thumbnail
        layer->stroke = Qt::white;
        layer->strokeWidth = 3.0;
        break;
    }
    case ShapeKind::ArrowCurved: {
        layer->name = m_i18n->t("common", "menu.layer.shape.arrowCurved").remove('&');
        const double w = std::clamp(docW * 0.22, 120.0, 320.0);
        const double h = w * 0.8;
        layer->points = QPolygonF{
            QPointF(0, 0), QPointF(w, 0), QPointF(w, h), QPointF(0, h)
        };
        layer->fill = QColor(255, 204, 0); // Amarelo vibrante
        layer->stroke = QColor(30, 30, 30);
        layer->strokeWidth = 3.0;
        break;
    }
    case ShapeKind::Star: {
        layer->name = m_i18n->t("common", "menu.layer.shape.star").remove('&');
        const double size = std::clamp(std::min(docW, docH) * 0.22, 100.0, 260.0);
        layer->points = QPolygonF{
            QPointF(0, 0), QPointF(size, 0), QPointF(size, size), QPointF(0, size)
        };
        layer->fill = QColor(255, 215, 0); // Dourado
        layer->stroke = QColor(180, 100, 0);
        layer->strokeWidth = 2.0;
        break;
    }
    case ShapeKind::Badge: {
        layer->name = m_i18n->t("common", "menu.layer.shape.badge").remove('&');
        const double size = std::clamp(std::min(docW, docH) * 0.22, 110.0, 260.0);
        layer->points = QPolygonF{
            QPointF(0, 0), QPointF(size, 0), QPointF(size, size), QPointF(0, size)
        };
        layer->fill = QColor(230, 30, 80); // Vermelho/rosa badge
        layer->stroke = Qt::white;
        layer->strokeWidth = 3.0;
        break;
    }
    case ShapeKind::SpeechBubble: {
        layer->name = m_i18n->t("common", "menu.layer.shape.speechBubble").remove('&');
        const double w = std::clamp(docW * 0.32, 140.0, 420.0);
        const double h = std::clamp(docH * 0.22, 100.0, 280.0);
        layer->points = QPolygonF{
            QPointF(0, 0), QPointF(w, 0), QPointF(w, h), QPointF(0, h)
        };
        layer->cornerRadius = 20.0;
        layer->fill = Qt::white;
        layer->stroke = QColor(30, 30, 30);
        layer->strokeWidth = 3.0;
        break;
    }
    case ShapeKind::Heart: {
        layer->name = m_i18n->t("common", "menu.layer.shape.heart").remove('&');
        const double size = std::clamp(std::min(docW, docH) * 0.24, 100.0, 280.0);
        layer->points = QPolygonF{
            QPointF(0, 0), QPointF(size, 0), QPointF(size, size), QPointF(0, size)
        };
        layer->fill = QColor(244, 63, 94); // Vermelho/rosa romântico
        layer->stroke = Qt::white;
        layer->strokeWidth = 2.0;
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

    if (LayerAlignment::alignLayer(*m_document, m_history.get(), m_selectedId, target)) {
        if (m_canvas)
            m_canvas->update();
    }
}

void MainWindow::alignMultipleLayers(AlignTarget target)
{
    if (!m_document || !m_canvas)
        return;
    const QList<LayerId> ids = m_canvas->selectedLayers();
    if (ids.size() < 2)
        return;

    if (LayerAlignment::alignLayers(*m_document, m_history.get(), ids, target)) {
        m_canvas->update();
    }
}

void MainWindow::distributeHorizontally()
{
    if (!m_document || !m_canvas)
        return;
    const QList<LayerId> ids = m_canvas->selectedLayers();
    if (ids.size() < 3)
        return;

    if (LayerAlignment::distributeHorizontally(*m_document, m_history.get(), ids)) {
        m_canvas->update();
    }
}

void MainWindow::distributeVertically()
{
    if (!m_document || !m_canvas)
        return;
    const QList<LayerId> ids = m_canvas->selectedLayers();
    if (ids.size() < 3)
        return;

    if (LayerAlignment::distributeVertically(*m_document, m_history.get(), ids)) {
        m_canvas->update();
    }
}

void MainWindow::groupSelectedLayers()
{
    if (!m_document || !m_canvas)
        return;
    const QList<LayerId> rawIds = m_canvas->selectedLayers();
    const QString groupName = m_i18n ? m_i18n->t("editor", "layer.group") : QStringLiteral("Group");
    const LayerId groupId = LayerOperations::groupSelected(*m_document, m_history.get(), rawIds, groupName);
    if (!groupId.isNull()) {
        m_canvas->setSelectedLayer(groupId);
        m_canvas->update();
        if (m_layersPanel)
            m_layersPanel->refresh();
    }
}

void MainWindow::ungroupSelectedLayer()
{
    if (!m_document || !m_canvas)
        return;
    const LayerId nextSelection = LayerOperations::ungroup(*m_document, m_history.get(), m_selectedId);
    if (!nextSelection.isNull()) {
        m_canvas->setSelectedLayer(nextSelection);
        m_canvas->update();
        if (m_layersPanel)
            m_layersPanel->refresh();
    }
}

void MainWindow::mergeSelectedLayers()
{
    if (!m_document || !m_canvas)
        return;
    const QList<LayerId> ids = m_canvas->selectedLayers();
    const LayerId mergedId = LayerOperations::mergeSelected(*m_document, m_history.get(), ids, tr("Merged layers"));
    if (mergedId.isNull()) {
        statusBar()->showMessage(tr("Select two or more layers in the same group, or a layer above another, to merge."), 3500);
        return;
    }
    m_canvas->setSelectedLayer(mergedId);
    if (m_layersPanel)
        m_layersPanel->refresh();
}

void MainWindow::mergeVisibleLayers()
{
    if (!m_document || !m_canvas)
        return;
    const LayerId mergedId = LayerOperations::mergeVisible(*m_document, m_history.get(), tr("Visible merge"));
    if (mergedId.isNull()) {
        statusBar()->showMessage(tr("At least two visible unlocked layers are required."), 3500);
        return;
    }
    m_canvas->setSelectedLayer(mergedId);
    if (m_layersPanel)
        m_layersPanel->refresh();
}

void MainWindow::flattenDocument()
{
    if (!m_document || !m_canvas || !m_history)
        return;
    const LayerId flatId = LayerOperations::flatten(*m_document, m_history.get(), tr("Flattened image"));
    if (flatId.isNull()) {
        statusBar()->showMessage(tr("All layers are locked — nothing to flatten."), 3500);
        return;
    }
    m_canvas->setSelectedLayer(flatId);
    if (m_layersPanel)
        m_layersPanel->refresh();
}

void MainWindow::textBehindSubject()
{
    if (!m_document || !m_canvas || !m_history)
        return;
    const QList<LayerId> ids = m_canvas->selectedLayers();
    if (ids.size() != 2)
        return;

    LayerId textId;
    LayerId subjectId;
    for (const LayerId& id : ids) {
        const Layer* layer = m_document->findLayer(id);
        if (!layer) continue;
        if (layer->type() == LayerType::Text) textId = id;
        if (layer->type() == LayerType::Image) subjectId = id;
    }
    LayerOperations::textBehindSubject(*m_document, m_history.get(), textId, subjectId);
}


void MainWindow::startFromTemplate(int templateKind)
{
    if (!confirmDiscardUnsavedChanges())
        return;

    auto kind = static_cast<TemplateKind>(templateKind);
    auto doc = TemplateFactory::createTemplate(kind, m_i18n);
    if (!doc)
        return;

    attachDocument(std::move(doc));
    m_canvas->fitToViewport();
    enterEditor();
    updateWindowTitle();
}

void MainWindow::exportImage()
{
    if (!m_i18n || !m_document)
        return;

    QUuid selectedLayerId;
    if (m_canvas && !m_canvas->selectedLayers().isEmpty())
        selectedLayerId = m_canvas->selectedLayers().first();
    else if (m_layersPanel && !m_layersPanel->selectedLayer().isNull())
        selectedLayerId = m_layersPanel->selectedLayer();

    ExportDialog dialog(m_document, m_i18n, selectedLayerId, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const ExportSettings settings = dialog.settings();
    QString filter = QStringLiteral("PNG (*.png)");
    if (settings.format == QLatin1String("jpeg"))
        filter = QStringLiteral("JPEG (*.jpg *.jpeg)");
    else if (settings.format == QLatin1String("webp"))
        filter = QStringLiteral("WebP (*.webp)");

    QString defaultName = QStringLiteral("export");
    if (settings.exportSelectedLayerOnly && !settings.selectedLayerId.isNull()) {
        if (const auto* layer = m_document->findLayer(settings.selectedLayerId)) {
            if (!layer->name.trimmed().isEmpty())
                defaultName = layer->name.trimmed();
        }
    }

    QString path = QFileDialog::getSaveFileName(
        this, m_i18n->t("editor", "export.dialog.title"),
        defaultName, filter);
    if (path.isEmpty())
        return;

    const QString suffix = QStringLiteral(".") + settings.format;
    if (!path.endsWith(suffix, Qt::CaseInsensitive))
        path += suffix;

    QString error;
    bool success = false;
    {
        QGuiApplication::setOverrideCursor(Qt::WaitCursor);
        if (settings.exportSelectedLayerOnly && !settings.selectedLayerId.isNull()) {
            success = exportLayerToImage(*m_document, settings.selectedLayerId, path,
                                         settings.format, settings.quality,
                                         settings.scale, &error);
        } else {
            success = exportDocumentToImage(*m_document, path, settings.format,
                                            settings.quality, settings.scale, &error);
        }
        QGuiApplication::restoreOverrideCursor();
    }

    if (!success) {
        QMessageBox::warning(this,
                             m_i18n->t("editor", "export.error.title"), error);
        return;
    }
    QMessageBox::information(
        this, m_i18n->t("editor", "export.success.title"),
        m_i18n->t("editor", "export.success").arg(path));
}

void MainWindow::quickExportLayerPng(const LayerId& id)
{
    if (!m_document || id.isNull())
        return;

    const auto* layer = m_document->findLayer(id);
    if (!layer)
        return;

    QString defaultName = layer->name.trimmed();
    if (defaultName.isEmpty())
        defaultName = QStringLiteral("layer");
    // Sanitize filename
    defaultName.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));

    QString exportDir = QDir::homePath();
    if (!m_currentFilePath.isEmpty()) {
        exportDir = QFileInfo(m_currentFilePath).absolutePath();
    }

    const QString defaultPath = QDir(exportDir).filePath(defaultName + QStringLiteral(".png"));

    const QString path = QFileDialog::getSaveFileName(
        this,
        m_i18n ? m_i18n->t("common", "panel.quickExportPng") : QStringLiteral("Quick Export as PNG"),
        defaultPath,
        QStringLiteral("PNG (*.png)"));

    if (path.isEmpty())
        return;

    QString error;
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    const bool success = exportLayerToImage(*m_document, id, path,
                                            QStringLiteral("png"), 95, 1.0, &error);
    QGuiApplication::restoreOverrideCursor();
    if (!success) {
        QMessageBox::warning(this,
                             m_i18n ? m_i18n->t("editor", "export.error.title") : QStringLiteral("Export failed"),
                             error);
        return;
    }

    statusBar()->showMessage(
        m_i18n ? m_i18n->t("editor", "export.success").arg(path) : QStringLiteral("Exported to %1").arg(path),
        4000);
}

void MainWindow::flipLayer(bool horizontal)
{
    if (!m_document || m_selectedId.isNull())
        return;
    LayerOperations::flip(*m_document, m_history.get(), m_selectedId, horizontal);
}

void MainWindow::rotateLayer(double degrees)
{
    if (!m_document || m_selectedId.isNull())
        return;
    LayerOperations::rotate(*m_document, m_history.get(), m_selectedId, degrees);
}

void MainWindow::deleteSelectedLayer()
{
    if (m_selectedId.isNull() || !m_document || !m_history)
        return;
    if (LayerOperations::deleteLayer(*m_document, m_history.get(), m_selectedId)) {
        m_canvas->clearSelection();
    } else {
        if (!m_document->findLayer(m_selectedId)) {
            m_selectedId = LayerId();
            m_canvas->clearSelection();
        }
    }
}

bool MainWindow::saveDocument()
{
    if (m_currentFilePath.isEmpty())
        return saveDocumentAs();

    QString error;
    if (m_document)
        m_document->purgeUnreferencedAssets();
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
    // Pré-preenchimento: vazio por padrão; após abrir uma recuperação como
    // cópia, sugere o arquivo original para o usuário não perder o rastro.
    const QString suggestion = !m_suggestedSavePath.isEmpty()
                                   ? m_suggestedSavePath
                                   : QStringLiteral("untitled.creatorcanvas");
    m_suggestedSavePath.clear();
    QString path = QFileDialog::getSaveFileName(
        this,
        m_i18n->t("common", "dialog.saveAs.title"),
        suggestion,
        QStringLiteral("CreatorCanvas (*.creatorcanvas)"));
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QLatin1String(".creatorcanvas")))
        path += QLatin1String(".creatorcanvas");

    QString error;
    if (m_document)
        m_document->purgeUnreferencedAssets();
    const QImage thumb = generateThumbnail(*m_document);
    if (!cc::saveDocument(*m_document, path, &error, &thumb)) {
        QMessageBox::warning(this,
                             m_i18n->t("common", "dialog.saveError.title"), error);
        return false;
    }
    m_currentFilePath = path;
    m_modified = false;
    if (m_autosave) {
        m_autosave->discardRecovery();
        m_autosave->setSourcePath(path);
    }
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

    RecoveryDialog dialog(m_i18n, m_autosave->recoveryFilePath(),
                          m_autosave->recoverySourcePath(), this);
    if (dialog.exec() != QDialog::Accepted) {
        // Esc/X NÃO apaga a única cópia do trabalho não salvo — só um
        // descarte explícito remove o arquivo. O diálogo reaparece no
        // próximo início.
        return;
    }

    const auto choice = dialog.userChoice();
    if (choice == RecoveryDialog::Choice::Discard) {
        m_autosave->discardRecovery();
        return;
    }

    QString error;
    auto loaded = loadDocument(m_autosave->recoveryFilePath(), &error);
    if (!loaded) {
        if (m_i18n)
            QMessageBox::warning(this,
                                 m_i18n->t("common", "dialog.openError.title"), error);
        // Recuperação ilegível (corrompida/versão nova): preserva o arquivo
        // renomeado para .corrupt em vez de apagá-lo — o usuário ainda pode
        // resgatar o conteúdo.
        m_autosave->quarantineRecovery();
        return;
    }

    // Procedência: se o arquivo ORIGINAL é mais recente que a recuperação,
    // avisar antes de abrir — salvar a recuperação por cima pode jogar fora
    // trabalho novo (o usuário editou e salvou entre o crash e agora).
    const QString sourcePath = m_autosave->recoverySourcePath();
    if (!sourcePath.isEmpty() && QFileInfo::exists(sourcePath)) {
        const QDateTime srcModified = QFileInfo(sourcePath).lastModified();
        const QDateTime recModified = QFileInfo(m_autosave->recoveryFilePath()).lastModified();
        if (srcModified > recModified) {
            const QString title = m_i18n
                ? m_i18n->t("common", "dialog.recovery.newerTitle")
                : QStringLiteral("O arquivo do projeto é mais recente");
            const QString body = (m_i18n
                ? m_i18n->t("common", "dialog.recovery.newerBody")
                : QStringLiteral("O arquivo \"%1\" foi salvo em %2 — DEPOIS da recuperação (%3). Se você salvar a recuperação por cima dele, pode perder o trabalho mais novo. Continuar com a recuperação?"))
                  .arg(QDir::toNativeSeparators(sourcePath),
                       srcModified.toString(QStringLiteral("dd/MM/yyyy HH:mm:ss")),
                       recModified.toString(QStringLiteral("dd/MM/yyyy HH:mm:ss")));
            const auto answer = QMessageBox::warning(
                this, title, body,
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer != QMessageBox::Yes) {
                // Mantém a recuperação no disco e o documento atual intacto.
                return;
            }
        }
    }

    attachDocument(std::move(loaded));
    m_modified = true; // recovered session stays unsaved until explicitly saved
    if (m_docController) {
        m_docController->setModified(true);
    }
    updateWindowTitle();
    enterEditor();

    if (choice == RecoveryDialog::Choice::OpenAsCopy) {
        // Pré-preenche o "Salvar Como" com o arquivo original: conveniente
        // e mantém a trilha de onde veio o conteúdo.
        m_suggestedSavePath = sourcePath;
        saveDocumentAs();
    }
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

    if (m_actions)
        m_actions->retranslate(m_i18n);
    if (m_menuBuilder)
        m_menuBuilder->retranslate(m_i18n);

    if (m_layersDock)
        m_layersDock->setWindowTitle(m_i18n->t("common", "panel.layers"));
    if (m_textDock)
        m_textDock->setWindowTitle(m_i18n->t("editor", "text.title"));
    if (m_shapeDock)
        m_shapeDock->setWindowTitle(m_i18n->t("editor", "shape.title"));
    if (m_imageDock)
        m_imageDock->setWindowTitle(m_i18n->t("common", "panel.image"));
    if (m_historyDock)
        m_historyDock->setWindowTitle(m_i18n->t("common", "panel.history"));

    if (m_toolOptionsBarWidget)
        m_toolOptionsBarWidget->retranslateUi();

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
