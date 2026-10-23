#include "mainwindow.h"

#include "core/document/NewDocumentSpec.h"
#include "core/DocumentController.h"
#include "core/selection/SelectionEngine.h"
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
#include "services/settingsservice.h"
#include "ui/textinspector.h"
#include "ui/shapeinspector.h"
#include "ui/imageinspector.h"
#include "ui/historypanel.h"
#include "ui/designtokens.h"
#include "ui/recoverydialog.h"
#include "ui/themeicons.h"
#include "ui/tooloptionsbar.h"

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


// Formas do Shape Power Pack (menu Inserir > Forma): kind + chave i18n + fallback PT
struct NewShapeDef { ShapeKind kind; const char* key; const char* fallback; };
static const NewShapeDef kNewShapeDefs[] = {
    {ShapeKind::Triangle,   "triangle",   QT_TRANSLATE_NOOP("editor", "Triângulo")},
    {ShapeKind::Diamond,    "diamond",    "Losango"},
    {ShapeKind::Pentagon,   "pentagon",   "Pentágono"},
    {ShapeKind::Hexagon,    "hexagon",    "Hexágono"},
    {ShapeKind::Cross,      "cross",      "Cruz"},
    {ShapeKind::Checkmark,  "checkmark",  "Marca de verificação"},
    {ShapeKind::Cloud,      "cloud",      "Nuvem"},
    {ShapeKind::Explosion,  "explosion",  "Explosão (Burst)"},
};
constexpr int kNewShapeCount = sizeof(kNewShapeDefs) / sizeof(kNewShapeDefs[0]);

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

    m_addArrowRightAction = new QAction(this);
    m_addArrowRightAction->setIcon(ThemeIcons::toolShape());
    connect(m_addArrowRightAction, &QAction::triggered, this, [this] {
        addShape(ShapeKind::ArrowRight);
    });

    m_addArrowCurvedAction = new QAction(this);
    m_addArrowCurvedAction->setIcon(ThemeIcons::toolShape());
    connect(m_addArrowCurvedAction, &QAction::triggered, this, [this] {
        addShape(ShapeKind::ArrowCurved);
    });

    m_addStarAction = new QAction(this);
    m_addStarAction->setIcon(ThemeIcons::toolShape());
    connect(m_addStarAction, &QAction::triggered, this, [this] {
        addShape(ShapeKind::Star);
    });

    m_addBadgeAction = new QAction(this);
    m_addBadgeAction->setIcon(ThemeIcons::toolShape());
    connect(m_addBadgeAction, &QAction::triggered, this, [this] {
        addShape(ShapeKind::Badge);
    });

    m_addSpeechBubbleAction = new QAction(this);
    m_addSpeechBubbleAction->setIcon(ThemeIcons::toolShape());
    connect(m_addSpeechBubbleAction, &QAction::triggered, this, [this] {
        addShape(ShapeKind::SpeechBubble);
    });

    m_addHeartAction = new QAction(this);
    m_addHeartAction->setIcon(ThemeIcons::toolShape());
    connect(m_addHeartAction, &QAction::triggered, this, [this] {
        addShape(ShapeKind::Heart);
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

    m_ungroupAction = new QAction(this);
    m_ungroupAction->setIcon(ThemeIcons::layerTypeGroup());
    m_ungroupAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));
    connect(m_ungroupAction, &QAction::triggered, this, &MainWindow::ungroupSelectedLayer);

    m_mergeSelectedAction = new QAction(this);
    m_mergeSelectedAction->setEnabled(false);
    // Ctrl+E é o Export. Antes o Merge Selected usava o MESMO atalho e o
    // Qt tratava como atalho ambíguo (nenhum disparava).
    m_mergeSelectedAction->setShortcuts({QKeySequence(QStringLiteral("Ctrl+Shift+E"))});
    connect(m_mergeSelectedAction, &QAction::triggered,
            this, &MainWindow::mergeSelectedLayers);

    m_mergeVisibleAction = new QAction(this);
    m_mergeVisibleAction->setEnabled(false);
    connect(m_mergeVisibleAction, &QAction::triggered,
            this, &MainWindow::mergeVisibleLayers);

    m_flattenAction = new QAction(this);
    m_flattenAction->setEnabled(false);
    connect(m_flattenAction, &QAction::triggered,
            this, &MainWindow::flattenDocument);

    m_textBehindSubjectAction = new QAction(this);
    m_textBehindSubjectAction->setEnabled(false);
    connect(m_textBehindSubjectAction, &QAction::triggered,
            this, &MainWindow::textBehindSubject);

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

    m_toggleHistoryAction = new QAction(this);
    m_toggleHistoryAction->setIcon(ThemeIcons::actionHistory());
    m_toggleHistoryAction->setCheckable(true);
    m_toggleHistoryAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
    connect(m_toggleHistoryAction, &QAction::triggered, this, [this](bool checked) {
        if (m_historyDock) {
            m_historyDock->setVisible(checked);
            if (checked) m_historyDock->raise();
        }
    });

    m_toggleToolsBarAction = new QAction(this);
    m_toggleToolsBarAction->setCheckable(true);
    m_toggleToolsBarAction->setChecked(true);
    m_toggleToolsBarAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    connect(m_toggleToolsBarAction, &QAction::triggered, this, [this](bool checked) {
        const bool narrow = width() < Design::BreakSM;
        if (m_toolsBar) m_toolsBar->setVisible(checked && !narrow);
        if (m_toolsBarCompact) m_toolsBarCompact->setVisible(checked && narrow);
        if (m_toolOptionsBar)
            m_toolOptionsBar->setVisible(checked && m_toolOptionsBarWidget
                                         && m_toolOptionsBarWidget->currentToolIndex() != 0);
        QTimer::singleShot(150, this, [this] { if (m_canvas) m_canvas->fitToViewport(); });
    });

    m_toggleRightPanelAction = new QAction(this);
    m_toggleRightPanelAction->setCheckable(true);
    m_toggleRightPanelAction->setChecked(true);
    m_toggleRightPanelAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_J));
    connect(m_toggleRightPanelAction, &QAction::triggered, this, [this](bool checked) {
        const QList<QDockWidget*> panels = {m_layersDock, m_textDock, m_shapeDock,
                                            m_imageDock, m_historyDock};
        for (auto* panel : panels)
            if (panel) panel->setVisible(checked);
        QTimer::singleShot(150, this, [this] { if (m_canvas) m_canvas->fitToViewport(); });
    });

    m_toggleLayersDockAction = new QAction(this);
    m_toggleLayersDockAction->setCheckable(true);
    m_toggleLayersDockAction->setChecked(true);
    connect(m_toggleLayersDockAction, &QAction::triggered, this, [this](bool checked) {
        if (m_layersDock) {
            m_layersDock->setVisible(checked);
            if (checked) m_layersDock->raise();
        }
    });

    m_toggleTextDockAction = new QAction(this);
    m_toggleTextDockAction->setCheckable(true);
    m_toggleTextDockAction->setChecked(true);
    connect(m_toggleTextDockAction, &QAction::triggered, this, [this](bool checked) {
        if (m_textDock) {
            m_textDock->setVisible(checked);
            if (checked) m_textDock->raise();
        }
    });

    m_toggleShapeDockAction = new QAction(this);
    m_toggleShapeDockAction->setCheckable(true);
    m_toggleShapeDockAction->setChecked(true);
    connect(m_toggleShapeDockAction, &QAction::triggered, this, [this](bool checked) {
        if (m_shapeDock) {
            m_shapeDock->setVisible(checked);
            if (checked) m_shapeDock->raise();
        }
    });

    m_toggleImageDockAction = new QAction(this);
    m_toggleImageDockAction->setCheckable(true);
    m_toggleImageDockAction->setChecked(true);
    connect(m_toggleImageDockAction, &QAction::triggered, this, [this](bool checked) {
        if (m_imageDock) {
            m_imageDock->setVisible(checked);
            if (checked) m_imageDock->raise();
        }
    });

    m_flipHAction = new QAction(this);
    m_flipHAction->setIcon(ThemeIcons::actionFlipH());
    connect(m_flipHAction, &QAction::triggered,
            this, [this] { flipLayer(true); });

    m_flipVAction = new QAction(this);
    m_flipVAction->setIcon(ThemeIcons::actionFlipV());
    connect(m_flipVAction, &QAction::triggered,
            this, [this] { flipLayer(false); });
    m_rotateLeftAction = new QAction(this);
    m_rotateLeftAction->setIcon(ThemeIcons::actionRotateLeft());
    connect(m_rotateLeftAction, &QAction::triggered, this, [this] { rotateLayer(-90.0); });
    m_rotateRightAction = new QAction(this);
    m_rotateRightAction->setIcon(ThemeIcons::actionRotateRight());
    connect(m_rotateRightAction, &QAction::triggered, this, [this] { rotateLayer(90.0); });

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
    m_toolSelectAction->setToolTip((m_i18n ? m_i18n->t("editor", "tools.select")
                                          : QStringLiteral("Select / Move")) + QStringLiteral(" (V)"));
    m_toolGroup->addAction(m_toolSelectAction);
    connect(m_toolSelectAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::Select);
    });

    m_toolCropAction = new QAction(this);
    m_toolCropAction->setCheckable(true);
    m_toolCropAction->setShortcut(QKeySequence(QStringLiteral("C")));
    m_toolCropAction->setToolTip((m_i18n ? m_i18n->t("editor", "tools.crop")
                                        : QStringLiteral("Crop")) + QStringLiteral(" (C)"));
    m_toolGroup->addAction(m_toolCropAction);
    connect(m_toolCropAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::Crop);
    });

    m_toolScissorsAction = new QAction(this);
    m_toolScissorsAction->setCheckable(true);
    m_toolScissorsAction->setShortcut(QKeySequence(QStringLiteral("X")));
    m_toolScissorsAction->setToolTip((m_i18n ? m_i18n->t("editor", "tools.scissors")
                                            : QStringLiteral("Scissors Selection")) + QStringLiteral(" (X)"));
    m_toolGroup->addAction(m_toolScissorsAction);
    connect(m_toolScissorsAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::Scissors);
    });

    m_toolWandAction = new QAction(this);
    m_toolWandAction->setCheckable(true);
    m_toolWandAction->setShortcut(QKeySequence(QStringLiteral("W")));
    m_toolWandAction->setToolTip((m_i18n ? m_i18n->t("editor", "tools.wand")
                                        : QStringLiteral("Magic Wand")) + QStringLiteral(" (W)"));
    m_toolGroup->addAction(m_toolWandAction);
    connect(m_toolWandAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::MagicWand);
    });

    m_toolCloneAction = new QAction(this);
    m_toolCloneAction->setCheckable(true);
    m_toolCloneAction->setShortcut(QKeySequence(QStringLiteral("S")));
    m_toolCloneAction->setToolTip((m_i18n ? m_i18n->t("editor", "tools.clone")
                                         : QStringLiteral("Clone Stamp")) + QStringLiteral(" (S)"));
    m_toolGroup->addAction(m_toolCloneAction);
    connect(m_toolCloneAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::CloneStamp);
    });

    m_toolPaintAction = new QAction(this);
    m_toolPaintAction->setCheckable(true);
    m_toolPaintAction->setShortcut(QKeySequence(QStringLiteral("B")));
    m_toolPaintAction->setToolTip((m_i18n ? m_i18n->t("editor", "tools.paint")
                                         : QStringLiteral("Brush")) + QStringLiteral(" (B)"));
    m_toolGroup->addAction(m_toolPaintAction);
    connect(m_toolPaintAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::Paint);
    });

    m_toolFloodAction = new QAction(this);
    m_toolFloodAction->setCheckable(true);
    // G agora é o Gradiente (grupo Photoshop: G / Shift+G = balde).
    m_toolFloodAction->setShortcut(QKeySequence(QStringLiteral("Shift+G")));
    m_toolFloodAction->setToolTip((m_i18n ? m_i18n->t("editor", "tools.flood")
                                         : QStringLiteral("Paint Bucket")) + QStringLiteral(" (Shift+G)"));
    m_toolGroup->addAction(m_toolFloodAction);
    connect(m_toolFloodAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::FloodFill);
    });

    m_toolEyedropperAction = new QAction(this);
    m_toolEyedropperAction->setCheckable(true);
    m_toolEyedropperAction->setShortcut(QKeySequence(QStringLiteral("I")));
    m_toolEyedropperAction->setToolTip((m_i18n ? m_i18n->t("editor", "tools.eyedropper")
                                              : QStringLiteral("Eyedropper")) + QStringLiteral(" (I)"));
    m_toolGroup->addAction(m_toolEyedropperAction);
    connect(m_toolEyedropperAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::Eyedropper);
    });

    // --- Ferramentas estilo PhotoCraft adicionadas ---
    m_toolMarqueeAction = new QAction(this);
    m_toolMarqueeAction->setCheckable(true);
    m_toolMarqueeAction->setShortcut(QKeySequence(QStringLiteral("M")));
    m_toolMarqueeAction->setToolTip((m_i18n ? m_i18n->t("editor", "tools.marquee")
                                           : QStringLiteral("Marquee Selection")) + QStringLiteral(" (M)"));
    m_toolGroup->addAction(m_toolMarqueeAction);
    connect(m_toolMarqueeAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::Marquee);
    });

    m_toolGradientAction = new QAction(this);
    m_toolGradientAction->setCheckable(true);
    m_toolGradientAction->setShortcut(QKeySequence(QStringLiteral("G")));
    m_toolGradientAction->setToolTip((m_i18n ? m_i18n->t("editor", "tools.gradient")
                                            : QStringLiteral("Gradient")) + QStringLiteral(" (G)"));
    m_toolGroup->addAction(m_toolGradientAction);
    connect(m_toolGradientAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::Gradient);
    });

    m_toolRetouchAction = new QAction(this);
    m_toolRetouchAction->setCheckable(true);
    // "R" já é o atalho do retângulo de forma — atalho duplicado no mesmo
    // QActionGroup/QActionWindow deixa AMBÍGUO e nenhum dos dois dispara.
    // "U" é a convenção do grupo de retoque no Photoshop.
    m_toolRetouchAction->setShortcut(QKeySequence(QStringLiteral("U")));
    m_toolRetouchAction->setToolTip((m_i18n ? m_i18n->t("editor", "tools.retouch")
                                           : QStringLiteral("Retouch Brush")) + QStringLiteral(" (U)"));
    m_toolGroup->addAction(m_toolRetouchAction);
    connect(m_toolRetouchAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::Retouch);
    });

    m_toolHealingAction = new QAction(this);
    m_toolHealingAction->setCheckable(true);
    m_toolHealingAction->setShortcut(QKeySequence(QStringLiteral("J")));
    m_toolHealingAction->setToolTip((m_i18n ? m_i18n->t("editor", "tools.healing")
                                           : QStringLiteral("Spot Healing / Red Eye")) + QStringLiteral(" (J)"));
    m_toolGroup->addAction(m_toolHealingAction);
    connect(m_toolHealingAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->setTool(CanvasTool::Healing);
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
    m_fileMenu = menuBar()->addMenu(QString());
    m_fileMenu->addAction(m_newAction);
    m_fileMenu->addAction(m_openAction);
    m_openRecentMenu = m_fileMenu->addMenu(QString());
    // Lista reconstruida a cada abertura — sempre atualizada
    connect(m_openRecentMenu, &QMenu::aboutToShow, this, [this] { refreshRecentMenu(); });
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

    m_imageMenu = menuBar()->addMenu(QStringLiteral("Image"));
    m_imageMenu->addAction(m_importAction);
    m_imageMenu->addAction(m_exportAction);
    m_imageMenu->addSeparator();

    // Rotação & Espelhamento da Imagem/Camada
    auto* flipHAct = m_imageMenu->addAction(QStringLiteral("Virar Horizontalmente"));
    flipHAct->setIcon(ThemeIcons::actionFlipH());
    connect(flipHAct, &QAction::triggered, this, [this] { flipLayer(true); });
    auto* flipVAct = m_imageMenu->addAction(QStringLiteral("Virar Verticalmente"));
    flipVAct->setIcon(ThemeIcons::actionFlipV());
    connect(flipVAct, &QAction::triggered, this, [this] { flipLayer(false); });
    auto* rotLeftAct = m_imageMenu->addAction(QStringLiteral("Girar 90° para a Esquerda"));
    rotLeftAct->setIcon(ThemeIcons::actionRotateLeft());
    connect(rotLeftAct, &QAction::triggered, this, [this] { rotateLayer(-90.0); });
    auto* rotRightAct = m_imageMenu->addAction(QStringLiteral("Girar 90° para a Direita"));
    rotRightAct->setIcon(ThemeIcons::actionRotateRight());
    connect(rotRightAct, &QAction::triggered, this, [this] { rotateLayer(90.0); });
    m_imageMenu->addSeparator();

    // IA & Ferramentas Inteligentes
    m_imageMenu->addAction(m_removeBgAiAction);
    m_imageMenu->addAction(m_removeBgAiQuickAction);
    m_mockupAction = m_imageMenu->addAction(QStringLiteral("Mockup / Substituir Logo..."));
    m_mockupAction->setEnabled(false);
    connect(m_mockupAction, &QAction::triggered, this, [this] {
        if (m_imageInspector) m_imageInspector->openMockupDialog();
    });
    m_imageMenu->addSeparator();

    // Mesclagens do Documento
    m_imageMenu->addAction(m_mergeSelectedAction);
    m_imageMenu->addAction(m_mergeVisibleAction);
    m_imageMenu->addAction(m_flattenAction);

    // --- Menu Filtro (Filtros criativos e de estilo Photocraft) ---
    m_filterMenu = menuBar()->addMenu(QStringLiteral("Filter"));

    auto addFilterAction = [this](QMenu* menu, const QString& title, int presetIdx) {
        auto* act = menu->addAction(title);
        connect(act, &QAction::triggered, this, [this, presetIdx] {
            if (m_imageInspector) {
                m_imageInspector->applyPreset(presetIdx);
                if (m_imageDock && m_imageDock->isVisible())
                    m_imageDock->raise();
            }
        });
        return act;
    };

    const auto tr = [this](const char* key, const char* fallback) {
        return m_i18n ? m_i18n->t("editor", QString::fromLatin1(key))
                      : QString::fromLatin1(fallback);
    };
    m_artisticMenu = m_filterMenu->addMenu(
        tr("filters.artistic", "Artistic & Color"));
    m_toneMenu = m_filterMenu->addMenu(
        tr("filters.tone", "Tone & Special Effects"));
    // Mesma ordem dos presets (0..10); os textos são re-traduzidos em
    // retranslateUi, por isso os ponteiros ficam guardados.
    const QList<QPair<QMenu*, QPair<const char*, const char*>>> filterDefs = {
        {m_artisticMenu, {"filters.grayscale", "Black & White"}},
        {m_artisticMenu, {"filters.sepia", "Classic Sepia"}},
        {m_artisticMenu, {"filters.vintage", "Vintage Look"}},
        {m_artisticMenu, {"filters.highContrast", "High Contrast"}},
        {m_artisticMenu, {"filters.vividPop", "Vivid Pop"}},
        {m_artisticMenu, {"filters.cyberpunk", "Cyberpunk Neon"}},
        {m_artisticMenu, {"filters.goldenHour", "Golden Hour (Sunset)"}},
        {m_artisticMenu, {"filters.cinemaNoir", "Cinema Noir"}},
        {m_toneMenu, {"filters.invert", "Invert Colors"}},
        {m_toneMenu, {"filters.posterize", "Posterize (4 Levels)"}},
        {m_toneMenu, {"filters.threshold", "Threshold B&W"}},
    };
    for (int i = 0; i < filterDefs.size(); ++i) {
        QAction* act = addFilterAction(filterDefs[i].first,
                                       tr(filterDefs[i].second.first,
                                          filterDefs[i].second.second),
                                       i);
        m_filterActions.append(act);
    }

    m_filterMenu->addSeparator();
    auto* openInspectAct = m_filterMenu->addAction(QStringLiteral("Abrir Painel de Ajustes e Efeitos..."));
    connect(openInspectAct, &QAction::triggered, this, [this] {
        if (m_imageDock) {
            m_imageDock->show();
            m_imageDock->raise();
        }
    });

    m_selectMenu = menuBar()->addMenu(QStringLiteral("Select"));
    auto* selectAllAct = m_selectMenu->addAction(QStringLiteral("Selecionar Tudo"));
    selectAllAct->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAct, &QAction::triggered, this, [this] {
        if (m_document) {
            QImage full(m_document->width(), m_document->height(), QImage::Format_Alpha8);
            full.fill(255);
            executeSetPixelSelection(*m_document, m_history.get(), full,
                                     QStringLiteral("selection.selectAll"));
        }
    });
    m_deselectAction = m_selectMenu->addAction(
        tr("selection.deselect", "Deselect"));
    m_deselectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    m_invertSelectionAction = m_selectMenu->addAction(
        tr("selection.invert", "Invert Selection"));
    m_invertSelectionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
    m_selectMenu->addSeparator();
    m_selectionOpsAction = m_selectMenu->addAction(
        tr("selection.operations", "Selection Operations (Copy / Cut / Mask)..."));
    m_selectionOpsAction->setToolTip(
        tr("selection.operationsTooltip", "Opens the operations menu with the active selection"));
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
    connect(m_deselectAction, &QAction::triggered, this, [this] {
        if (m_document && !m_document->pixelSelection().isNull())
            executeSetPixelSelection(*m_document, m_history.get(), QImage(),
                                     QStringLiteral("selection.deselect"));
    });
    connect(m_selectionOpsAction, &QAction::triggered, this, [this] {
        if (m_canvas) m_canvas->openSelectionActionsMenu();
    });

    // --- Select > Modify: operações morfológicas na seleção ativa ---
    // (Photocraft: expand/contract/border/smooth/feather sobre a máscara)
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

    m_modifyMenu = m_selectMenu->addMenu(QString());
    auto* expandAct = m_modifyMenu->addAction(QString());
    m_modifyActions.append(expandAct);
    connect(expandAct, &QAction::triggered, this, [modifySelection, askRadius, this] {
        const double r = askRadius(
            m_i18n ? m_i18n->t("editor", "selection.modify.expand")
                   : QStringLiteral("Expand Selection"), 2.0);
        if (r > 0)
            modifySelection([r](const QImage& s) {
                return SelectionEngine::expand(s, float(r));
            });
    });
    auto* contractAct = m_modifyMenu->addAction(QString());
    m_modifyActions.append(contractAct);
    connect(contractAct, &QAction::triggered, this, [modifySelection, askRadius, this] {
        const double r = askRadius(
            m_i18n ? m_i18n->t("editor", "selection.modify.contract")
                   : QStringLiteral("Contract Selection"), 2.0);
        if (r > 0)
            modifySelection([r](const QImage& s) {
                return SelectionEngine::contract(s, float(r));
            });
    });
    auto* borderAct = m_modifyMenu->addAction(QString());
    m_modifyActions.append(borderAct);
    connect(borderAct, &QAction::triggered, this, [modifySelection, askRadius, this] {
        const double r = askRadius(
            m_i18n ? m_i18n->t("editor", "selection.modify.border")
                   : QStringLiteral("Border Selection"), 4.0);
        if (r > 0)
            modifySelection([r](const QImage& s) {
                return SelectionEngine::border(s, float(r));
            });
    });
    auto* smoothAct = m_modifyMenu->addAction(QString());
    m_modifyActions.append(smoothAct);
    connect(smoothAct, &QAction::triggered, this, [modifySelection, askRadius, this] {
        const double r = askRadius(
            m_i18n ? m_i18n->t("editor", "selection.modify.smooth")
                   : QStringLiteral("Smooth Selection"), 2.0);
        if (r > 0)
            modifySelection([r](const QImage& s) {
                return SelectionEngine::smooth(s, float(r));
            });
    });
    auto* featherAct = m_modifyMenu->addAction(QString());
    m_modifyActions.append(featherAct);
    connect(featherAct, &QAction::triggered, this, [modifySelection, askRadius, this] {
        const double r = askRadius(
            m_i18n ? m_i18n->t("editor", "selection.modify.feather")
                   : QStringLiteral("Feather Selection"), 5.0);
        if (r > 0)
            modifySelection([r](const QImage& s) {
                return SelectionEngine::feather(s, float(r));
            });
    });

    // --- Select > Color Range: seleção por faixa de cor ---
    m_colorRangeAction = m_selectMenu->addAction(QString());
    auto* colorRangeAct = m_colorRangeAction;
    connect(colorRangeAct, &QAction::triggered, this, [this] {
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

        // Usa a cor atual do pincel como alvo da amostra.
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

    m_layerMenu = menuBar()->addMenu(QString());
    m_layerMenu->addAction(m_addTextAction);

    // --- Nova Camada de Preenchimento (Fill Layer, estilo Photoshop) ---
    m_addFillLayerAction = m_layerMenu->addAction(QString());
    m_addFillLayerAction->setIcon(ThemeIcons::toolGradient());
    connect(m_addFillLayerAction, &QAction::triggered, this, [this] {
        if (!m_document)
            return;
        // Diálogo de cor para o preenchimento sólido.
        const QColor chosen = QColorDialog::getColor(
            QColor(61, 126, 255), this,
            m_i18n ? m_i18n->t("editor", "layer.newFillLayer.colorTitle")
                   : QStringLiteral("Fill Layer Color"));
        if (!chosen.isValid())
            return;
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

    // --- Nova Camada de Ajuste (Adjustment Layer, estilo Photoshop) ---
    m_addAdjustmentLayerAction = m_layerMenu->addAction(QString());
    m_addAdjustmentLayerAction->setIcon(ThemeIcons::actionHistory());
    connect(m_addAdjustmentLayerAction, &QAction::triggered, this, [this] {
        if (!m_document)
            return;
        // Menu com os tipos de ajuste disponíveis.
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
                // Parâmetros padrão por tipo (o efeito é visível de cara;
                // PhotoCraft usa os mesmos defaults de cada diálogo).
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

    m_layerMenu->addSeparator();

    // Submenu para adicionar diferentes tipos de formas geométricas
    m_addShapeMenu = m_layerMenu->addMenu(QString());
    m_addShapeMenu->addAction(m_addRectAction);
    m_addShapeMenu->addAction(m_addRoundedRectAction);
    m_addShapeMenu->addAction(m_addEllipseAction);
    m_addShapeMenu->addAction(m_addLineAction);
    m_addShapeMenu->addSeparator();
    m_addShapeMenu->addAction(m_addArrowRightAction);
    m_addShapeMenu->addAction(m_addArrowCurvedAction);
    m_addShapeMenu->addAction(m_addStarAction);
    m_addShapeMenu->addAction(m_addBadgeAction);
    m_addShapeMenu->addSeparator();
    m_addShapeMenu->addAction(m_addSpeechBubbleAction);
    m_addShapeMenu->addAction(m_addHeartAction);
    m_addShapeMenu->addSeparator();
    for (const auto& def : kNewShapeDefs) {
        QAction* act = m_addShapeMenu->addAction(QString());
        act->setIcon(ThemeIcons::toolShape());
        const ShapeKind kind = def.kind;
        connect(act, &QAction::triggered, this, [this, kind] { addShape(kind); });
        m_addNewShapeActions.append(act);
    }

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
    m_layerMenu->addAction(m_ungroupAction);
    m_layerMenu->addSeparator();
    m_layerMenu->addAction(m_mergeSelectedAction);
    m_layerMenu->addAction(m_mergeVisibleAction);
    m_layerMenu->addAction(m_flattenAction);
    m_layerMenu->addAction(m_textBehindSubjectAction);
    m_layerMenu->addSeparator();
    m_layerMenu->addAction(m_flipHAction);
    m_layerMenu->addAction(m_flipVAction);
    m_layerMenu->addAction(m_rotateLeftAction);
    m_layerMenu->addAction(m_rotateRightAction);
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
    m_viewMenu->addAction(m_toggleHistoryAction);
    m_panelsMenu = m_viewMenu->addMenu(m_i18n ? m_i18n->t("common", "menu.view.panels") : QStringLiteral("Panels"));
    m_panelsMenu->addAction(m_toggleToolsBarAction);
    m_panelsMenu->addAction(m_toggleRightPanelAction);
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
    m_windowMenu = menuBar()->addMenu(QStringLiteral("Window"));
    m_windowMenu->addAction(m_toggleToolsBarAction);
    m_windowMenu->addAction(m_toggleRightPanelAction);
    m_windowMenu->addSeparator();
    m_windowMenu->addAction(m_toggleLayersDockAction);
    m_windowMenu->addAction(m_toggleTextDockAction);
    m_windowMenu->addAction(m_toggleShapeDockAction);
    m_windowMenu->addAction(m_toggleImageDockAction);
    m_windowMenu->addAction(m_toggleHistoryAction);
    m_selectionToolsMenu = m_toolsMenu->addMenu(m_i18n ? m_i18n->t("common", "menu.tools.selection") : QStringLiteral("Select & Cut"));
    m_selectionToolsMenu->addAction(m_toolSelectAction);
    m_selectionToolsMenu->addAction(m_toolMarqueeAction);
    m_selectionToolsMenu->addAction(m_toolScissorsAction);
    m_selectionToolsMenu->addAction(m_toolWandAction);
    m_selectionToolsMenu->addAction(m_toolCropAction);
    m_retouchToolsMenu = m_toolsMenu->addMenu(m_i18n ? m_i18n->t("common", "menu.tools.retouch") : QStringLiteral("Retouch & Paint"));
    m_retouchToolsMenu->addAction(m_toolPaintAction);
    m_retouchToolsMenu->addAction(m_toolGradientAction);
    m_retouchToolsMenu->addAction(m_toolFloodAction);
    m_retouchToolsMenu->addSeparator();
    m_retouchToolsMenu->addAction(m_toolCloneAction);
    m_retouchToolsMenu->addAction(m_toolRetouchAction);
    m_retouchToolsMenu->addAction(m_toolHealingAction);
    m_retouchToolsMenu->addSeparator();
    m_retouchToolsMenu->addAction(m_toolEyedropperAction);
    m_createToolsMenu = m_toolsMenu->addMenu(m_i18n ? m_i18n->t("common", "menu.tools.create") : QStringLiteral("Create"));
    m_createToolsMenu->addAction(m_addTextAction);
    m_createToolsMenu->addSeparator();
    m_createToolsMenu->addAction(m_addRectAction);
    m_createToolsMenu->addAction(m_addRoundedRectAction);
    m_createToolsMenu->addAction(m_addEllipseAction);
    m_createToolsMenu->addAction(m_addLineAction);

    m_settingsMenu = menuBar()->addMenu(QString());
    m_settingsMenu->addAction(m_settingsAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_startScreenAction);

    m_helpMenu = menuBar()->addMenu(QString());
    m_helpMenu->addAction(m_aboutAction);
    m_helpMenu->addAction(m_aboutQtAction);

    warnDuplicateShortcuts(this);
}

void MainWindow::createDemoDocumentForSnapshot()
{
    createDefaultDocument();
    enterEditor();
}

void MainWindow::selectToolByName(const QString& name)
{
    QAction* target = nullptr;
    if (name == QLatin1String("select"))          target = m_toolSelectAction;
    else if (name == QLatin1String("marquee"))    target = m_toolMarqueeAction;
    else if (name == QLatin1String("crop"))       target = m_toolCropAction;
    else if (name == QLatin1String("scissors"))   target = m_toolScissorsAction;
    else if (name == QLatin1String("wand"))       target = m_toolWandAction;
    else if (name == QLatin1String("clone"))      target = m_toolCloneAction;
    else if (name == QLatin1String("paint"))      target = m_toolPaintAction;
    else if (name == QLatin1String("gradient"))   target = m_toolGradientAction;
    else if (name == QLatin1String("flood"))      target = m_toolFloodAction;
    else if (name == QLatin1String("retouch"))    target = m_toolRetouchAction;
    else if (name == QLatin1String("healing"))    target = m_toolHealingAction;
    else if (name == QLatin1String("eyedropper")) target = m_toolEyedropperAction;
    if (target)
        target->trigger();
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

    auto compound = std::make_unique<CompoundCommand>(QStringLiteral("layer.align"));

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
                compound->addCommand(std::make_unique<SetLayerTransformCommand>(
                    *m_document, info.id, info.oldTransform, newT));
            } else {
                m_document->setLayerTransform(info.id, newT);
            }
        }
    }

    if (m_history && !compound->isEmpty()) {
        m_history->execute(std::move(compound));
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

    auto compound = std::make_unique<CompoundCommand>(QStringLiteral("layer.distributeH"));

    for (int i = 1; i < infos.size() - 1; ++i) {
        const double targetX = firstCenter + step * i;
        const double dx = targetX - infos[i].centerX;
        if (!qFuzzyIsNull(dx)) {
            AffineTransform newT = infos[i].oldTransform;
            newT.position += QPointF(dx, 0.0);
            if (m_history) {
                compound->addCommand(std::make_unique<SetLayerTransformCommand>(
                    *m_document, infos[i].id, infos[i].oldTransform, newT));
            } else {
                m_document->setLayerTransform(infos[i].id, newT);
            }
        }
    }

    if (m_history && !compound->isEmpty()) {
        m_history->execute(std::move(compound));
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

    auto compound = std::make_unique<CompoundCommand>(QStringLiteral("layer.distributeV"));

    for (int i = 1; i < infos.size() - 1; ++i) {
        const double targetY = firstCenter + step * i;
        const double dy = targetY - infos[i].centerY;
        if (!qFuzzyIsNull(dy)) {
            AffineTransform newT = infos[i].oldTransform;
            newT.position += QPointF(0.0, dy);
            if (m_history) {
                compound->addCommand(std::make_unique<SetLayerTransformCommand>(
                    *m_document, infos[i].id, infos[i].oldTransform, newT));
            } else {
                m_document->setLayerTransform(infos[i].id, newT);
            }
        }
    }

    if (m_history && !compound->isEmpty()) {
        m_history->execute(std::move(compound));
    }
    m_canvas->update();
}

void MainWindow::groupSelectedLayers()
{
    if (!m_document || !m_canvas)
        return;
    const QList<LayerId> rawIds = m_canvas->selectedLayers();
    if (rawIds.size() < 2)
        return;

    // Filter out background layer and layers whose ancestors are already selected
    QList<LayerId> ids;
    for (const auto& id : rawIds) {
        Layer* layer = m_document->findLayer(id);
        if (!layer || layer->type() == LayerType::Background)
            continue;

        bool hasSelectedAncestor = false;
        GroupLayer* p = m_document->parentOf(id);
        while (p && p != m_document->rootGroup()) {
            if (rawIds.contains(p->id())) {
                hasSelectedAncestor = true;
                break;
            }
            p = m_document->parentOf(p->id());
        }
        if (!hasSelectedAncestor && !ids.contains(id)) {
            ids.append(id);
        }
    }

    if (ids.size() < 2)
        return;

    const QString groupName = m_i18n ? m_i18n->t("editor", "layer.group") : QStringLiteral("Group");

    if (m_history) {
        auto cmd = std::make_unique<GroupLayersCommand>(*m_document, ids, groupName);
        const LayerId groupId = cmd->groupId();
        m_history->execute(std::move(cmd));
        m_canvas->setSelectedLayer(groupId);
    } else {
        auto group = std::make_unique<GroupLayer>();
        group->name = groupName;
        const LayerId groupId = group->id();

        for (const auto& lid : ids) {
            if (auto taken = m_document->takeLayer(lid))
                group->children.push_back(std::move(taken));
        }

        m_document->addLayer(std::move(group));
        m_canvas->setSelectedLayer(groupId);
    }

    m_canvas->update();
    if (m_layersPanel)
        m_layersPanel->refresh();
}

void MainWindow::ungroupSelectedLayer()
{
    if (!m_document || !m_canvas)
        return;
    const LayerId id = m_selectedId;
    Layer* layer = m_document->findLayer(id);
    if (!layer || layer->type() != LayerType::Group)
        return;

    auto* group = static_cast<GroupLayer*>(layer);
    if (group->children.empty())
        return;
    const LayerId nextSelection = group->children.back()->id();
    if (m_history)
        m_history->execute(std::make_unique<UngroupLayersCommand>(*m_document, id));
    else {
        GroupLayer* parent = m_document->parentOf(id);
        const int insertion = m_document->indexOf(id);
        auto holder = m_document->takeLayer(id);
        auto* heldGroup = static_cast<GroupLayer*>(holder.get());
        int index = insertion;
        while (!heldGroup->children.empty()) {
            auto child = std::move(heldGroup->children.front());
            heldGroup->children.erase(heldGroup->children.begin());
            m_document->addLayer(std::move(child), parent, index++);
        }
    }
    m_canvas->setSelectedLayer(nextSelection);
    m_canvas->update();
    if (m_layersPanel)
        m_layersPanel->refresh();
}

namespace {

std::unique_ptr<ImageLayer> rasterLayerFor(Document& document,
                                           const QList<LayerId>& layerIds,
                                           const QString& name)
{
    QImage raster(document.width(), document.height(), QImage::Format_ARGB32_Premultiplied);
    raster.fill(Qt::transparent);
    {
        QPainter painter(&raster);
        RenderOptions options;
        options.drawCheckerboard = false;
        options.canvasBorder = Qt::transparent;
        renderSiblingLayers(document, layerIds, &painter, options);
    }

    const LayerId assetId = document.assets().addImage(raster);
    if (assetId.isNull())
        return {};

    auto layer = std::make_unique<ImageLayer>();
    layer->name = name;
    layer->assetId = assetId;
    layer->naturalWidth = document.width();
    layer->naturalHeight = document.height();
    layer->transform.position = QPointF(document.width() / 2.0, document.height() / 2.0);
    return layer;
}

bool areMergeableSiblings(const Document& document, const QList<LayerId>& ids)
{
    if (ids.size() < 2)
        return false;
    const GroupLayer* parent = document.parentOf(ids.first());
    if (!parent)
        return false;
    for (const LayerId& id : ids) {
        const Layer* layer = document.findLayer(id);
        if (!layer || layer->type() == LayerType::Background || document.parentOf(id) != parent)
            return false;
    }
    return true;
}

} // namespace

void MainWindow::mergeSelectedLayers()
{
    if (!m_document || !m_canvas)
        return;
    QList<LayerId> ids = m_canvas->selectedLayers();
    if (ids.size() == 1) {
        // Merge Down: merge with the layer directly below it
        const LayerId id = ids.first();
        GroupLayer* parent = m_document->parentOf(id);
        if (parent) {
            const int idx = m_document->indexOf(id);
            if (idx > 0 && idx < static_cast<int>(parent->children.size())) {
                const Layer* below = parent->children[static_cast<std::size_t>(idx - 1)].get();
                if (below && below->type() != LayerType::Background) {
                    ids = {below->id(), id}; // in bottom-to-top order
                }
            }
        }
    }

    if (!areMergeableSiblings(*m_document, ids)) {
        statusBar()->showMessage(tr("Select two or more layers in the same group, or a layer above another, to merge."), 3500);
        return;
    }
    auto raster = rasterLayerFor(*m_document, ids, tr("Merged layers"));
    if (!raster || !m_history)
        return;
    const LayerId mergedId = raster->id();
    m_history->execute(std::make_unique<ReplaceLayersWithRasterCommand>(
        *m_document, ids, std::move(raster), QStringLiteral("layer.mergeSelected")));
    m_canvas->setSelectedLayer(mergedId);
    if (m_layersPanel) m_layersPanel->refresh();
}

void MainWindow::mergeVisibleLayers()
{
    if (!m_document || !m_canvas)
        return;
    QList<LayerId> ids;
    for (const auto& child : m_document->rootGroup()->children) {
        // Camadas bloqueadas não participam — ficam intactas por cima
        if (child->visible && !child->locked
            && child->type() != LayerType::Background)
            ids.append(child->id());
    }
    if (ids.size() < 2) {
        statusBar()->showMessage(tr("At least two visible unlocked layers are required."), 3500);
        return;
    }
    auto raster = rasterLayerFor(*m_document, ids, tr("Visible merge"));
    if (!raster || !m_history)
        return;
    const LayerId mergedId = raster->id();
    m_history->execute(std::make_unique<ReplaceLayersWithRasterCommand>(
        *m_document, ids, std::move(raster), QStringLiteral("layer.mergeVisible")));
    m_canvas->setSelectedLayer(mergedId);
    if (m_layersPanel) m_layersPanel->refresh();
}

void MainWindow::flattenDocument()
{
    if (!m_document || !m_canvas || !m_history)
        return;
    QList<LayerId> ids;       // desbloqueadas — serão achatadas na imagem
    QList<Layer*> kept;       // bloqueadas — preservadas no lugar
    for (const auto& child : m_document->rootGroup()->children) {
        if (child->locked)
            kept.append(child.get());
        else
            ids.append(child->id());
    }
    if (ids.isEmpty()) {
        statusBar()->showMessage(tr("All layers are locked — nothing to flatten."), 3500);
        return;
    }

    // Rasterizar SOMENTE as camadas desbloqueadas: esconde as bloqueadas
    // durante o render (o renderer lê `visible` direto) e devolve o estado
    // EXATO de antes — antes, uma camada travada que estava oculta era
    // reexibida involuntariamente após o flatten.
    QVector<QPair<Layer*, bool>> hiddenForRender;
    hiddenForRender.reserve(kept.size());
    for (Layer* lockedLayer : kept) {
        if (lockedLayer->visible) {
            lockedLayer->visible = false;
            hiddenForRender.append({lockedLayer, true});
        }
    }
    QImage raster(m_document->width(), m_document->height(), QImage::Format_ARGB32_Premultiplied);
    raster.fill(Qt::transparent);
    {
        QPainter painter(&raster);
        RenderOptions options;
        options.drawCheckerboard = false;
        options.canvasBorder = Qt::transparent;
        renderDocument(*m_document, &painter, QTransform(), options);
    }
    for (const auto& [layer, wasVisible] : hiddenForRender)
        layer->visible = wasVisible;
    const LayerId assetId = m_document->assets().addImage(raster);
    if (assetId.isNull())
        return;
    auto flat = std::make_unique<ImageLayer>();
    flat->name = tr("Flattened image");
    flat->assetId = assetId;
    flat->naturalWidth = m_document->width();
    flat->naturalHeight = m_document->height();
    flat->transform.position = QPointF(m_document->width() / 2.0, m_document->height() / 2.0);
    const LayerId flatId = flat->id();
    m_history->execute(std::make_unique<ReplaceLayersWithRasterCommand>(
        *m_document, ids, std::move(flat), QStringLiteral("layer.flatten")));
    m_canvas->setSelectedLayer(flatId);
    if (m_layersPanel) m_layersPanel->refresh();
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
    if (textId.isNull() || subjectId.isNull()
        || m_document->parentOf(textId) != m_document->parentOf(subjectId))
        return;

    m_history->execute(std::make_unique<TextBehindSubjectCommand>(
        *m_document, textId, subjectId));
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

void MainWindow::rotateLayer(double degrees)
{
    if (!m_document || m_selectedId.isNull())
        return;
    const auto locks = m_document->effectiveLocks(m_selectedId);
    if (locks.position || locks.all)
        return;
    Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer)
        return;
    const AffineTransform oldT = layer->transform;
    AffineTransform newT = oldT;
    newT.rotationDeg += degrees;
    if (m_history)
        m_history->execute(std::make_unique<SetLayerTransformCommand>(
            *m_document, m_selectedId, oldT, newT));
}

void MainWindow::deleteSelectedLayer()
{
    if (m_selectedId.isNull() || !m_document || !m_history)
        return;
    if (m_document->effectiveLocks(m_selectedId).all)
        return;
    // A1: seleção pendurada após undo de grupo/adição/remoção — sem este
    // guard o comando dereferenciava um pai nulo e crashava no Delete.
    if (!m_document->findLayer(m_selectedId)) {
        m_selectedId = LayerId();
        m_canvas->clearSelection();
        return;
    }
    m_history->execute(
        std::make_unique<RemoveLayerCommand>(*m_document, m_selectedId));
    m_canvas->clearSelection();
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
    if (m_addArrowRightAction)
        m_addArrowRightAction->setText(m_i18n->t("common", "menu.layer.shape.arrowRight"));
    if (m_addArrowCurvedAction)
        m_addArrowCurvedAction->setText(m_i18n->t("common", "menu.layer.shape.arrowCurved"));
    if (m_addStarAction)
        m_addStarAction->setText(m_i18n->t("common", "menu.layer.shape.star"));
    if (m_addBadgeAction)
        m_addBadgeAction->setText(m_i18n->t("common", "menu.layer.shape.badge"));
    if (m_addSpeechBubbleAction)
        m_addSpeechBubbleAction->setText(m_i18n->t("common", "menu.layer.shape.speechBubble"));
    if (m_addHeartAction)
        m_addHeartAction->setText(m_i18n->t("common", "menu.layer.shape.heart"));
    for (int i = 0; i < m_addNewShapeActions.size() && i < kNewShapeCount; ++i) {
        m_addNewShapeActions[i]->setText(
            m_i18n->t("editor", QStringLiteral("shape.kind.%1")
                          .arg(QLatin1String(kNewShapeDefs[i].key))));
    }
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
    if (m_ungroupAction)
        m_ungroupAction->setText(m_i18n->t("common", "menu.layer.ungroup"));
    const bool portuguese = m_i18n && m_i18n->currentLanguage() == QStringLiteral("pt-BR");
    if (m_mergeSelectedAction)
        m_mergeSelectedAction->setText(portuguese ? QStringLiteral("Mesclar camadas selecionadas") : QStringLiteral("Merge selected layers"));
    if (m_mergeVisibleAction)
        m_mergeVisibleAction->setText(portuguese ? QStringLiteral("Mesclar camadas visíveis") : QStringLiteral("Merge visible layers"));
    if (m_flattenAction)
        m_flattenAction->setText(portuguese ? QStringLiteral("Achatar imagem") : QStringLiteral("Flatten image"));

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
    auto setToolTipWithShortcut = [](QAction* action, const QString& label,
                                     const QString& shortcut) {
        if (action)
            action->setToolTip(label + QStringLiteral("  ") + shortcut);
    };
    // Uma única tabela (nome i18n + atalho) para TODAS as ferramentas:
    // antes o retranslate sobrescrevia os tooltips e esquecia gradiente/
    // retoque/reparo, e o balde ainda anunciava "G" (virou Shift+G).
    const bool pt = m_i18n->currentLanguage() == QStringLiteral("pt-BR");
    struct ToolTipDef {
        QAction* action;
        const char* key;
        const char* shortcut;
    };
    const ToolTipDef toolTips[] = {
        {m_toolSelectAction,     "tools.select",     "V"},
        {m_toolMarqueeAction,    "tools.marquee",    "M"},
        {m_toolScissorsAction,   "tools.scissors",   "X"},
        {m_toolWandAction,       "tools.wand",       "W"},
        {m_toolCropAction,       "tools.crop",       "C"},
        {m_toolPaintAction,      "tools.paint",      "B"},
        {m_toolGradientAction,   "tools.gradient",   "G"},
        {m_toolFloodAction,      "tools.flood",      "Shift+G"},
        {m_toolCloneAction,      "tools.clone",      "S"},
        {m_toolRetouchAction,    "tools.retouch",    "U"},
        {m_toolHealingAction,    "tools.healing",    "J"},
        {m_toolEyedropperAction, "tools.eyedropper", "I"},
    };
    for (const ToolTipDef& def : toolTips) {
        if (!def.action)
            continue;
        setToolTipWithShortcut(def.action,
                               m_i18n->t("editor", QString::fromLatin1(def.key)),
                               QString::fromLatin1(def.shortcut));
    }
    setToolTipWithShortcut(m_addTextAction,
                           pt ? QStringLiteral("Texto") : QStringLiteral("Text"), "T");
    setToolTipWithShortcut(m_addRectAction,
                           pt ? QStringLiteral("Retângulo") : QStringLiteral("Rectangle"), "R");

    // Menus Filtro/Seleção, dica do conta-gotas e pincéis: antes ficavam
    // só em português (usuário em inglês via texto PT).
    if (m_artisticMenu)
        m_artisticMenu->setTitle(m_i18n->t("editor", "filters.artistic"));
    if (m_toneMenu)
        m_toneMenu->setTitle(m_i18n->t("editor", "filters.tone"));
    {
        static const char* const filterKeys[] = {
            "filters.grayscale", "filters.sepia", "filters.vintage",
            "filters.highContrast", "filters.vividPop", "filters.cyberpunk",
            "filters.goldenHour", "filters.cinemaNoir", "filters.invert",
            "filters.posterize", "filters.threshold",
        };
        const int filterCount = int(sizeof(filterKeys) / sizeof(filterKeys[0]));
        for (int i = 0; i < m_filterActions.size() && i < filterCount; ++i) {
            if (m_filterActions[i])
                m_filterActions[i]->setText(
                    m_i18n->t("editor", QString::fromLatin1(filterKeys[i])));
        }
    }
    if (m_deselectAction)
        m_deselectAction->setText(m_i18n->t("editor", "selection.deselect"));
    if (m_invertSelectionAction)
        m_invertSelectionAction->setText(m_i18n->t("editor", "selection.invert"));
    if (m_selectionOpsAction) {
        m_selectionOpsAction->setText(m_i18n->t("editor", "selection.operations"));
        m_selectionOpsAction->setToolTip(
            m_i18n->t("editor", "selection.operationsTooltip"));
    }
    // Select > Modify e Color Range (mesma ordem de m_modifyActions)
    if (m_modifyMenu)
        m_modifyMenu->setTitle(m_i18n->t("editor", "selection.modify.title"));
    {
        static const char* const modifyKeys[] = {
            "selection.modify.expand", "selection.modify.contract",
            "selection.modify.border", "selection.modify.smooth",
            "selection.modify.feather",
        };
        const int keyCount = int(sizeof(modifyKeys) / sizeof(modifyKeys[0]));
        for (int i = 0; i < m_modifyActions.size() && i < keyCount; ++i) {
            if (m_modifyActions[i])
                m_modifyActions[i]->setText(
                    m_i18n->t("editor", QString::fromLatin1(modifyKeys[i])));
        }
    }
    if (m_colorRangeAction)
        m_colorRangeAction->setText(m_i18n->t("editor", "selection.colorRange.title"));
    if (m_toggleGridAction) {
        m_toggleGridAction->setText(m_i18n->t("common", "menu.view.showGrid"));
        m_toggleGridAction->setToolTip(m_toggleGridAction->text() + QStringLiteral(" (Ctrl+')"));
    }
    if (m_toggleSnapAction) {
        m_toggleSnapAction->setText(m_i18n->t("common", "menu.view.snapToGrid"));
        m_toggleSnapAction->setToolTip(m_toggleSnapAction->text());
    }
    if (m_toggleHistoryAction) {
        m_toggleHistoryAction->setText(m_i18n->t("common", "menu.view.history"));
        m_toggleHistoryAction->setToolTip(m_toggleHistoryAction->text() + QStringLiteral(" (Ctrl+H)"));
    }
    if (m_panelsMenu)
        m_panelsMenu->setTitle(m_i18n->t("common", "menu.view.panels"));
    if (m_toggleToolsBarAction) {
        m_toggleToolsBarAction->setText(m_i18n->t("common", "menu.view.toggleToolsBar"));
        m_toggleToolsBarAction->setToolTip(m_toggleToolsBarAction->text() + QStringLiteral(" (Ctrl+B)"));
    }
    if (m_toggleRightPanelAction) {
        m_toggleRightPanelAction->setText(m_i18n->t("common", "menu.view.toggleRightPanel"));
        m_toggleRightPanelAction->setToolTip(m_toggleRightPanelAction->text() + QStringLiteral(" (Ctrl+J)"));
    }
    if (m_toggleLayersDockAction) {
        m_toggleLayersDockAction->setText(m_i18n->t("common", "panel.layers"));
        m_toggleLayersDockAction->setToolTip(m_toggleLayersDockAction->text());
    }
    if (m_toggleTextDockAction) {
        m_toggleTextDockAction->setText(m_i18n->t("editor", "text.title"));
        m_toggleTextDockAction->setToolTip(m_toggleTextDockAction->text());
    }
    if (m_toggleShapeDockAction) {
        m_toggleShapeDockAction->setText(m_i18n->t("editor", "shape.title"));
        m_toggleShapeDockAction->setToolTip(m_toggleShapeDockAction->text());
    }
    if (m_toggleImageDockAction) {
        m_toggleImageDockAction->setText(m_i18n->t("common", "panel.image"));
        m_toggleImageDockAction->setToolTip(m_toggleImageDockAction->text());
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
    m_rotateLeftAction->setText(m_i18n->t("common", "menu.layer.rotateLeft"));
    m_rotateRightAction->setText(m_i18n->t("common", "menu.layer.rotateRight"));
    m_deleteAction->setText(m_i18n->t("common", "menu.layer.delete"));
    if (m_textBehindSubjectAction)
        m_textBehindSubjectAction->setText(m_i18n->currentLanguage() == QStringLiteral("pt-BR")
            ? QStringLiteral("Texto atrás do personagem")
            : QStringLiteral("Text behind subject"));
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
    if (m_openRecentMenu)
        m_openRecentMenu->setTitle(m_i18n->t("common", "menu.file.openRecent"));
    m_editMenu->setTitle(m_i18n->t("common", "menu.edit"));
    if (m_imageMenu) m_imageMenu->setTitle(pt ? QStringLiteral("Imagem") : QStringLiteral("Image"));
    if (m_selectMenu) m_selectMenu->setTitle(pt ? QStringLiteral("Selecionar") : QStringLiteral("Select"));
    if (m_filterMenu) m_filterMenu->setTitle(pt ? QStringLiteral("Filtro") : QStringLiteral("Filter"));
    if (m_windowMenu) m_windowMenu->setTitle(pt ? QStringLiteral("Janela") : QStringLiteral("Window"));
    m_layerMenu->setTitle(m_i18n->t("common", "menu.layer"));
    if (m_addFillLayerAction)
        m_addFillLayerAction->setText(m_i18n->t("editor", "layer.newFillLayer"));
    if (m_addAdjustmentLayerAction)
        m_addAdjustmentLayerAction->setText(m_i18n->t("editor", "layer.newAdjustmentLayer"));
    m_settingsMenu->setTitle(m_i18n->t("common", "menu.settings"));
    m_helpMenu->setTitle(m_i18n->t("common", "menu.help"));

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
    if (m_toolEyedropperAction) {
        const QString eyedropperText = m_i18n->currentLanguage() == QStringLiteral("pt-BR") ? QStringLiteral("Conta-gotas") : QStringLiteral("Eyedropper");
        m_toolEyedropperAction->setText(eyedropperText);
        m_toolEyedropperAction->setToolTip(QStringLiteral("%1 (I)").arg(eyedropperText));
    }


    if (m_addTextAction) {
        m_addTextAction->setToolTip(QStringLiteral("%1 (T)").arg(m_i18n->t("common", "menu.layer.addText")));
    }
    if (m_addRectAction) {
        m_addRectAction->setToolTip(QStringLiteral("%1 (U)").arg(m_i18n->t("common", "menu.layer.shape.rect")));
    }

    if (m_toolsMenu)
        m_toolsMenu->setTitle(m_i18n->t("editor", "tools.title"));
    if (m_selectionToolsMenu)
        m_selectionToolsMenu->setTitle(m_i18n->t("common", "menu.tools.selection"));
    if (m_retouchToolsMenu)
        m_retouchToolsMenu->setTitle(m_i18n->t("common", "menu.tools.retouch"));
    if (m_createToolsMenu)
        m_createToolsMenu->setTitle(m_i18n->t("common", "menu.tools.create"));

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
