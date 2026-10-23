#include "canvasview.h"

#include "aibackgrounddialog.h"
#include "core/image/BackgroundRemover.h"
#include "core/image/ImageProcessing.h"
#include "core/history/DocumentCommands.h"
#include "core/selection/SelectionOperations.h"
#include "core/history/CommandStack.h"
#include "rendering/CanvasRenderer.h"
#include "tools/SelectTool.h"
#include "tools/MarqueeTool.h"
#include "tools/PaintExtras.h"
#include "tools/CropTool.h"
#include "tools/ScissorsTool.h"
#include "tools/PaintTool.h"
#include "tools/RasterTools.h"
#include "designtokens.h"
#include <QThread>

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLineF>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QUrl>
#include <QWheelEvent>

#include "localization/i18nservice.h"

#include <algorithm>
#include <cmath>

namespace cc {
namespace {

QImage composeAlphaMask(const QImage& existing, const QImage& operation)
{
    const QImage op = operation.convertToFormat(QImage::Format_Alpha8);
    if (existing.isNull() || existing.size() != op.size())
        return op;
    QImage result = existing.convertToFormat(QImage::Format_Alpha8);
    for (int y = 0; y < result.height(); ++y) {
        uchar* dst = result.scanLine(y);
        const uchar* src = op.constScanLine(y);
        for (int x = 0; x < result.width(); ++x)
            dst[x] = static_cast<uchar>((int(dst[x]) * int(src[x])) / 255);
    }
    return result;
}

QImage alphaMaskFromImage(const QImage& image)
{
    if (image.isNull())
        return {};
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    QImage mask(rgba.size(), QImage::Format_Alpha8);
    for (int y = 0; y < rgba.height(); ++y) {
        const uchar* src = rgba.constScanLine(y);
        uchar* dst = mask.scanLine(y);
        for (int x = 0; x < rgba.width(); ++x)
            dst[x] = src[x * 4 + 3];
    }
    return mask;
}

constexpr double kMinZoom = 0.05;
constexpr double kMaxZoom = 32.0;
constexpr double kZoomStep = 1.2;
constexpr double kHandleRadius = 7.0;
constexpr double kRotateOffset = 26.0;

Qt::CursorShape handleCursor(int handle)
{
    switch (handle) {
    case 0: case 2: return Qt::SizeFDiagCursor;
    case 1: case 3: return Qt::SizeBDiagCursor;
    case 4: case 6: return Qt::SizeVerCursor;
    case 5: case 7: return Qt::SizeHorCursor;
    case 8:         return Qt::CrossCursor;
    }
    return Qt::ArrowCursor;
}

} // namespace

CanvasView::CanvasView(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);

    m_selectTool = std::make_unique<SelectTool>();
    m_cropTool = std::make_unique<CropTool>();
    m_scissorsTool = std::make_unique<ScissorsTool>();
    m_paintTool = std::make_unique<PaintTool>();
    m_wandTool = std::make_unique<MagicWandTool>();
    m_cloneTool = std::make_unique<CloneStampTool>();
    m_floodTool = std::make_unique<FloodFillTool>();
    m_marqueeTool = std::make_unique<MarqueeTool>();
    m_gradientTool = std::make_unique<GradientTool>();
    m_retouchTool = std::make_unique<RetouchTool>();
    m_healingTool = std::make_unique<HealingTool>();
    m_activeTool = m_selectTool.get();
}

ToolContext CanvasView::makeToolContext() const
{
    ToolContext ctx;
    ctx.document = m_document.data();
    ctx.history = m_history;
    ctx.snapEngine = const_cast<SnapEngine*>(&m_snapEngine);
    ctx.i18n = m_i18n;
    ctx.view = const_cast<CanvasView*>(this);
    ctx.zoom = m_zoom;
    ctx.panOffset = m_panOffset;
    ctx.docToDevice = docToDevice();
    ctx.deviceToDoc = deviceToDoc();
    ctx.showGrid = m_showGrid;
    ctx.snapToGrid = m_snapToGrid;
    ctx.gridSpacing = m_gridSpacing;
    ctx.safeZoneMode = m_safeZoneMode;
    ctx.selectedLayerId = m_selectedId;
    ctx.selectedLayerIds = m_multiSelection;
    ctx.requestUpdate = [this] { const_cast<CanvasView*>(this)->update(); };
    ctx.requestStatusMessage = [this](const QString& msg) {
        emit const_cast<CanvasView*>(this)->statusMessageRequested(msg);
    };
    ctx.selectLayer = [this](const LayerId& id) {
        const_cast<CanvasView*>(this)->selectLayer(id);
    };
    ctx.switchTool = [this](CanvasTool t) {
        const_cast<CanvasView*>(this)->setTool(t);
    };
    ctx.setLayerPreview = [this](const LayerId& id, const QImage& img) {
        const_cast<CanvasView*>(this)->setImagePreview(id, img);
    };
    ctx.clearLayerPreview = [this]() {
        const_cast<CanvasView*>(this)->clearImagePreview();
    };
    return ctx;
}

CanvasView::~CanvasView()
{
    if (m_quickAiCancelFlag) {
        m_quickAiCancelFlag->store(true);
    }
    if (m_quickAiThread && m_quickAiThread->isRunning()) {
        m_quickAiThread->wait(1000);
    }
}

void CanvasView::setDocument(Document* document)
{
    if (m_document == document)
        return;
    hideTextEdit();
    m_document = document;
    m_selectedId = LayerId();
    m_needsFit = true;
    update();
}

void CanvasView::setI18n(I18nService* i18n)
{
    m_i18n = i18n;
}

void CanvasView::setHistory(CommandStack* history)
{
    m_history = history;
}

void CanvasView::clearSelection()
{
    if (m_textEditor && m_textEditor->isVisible())
        commitTextEdit();
    selectLayer(LayerId());
}

void CanvasView::pruneSelection()
{
    if (!m_document)
        return;
    bool changed = false;
    if (!m_selectedId.isNull() && !m_document->findLayer(m_selectedId)) {
        m_selectedId = LayerId();
        changed = true;
    }
    for (int i = m_multiSelection.size() - 1; i >= 0; --i) {
        if (!m_document->findLayer(m_multiSelection.at(i))) {
            m_multiSelection.removeAt(i);
            changed = true;
        }
    }
    if (changed)
        update();
}

void CanvasView::setSelectedLayer(const LayerId& id)
{
    selectLayer(id);
}

void CanvasView::beginTextEdit(const LayerId& id)
{
    if (!m_document)
        return;
    Layer* layer = m_document->findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return;

    selectLayer(id);
    m_editingTextId = id;
    auto* textLayer = static_cast<TextLayer*>(layer);

    if (!m_textEditor) {
        m_textEditor = new QLineEdit(this);
        m_textEditor->installEventFilter(this);
        connect(m_textEditor, &QLineEdit::returnPressed,
                this, [this] { commitTextEdit(); });
        connect(m_textEditor, &QLineEdit::editingFinished,
                this, [this] { commitTextEdit(); });
    }

    // Position the editor over the text using the SAME math as the
    // selection overlay.
    const HandleSet set = handlePositions(*layer);
    if (!set.valid)
        return;
    const QRectF screenRect = QRectF(set.points[0], set.points[2])
                                  .normalized()
                                  .united(QRectF(set.points[3], set.points[1])
                                              .normalized());

    // WYSIWYG: style the editor with the text's own attributes (a widget
    // with a stylesheet resolves its font from the STYLE, not setFont).
    const QString hex = textLayer->color.name(QColor::HexRgb);
    const double px = qMax(6.0, textLayer->sizePt * (96.0 / 72.0) * m_zoom);
    // Light text on a light editor backdrop is unreadable while typing:
    // darken the editor background when the text color is light.
    const QColor textColor = textLayer->color;
    const double luminance = 0.299 * textColor.red()
                           + 0.587 * textColor.green()
                           + 0.114 * textColor.blue();
    const QString editorBg = luminance > 150 ? QStringLiteral("#26272b")
                                             : QStringLiteral("white");
    m_textEditor->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %7; color: %1;"
        " border: 1px solid #2f6fed; padding: 0px;"
        " font-family: \"%2\"; font-size: %3px;"
        " font-weight: %4; font-style: %5; text-decoration: %6; }"
        "QLineEdit { selection-background-color: #2f6fed;"
        " selection-color: %1; }")
        .arg(hex)
        .arg(textLayer->fontFamily)
        .arg(QString::number(px, 'f', 0))
        .arg(textLayer->bold ? QStringLiteral("bold")
                             : QStringLiteral("normal"))
        .arg(textLayer->italic ? QStringLiteral("italic")
                               : QStringLiteral("normal"))
        .arg(textLayer->underline ? QStringLiteral("underline")
                                  : QStringLiteral("none"))
        .arg(editorBg));

    m_textEditor->setAlignment(
        textLayer->align == TextAlignment::Center
            ? Qt::AlignCenter
            : (textLayer->align == TextAlignment::Right ? Qt::AlignRight
                                                        : Qt::AlignLeft));

    m_textEditor->setGeometry(screenRect.toRect().adjusted(-2, -2, 2, 2));
    m_textEditor->setText(textLayer->content);
    m_textEditor->show();
    m_textEditor->raise();
    m_textEditor->setFocus();
    m_textEditor->selectAll();
}

void CanvasView::commitTextEdit()
{
    if (!m_textEditor || !m_textEditor->isVisible() || !m_document)
        return;
    Layer* layer = m_document->findLayer(m_editingTextId);
    if (!layer || layer->type() != LayerType::Text) {
        m_textEditor->hide();
        return;
    }
    const QString oldContent = static_cast<TextLayer*>(layer)->content;
    const QString newContent = m_textEditor->text();
    m_textEditor->hide();
    if (newContent == oldContent)
        return;
    m_document->setLayerTextContent(m_editingTextId, newContent);
    emit textCommitted(m_editingTextId, oldContent, newContent);
}

void CanvasView::hideTextEdit()
{
    if (m_textEditor)
        m_textEditor->hide();
    m_editingTextId = LayerId();
}

void CanvasView::centerOn(const QPointF& documentPos)
{
    m_panOffset = QPointF(width() / 2.0, height() / 2.0)
                  - documentPos * m_zoom;
    update();
}


void CanvasView::selectLayer(const LayerId& id)
{
    if (m_textEditor && m_textEditor->isVisible() && id != m_editingTextId)
        commitTextEdit();
    if (m_selectedId == id)
        return;
    m_selectedId = id;

    if (m_tool == CanvasTool::Crop && m_cropTool) {
        m_cropTool->initCropRect(makeToolContext());
    }

    emit selectionChanged(m_selectedId);
    update();
}

QList<LayerId> CanvasView::selectedLayers() const
{
    if (m_multiSelection.isEmpty() && !m_selectedId.isNull())
        return {m_selectedId};
    return m_multiSelection;
}

void CanvasView::addToSelection(const LayerId& id)
{
    if (id.isNull()) return;
    if (m_multiSelection.isEmpty() && !m_selectedId.isNull())
        m_multiSelection.append(m_selectedId);
    if (!m_multiSelection.contains(id))
        m_multiSelection.append(id);
    m_selectedId = id;
    emit selectionChanged(m_selectedId);
    emit multiSelectionChanged(m_multiSelection);
    update();
}

void CanvasView::removeFromSelection(const LayerId& id)
{
    m_multiSelection.removeAll(id);
    if (m_selectedId == id) {
        m_selectedId = m_multiSelection.isEmpty() ? LayerId() : m_multiSelection.last();
        emit selectionChanged(m_selectedId);
    }
    if (m_multiSelection.size() <= 1)
        m_multiSelection.clear();
    emit multiSelectionChanged(m_multiSelection);
    update();
}

void CanvasView::setMultiSelection(const QList<LayerId>& ids)
{
    m_multiSelection = ids;
    if (!ids.isEmpty())
        m_selectedId = ids.last();
    else
        m_selectedId = LayerId();
    emit selectionChanged(m_selectedId);
    emit multiSelectionChanged(m_multiSelection);
    update();
}

namespace {
// Coleta recursivamente as camadas folha (desce em grupos) que intersectam a
// área de seleção — rubber band também tem que achar formas dentro de grupos.
void collectRubberBandHits(const GroupLayer& group, const QRectF& docRect,
                           QList<Layer*>& result)
{
    for (const auto& child : group.children) {
        Layer* layer = child.get();
        if (!layer->visible || layer->locked) continue;
        if (layer->type() == LayerType::Background) continue;
        if (layer->type() == LayerType::Group) {
            collectRubberBandHits(static_cast<const GroupLayer&>(*layer),
                                  docRect, result);
            continue;
        }
        const QRectF bounds = layer->contentBounds();
        if (bounds.isEmpty()) continue;
        const QRectF layerDocRect = layer->transform.matrix(bounds).mapRect(bounds);
        if (docRect.intersects(layerDocRect))
            result.append(layer);
    }
}
} // namespace

QList<Layer*> CanvasView::hitTestRubberBand(const QRectF& docRect) const
{
    QList<Layer*> result;
    if (!m_document) return result;
    collectRubberBandHits(*m_document->rootGroup(), docRect, result);
    return result;
}

void CanvasView::setShowGrid(bool show) { m_showGrid = show; update(); }
void CanvasView::setSnapToGrid(bool snap) { m_snapToGrid = snap; }
void CanvasView::setGridSpacing(int spacing) { m_gridSpacing = std::clamp(spacing, 10, 200); update(); }
void CanvasView::setSafeZoneMode(int mode) { m_safeZoneMode = std::clamp(mode, 0, 3); update(); }

void CanvasView::zoomIn()
{
    zoomAt(QPointF(width() / 2.0, height() / 2.0), kZoomStep);
}

void CanvasView::zoomOut()
{
    zoomAt(QPointF(width() / 2.0, height() / 2.0), 1.0 / kZoomStep);
}

void CanvasView::zoomTo(double zoom)
{
    zoom = std::clamp(zoom, kMinZoom, kMaxZoom);
    if (qFuzzyCompare(zoom, m_zoom))
        return;
    zoomAt(QPointF(width() / 2.0, height() / 2.0), zoom / m_zoom);
}

void CanvasView::fitToViewport()
{
    if (!m_document || width() <= 0 || height() <= 0)
        return;

    constexpr double kMarginFactor = 0.92;
    const double zoomX = width() * kMarginFactor / m_document->width();
    const double zoomY = height() * kMarginFactor / m_document->height();
    m_zoom = std::clamp(std::min(zoomX, zoomY), kMinZoom, kMaxZoom);
    m_panOffset = QPointF(
        (width() - m_document->width() * m_zoom) / 2.0,
        (height() - m_document->height() * m_zoom) / 2.0);
    m_needsFit = false;
    m_autoFit = true;

    update();
    emitZoomChanged();
}

QTransform CanvasView::docToDevice() const
{
    QTransform transform;
    transform.translate(m_panOffset.x(), m_panOffset.y());
    transform.scale(m_zoom, m_zoom);
    return transform;
}

QTransform CanvasView::deviceToDoc() const
{
    return docToDevice().inverted();
}

void CanvasView::zoomAt(const QPointF& widgetPos, double factor)
{
    const double newZoom = std::clamp(m_zoom * factor, kMinZoom, kMaxZoom);
    if (qFuzzyCompare(newZoom, m_zoom))
        return;

    // Zoom manual: o usuário assumiu o controle — resize não refaz o fit.
    m_autoFit = false;

    const double effective = newZoom / m_zoom;
    m_panOffset = widgetPos - (widgetPos - m_panOffset) * effective;
    m_zoom = newZoom;

    update();
    emitZoomChanged();
}

void CanvasView::emitZoomChanged()
{
    emit zoomChanged(m_zoom);
}

// Amostra o alfa efetivo (imagem * mascara alfa) nas coordenadas naturais
// da camada. Retorna true se o pixel for praticamente transparente — nesses
// pontos o clique deve ATRAVESSAR a imagem e acertar o que está por baixo.
bool CanvasView::imagePixelTransparent(const ImageLayer& image,
                                        const QPointF& local) const
{
    if (image.naturalWidth <= 0 || image.naturalHeight <= 0)
        return false;
    if (!m_document)
        return false;
    const QImage src = m_document->assets().decodedImage(image.assetId);
    if (src.isNull())
        return false; // sem bitmap: mantém o comportamento de retângulo

    // Limite contra as dimensões REAIS do bitmap decodificado, não contra
    // naturalWidth/naturalHeight salvos no projeto: um arquivo editado à mão
    // ou corrompido pode divergir, e indexar scanLine fora dos limites é OOB.
    const int maxX = qMin(image.naturalWidth, src.width()) - 1;
    const int maxY = qMin(image.naturalHeight, src.height()) - 1;
    if (maxX < 0 || maxY < 0)
        return false;
    const int ix = qBound(0, static_cast<int>(std::floor(local.x())), maxX);
    const int iy = qBound(0, static_cast<int>(std::floor(local.y())), maxY);

    // Fast-path 32bpp: leitura direta do pixel, sem conversão nem cópia.
    int alpha = 255;
    if (src.depth() == 32) {
        const QRgb* row = reinterpret_cast<const QRgb*>(src.constScanLine(iy));
        alpha = qAlpha(row[ix]);
    } else {
        alpha = qAlpha(src.pixelColor(ix, iy).rgba());
    }

    // Máscara alfa não-destrutiva também esconde o pixel
    if (!image.alphaMask.isNull()) {
        const QImage& mask = image.alphaMask;
        if (mask.valid(ix, iy)) {
            const int m = mask.format() == QImage::Format_Alpha8
                             ? mask.constScanLine(iy)[ix]
                             : qAlpha(mask.pixel(ix, iy));
            alpha = alpha * m / 255;
        }
    }

    // Limiar ~6%: abaixo disso consideramos "furado"
    return alpha < 16;
}

// Percorre os filhos de um grupo do TOPO para o FUNDO (o de cima ganha),
// DESCENDO dentro de subgrupos: formas dentro de grupos também recebem
// clique e menu de contexto. Grupos não interceptam cliques por si — só os
// filhos visíveis.
Layer* CanvasView::hitTestChildrenRecursive(const GroupLayer& group,
                                            const QPointF& docPos) const
{
    for (auto it = group.children.rbegin(); it != group.children.rend(); ++it) {
        Layer* layer = it->get();
        if (!layer->visible || layer->locked)
            continue;
        if (layer->opacity() <= 0.01f)
            continue; // invisível de fato não captura cliques
        if (layer->type() == LayerType::Background)
            continue;
        if (layer->type() == LayerType::Group) {
            if (Layer* child = hitTestChildrenRecursive(
                    static_cast<const GroupLayer&>(*layer), docPos))
                return child;
            continue; // clicou fora dos filhos: o grupo não captura
        }
        const QRectF bounds = layer->contentBounds();
        if (bounds.isEmpty())
            continue;
        const QPointF local = layer->transform.matrix(bounds).inverted().map(docPos);
        if (!bounds.contains(local))
            continue;

        // Imagens são testadas POR PIXEL: clicar numa área transparente de
        // uma imagem (ex.: cantos do recorte da IA) seleciona a camada de
        // baixo, não a imagem inteira.
        if (layer->type() == LayerType::Image) {
            if (imagePixelTransparent(static_cast<const ImageLayer&>(*layer), local))
                continue;
        }
        return layer;
    }
    return nullptr;
}

Layer* CanvasView::hitTestLayer(const QPointF& docPos) const
{
    if (!m_document)
        return nullptr;
    return hitTestChildrenRecursive(*m_document->rootGroup(), docPos);
}

CanvasView::HandleSet CanvasView::handlePositions(const Layer& layer) const
{
    HandleSet set;
    if (!m_document)
        return set;
    const QRectF bounds = layer.contentBounds();
    if (bounds.isEmpty())
        return set;

    const QTransform toScreen = layer.transform.matrix(bounds) * docToDevice();
    set.points[0] = toScreen.map(bounds.topLeft());
    set.points[1] = toScreen.map(bounds.topRight());
    set.points[2] = toScreen.map(bounds.bottomRight());
    set.points[3] = toScreen.map(bounds.bottomLeft());
    set.points[4] = (set.points[0] + set.points[1]) / 2;
    set.points[5] = (set.points[1] + set.points[2]) / 2;
    set.points[6] = (set.points[2] + set.points[3]) / 2;
    set.points[7] = (set.points[3] + set.points[0]) / 2;
    set.points[8] = set.points[4] + QPointF(0, -kRotateOffset);
    set.valid = true;
    return set;
}

int CanvasView::handleAt(const QPointF& widgetPos) const
{
    if (m_selectedId.isNull() || !m_document)
        return -1;
    const Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer)
        return -1;
    const HandleSet set = handlePositions(*layer);
    if (!set.valid)
        return -1;
    for (int i = 0; i < 9; ++i)
        if (QLineF(widgetPos, set.points[i]).length() <= kHandleRadius)
            return i;
    return -1;
}

void CanvasView::drawSelectionOverlay(QPainter* painter)
{
    Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer)
        return;
    const HandleSet set = handlePositions(*layer);
    if (!set.valid)
        return;

    painter->save();
    painter->resetTransform();
    const QColor accent = Design::Accent();

    painter->setPen(QPen(accent, 1.0));
    painter->setBrush(Qt::NoBrush);
    QPolygonF box;
    box << set.points[0] << set.points[1] << set.points[2] << set.points[3];
    painter->drawPolygon(box);

    painter->setPen(QPen(accent, 1.0));
    for (int i = 0; i < 8; ++i) {
        const QRectF rect(set.points[i] - QPointF(3, 3), QSizeF(6, 6));
        painter->setBrush(Qt::white);
        painter->drawRoundedRect(rect, 2.0, 2.0);
    }
    painter->setBrush(accent);
    painter->drawEllipse(set.points[8], 4, 4);
    painter->restore();
}

void CanvasView::updateCursor(const QPointF& widgetPos)
{
    if (m_panning || m_gesture != Gesture::None)
        return;

    if (m_tool == CanvasTool::Crop && m_cropTool) {
        const int ch = m_cropTool->cropHandleAt(widgetPos, makeToolContext());
        setCursor(ch >= 0 ? (ch == 8 ? Qt::SizeAllCursor : handleCursor(ch)) : Qt::CrossCursor);
        return;
    }

    if (m_tool == CanvasTool::Scissors || m_tool == CanvasTool::MagicWand
        || m_tool == CanvasTool::CloneStamp || m_tool == CanvasTool::Paint
        || m_tool == CanvasTool::FloodFill) {
        setCursor(Qt::CrossCursor);
        return;
    }

    const int handle = handleAt(widgetPos);
    setCursor(handle >= 0 ? handleCursor(handle) : Qt::ArrowCursor);
}

void CanvasView::paintEvent(QPaintEvent*)
{
    if (m_needsFit && m_document && width() > 0 && height() > 0)
        fitToViewport();

    QPainter painter(this);
    const bool dark = palette().color(QPalette::Window).value() < 128;
    painter.fillRect(rect(), dark ? Design::DWindow() : Design::LWindow());

    if (!m_document)
        return;

    painter.save();
    painter.resetTransform();
    const QRectF canvasOnDevice = docToDevice().mapRect(
        QRectF(0, 0, m_document->width(), m_document->height()));
    for (int spread = 8; spread >= 2; spread -= 2) {
        const int alpha = 10 + (8 - spread) * 3;
        painter.fillRect(canvasOnDevice.adjusted(-spread, -spread, spread, spread),
                         QColor(0, 0, 0, alpha));
    }
    painter.restore();

    RenderOptions options;
    options.previewLayerId = m_previewLayerId;
    options.previewImage = m_previewImage;
    options.interactive = (m_gesture != Gesture::None) || m_panning;
    options.checkerLight = dark ? Design::Base3() : QColor(0xe5, 0xe9, 0xef);
    options.checkerDark = dark ? Design::Base2() : QColor(0xff, 0xff, 0xff);
    options.canvasBorder = dark ? Design::DBorder() : Design::LBorder();
    renderDocument(*m_document, &painter, docToDevice(), options);

    // Mostra a seleção de pixels criada pela Varinha/Tesoura sem alterar a
    // camada original. A sobreposição vermelha permite confirmar a área antes
    // de aplicar uma máscara.
    const QImage& pixelSelection = m_document->pixelSelection();
    if (!pixelSelection.isNull()) {
        // A seleção da varinha é guardada em coordenadas da própria imagem,
        // não em coordenadas do documento. Desenhá-la como se fosse um bitmap
        // do documento fazia com que ela desaparecesse (e parecia que a
        // imagem tinha ficado transparente). Mostramos agora a área que será
        // removida, transformada exatamente como a camada selecionada.
        Layer* selectionLayer = m_selectedId.isNull() ? nullptr : m_document->findLayer(m_selectedId);
        if (selectionLayer && selectionLayer->type() == LayerType::Image) {
            auto* imageLayer = static_cast<ImageLayer*>(selectionLayer);
            const QImage source = m_document->assets().decodedImage(imageLayer->assetId);
            if (!source.isNull() && pixelSelection.size() == source.size()) {
                // Use a non-premultiplied image for the marching-ants mask;
                // writing straight qRgba values into ARGB32_Premultiplied
                // produces neon colour artifacts.
                QImage selectionOverlay(pixelSelection.size(), QImage::Format_ARGB32);
                selectionOverlay.fill(Qt::transparent);
                for (int y = 0; y < pixelSelection.height(); ++y) {
                    QRgb* dst = reinterpret_cast<QRgb*>(selectionOverlay.scanLine(y));
                    const uchar* src = pixelSelection.constScanLine(y);
                    for (int x = 0; x < pixelSelection.width(); ++x) {
                        // Draw only the boundary of the selected region. A
                        // full red fill looked like the image had become
                        // transparent, especially on large backgrounds.
                        const uchar here = src[x];
                        const uchar left = x > 0 ? src[x - 1] : here;
                        const uchar right = x + 1 < pixelSelection.width() ? src[x + 1] : here;
                        const uchar up = y > 0 ? pixelSelection.constScanLine(y - 1)[x] : here;
                        const uchar down = y + 1 < pixelSelection.height() ? pixelSelection.constScanLine(y + 1)[x] : here;
                        const bool edge = qAbs(int(here) - int(left)) > 20
                                       || qAbs(int(here) - int(right)) > 20
                                       || qAbs(int(here) - int(up)) > 20
                                       || qAbs(int(here) - int(down)) > 20;
                        // Photo editors show a selection as a black/white
                        // dashed boundary, not as a coloured fill.
                        if (edge)
                            dst[x] = ((x + y) / 3) % 2
                                ? qRgba(255, 255, 255, 255)
                                : qRgba(0, 0, 0, 255);
                    }
                }
                // Keep the same multiplication order used by the renderer
                // and the layer handles: layer transform, then canvas view
                // transform. Reversing these offsets the selection when the
                // layer is moved or scaled.
                painter.setTransform(imageLayer->transform.matrix(imageLayer->contentBounds()) * docToDevice());
                painter.drawImage(QPointF(0, 0), selectionOverlay);
                painter.setTransform(QTransform());
            }
        }
    }

    if (m_activeTool) {
        m_activeTool->drawOverlay(&painter, makeToolContext());
    }

    if (m_tool == CanvasTool::Select) {
        if (!m_selectedId.isNull())
            drawSelectionOverlay(&painter);

        if (m_multiSelection.size() > 1)
            drawMultiSelectionOverlay(&painter);

        if (m_rubberBanding)
            drawRubberBand(&painter);
    }

    if (m_showGrid)
        drawGridOverlay(&painter);

    if (m_safeZoneMode > 0)
        drawSafeZoneOverlay(&painter);

    if (m_mockupPlacementActive)
        drawMockupPlacementOverlay(&painter);

    drawSnapGuides(&painter);
}

void CanvasView::setImagePreview(const LayerId& id, const QImage& image)
{
    m_previewLayerId = id;
    m_previewImage = image;
    update();
}

void CanvasView::clearImagePreview()
{
    m_previewLayerId = LayerId();
    m_previewImage = QImage();
    update();
}

void CanvasView::wheelEvent(QWheelEvent* event)
{
    const double delta = event->angleDelta().y();
    if (delta == 0) {
        event->ignore();
        return;
    }
    zoomAt(event->position(), std::pow(kZoomStep, delta / 120.0));
    event->accept();
}

void CanvasView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton
        || (event->button() == Qt::LeftButton && m_spacePanning)) {
        m_panning = true;
        m_autoFit = false;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (!m_document) {
        event->ignore();
        return;
    }

    const QPointF docPos = deviceToDoc().map(QPointF(event->pos()));

    if (m_mockupPlacementActive) {
        if (event->button() == Qt::RightButton) {
            cancelMockupPlacement();
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton) {
            m_mockupCorners << docPos;
            update();
            event->accept();
            return;
        }
    }

    if (m_tool == CanvasTool::Eyedropper || ((event->modifiers() & Qt::AltModifier) && event->button() == Qt::LeftButton)) {
        const QColor sampled = sampleColorAt(docPos);
        if (sampled.isValid()) {
            emit colorPicked(sampled);
        }
        event->accept();
        return;
    }

    if (m_tool != CanvasTool::Select) {
        if (m_activeTool) {
            m_activeTool->mousePress(event, docPos, makeToolContext());
        }
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const int handle = handleAt(event->pos());
        if (handle >= 0) {
            if (Layer* layer = m_document->findLayer(m_selectedId)) {
                m_gestureStart = layer->transform;
                m_gestureBounds = layer->contentBounds();
                if (layer->type() == LayerType::Text)
                    m_gestureStartBox = static_cast<TextLayer*>(layer)->box;
                else
                    m_gestureStartBox = QSizeF();
                const QTransform matrix = m_gestureStart.matrix(m_gestureBounds);
                m_gestureStartDoc = docPos;
                m_localPress = matrix.inverted().map(docPos);

                if (handle == 8) {
                    m_rotateCenter = matrix.map(m_gestureBounds.center());
                    m_rotateStartAngle = std::atan2(
                        m_gestureStartDoc.y() - m_rotateCenter.y(),
                        m_gestureStartDoc.x() - m_rotateCenter.x());
                    m_gesture = Gesture::Rotate;
                } else {
                    m_activeHandle = handle;
                    static const int opposite[8] = {2, 3, 0, 1, 6, 7, 4, 5};
                    const QRectF b = m_gestureBounds;
                    switch (opposite[m_activeHandle]) {
                    case 0: m_fixedLocal = b.topLeft(); break;
                    case 1: m_fixedLocal = b.topRight(); break;
                    case 2: m_fixedLocal = b.bottomRight(); break;
                    case 3: m_fixedLocal = b.bottomLeft(); break;
                    case 4: m_fixedLocal = (b.topLeft() + b.topRight()) / 2; break;
                    case 5: m_fixedLocal = (b.topRight() + b.bottomRight()) / 2; break;
                    case 6: m_fixedLocal = (b.bottomRight() + b.bottomLeft()) / 2; break;
                    default: m_fixedLocal = (b.bottomLeft() + b.topLeft()) / 2; break;
                    }
                    m_fixedDoc = matrix.map(m_fixedLocal);
                    m_gesture = Gesture::Scale;
                }
                event->accept();
                return;
            }
        }

        if (Layer* layer = hitTestLayer(docPos)) {
            // Shift OU Ctrl adicionam/alternam a seleção (padrão de editores)
            if (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) {
                if (m_multiSelection.contains(layer->id()))
                    removeFromSelection(layer->id());
                else
                    addToSelection(layer->id());
            } else {
                // Um clique normal seleciona apenas o objeto clicado. A
                // seleção múltipla só permanece quando o utilizador segura
                // Shift, evitando que camadas antigas se movam juntas.
                m_multiSelection.clear();
                selectLayer(layer->id());
            }
            m_gestureStart = layer->transform;
            m_gestureStartDoc = docPos;
            if (m_multiSelection.size() > 1) {
                m_multiGestureStarts.clear();
                for (const auto& lid : m_multiSelection) {
                    if (Layer* l = m_document->findLayer(lid))
                        m_multiGestureStarts.append(l->transform);
                    else
                        m_multiGestureStarts.append(AffineTransform());
                }
            }
            m_gesture = Gesture::Move;
            event->accept();
            return;
        }

        if (m_tool == CanvasTool::Select) {
            m_rubberBanding = true;
            m_rubberBandStart = docPos;
            m_rubberBandCurrent = docPos;
            if (!(event->modifiers() & Qt::ShiftModifier)) {
                m_multiSelection.clear();
                selectLayer(LayerId());
            }
            event->accept();
            return;
        }
        selectLayer(LayerId());
    }
    event->ignore();
}

void CanvasView::collectSnapTargets(GroupLayer* group, const LayerId& excludeId,
                                    QList<Layer*>& out) const
{
    if (!group)
        return;

    // Percorre recursivamente todas as camadas filhas do grupo raiz
    for (const auto& child : group->children) {
        Layer* layer = child.get();
        // Ignora ponteiros nulos ou a própria camada que está sendo movida
        if (!layer || layer->id() == excludeId)
            continue;

        // O BackgroundLayer não tem dimensões intrínsecas (contentBounds vazio).
        // Os limites do documento já são providos por canvasRect, então ignoramos o fundo aqui.
        if (layer->type() == LayerType::Background)
            continue;

        // Apenas camadas visíveis e destravadas servem de alvo para snap magnético
        if (layer->visible && !layer->locked)
            out.append(layer);

        // Se for um grupo, desce recursivamente para coletar os filhos dentro dele
        if (layer->type() == LayerType::Group)
            collectSnapTargets(static_cast<GroupLayer*>(layer), excludeId, out);
    }
}

void CanvasView::drawSnapGuides(QPainter* painter)
{
    // Se não houver guias de alinhamento ativas ou se não houver documento aberto, sai
    if (m_activeGuides.isEmpty() || !m_document)
        return;

    painter->save();

    // Reseta a transformação do painter para desenhar diretamente em coordenadas da janela (device pixels).
    // Isso é crucial para que a espessura das linhas e o padrão tracejado não sofram distorção de zoom.
    painter->resetTransform();

    // Ativa o anti-aliasing explicitamente para garantir que linhas finas fiquem contínuas e sem artefatos
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QTransform toDevice = docToDevice();
    const double docW = double(m_document->width());
    const double docH = double(m_document->height());

    // Margem de transbordo (em pixels de documento) para a linha passar um pouco além das bordas do canvas.
    // Isso garante que mesmo quando a imagem encosta na borda exata da tela (como na foto do usuário),
    // a guia fique visível e não seja escondida pela borda de seleção azul ou pela própria imagem.
    const double marginDoc = 40.0 / std::max(0.01, m_zoom);

    // 1. Caneta de sombra (contraste escuro semi-transparente):
    // Garante que a linha guia seja facilmente legível mesmo sobre imagens claras ou brancas.
    QPen shadowPen(QColor(0, 0, 0, 140));
    shadowPen.setWidthF(3.0);
    shadowPen.setCosmetic(true);

    // 2. Caneta principal (magenta vibrante #ff007f):
    // Cor padrão da indústria (Canva, Figma, Illustrator) para guias inteligentes com alta visibilidade.
    QPen guidePen(QColor(0xff, 0x00, 0x7f));
    guidePen.setWidthF(1.5);
    guidePen.setStyle(Qt::DashLine);
    guidePen.setCosmetic(true); // Garante espessura exata de 1.5px na tela independente do zoom

    for (const GuideLine& guide : std::as_const(m_activeGuides)) {
        QPointF p1;
        QPointF p2;

        if (guide.isVertical()) {
            // Guia vertical: atravessa de cima a baixo ao longo do eixo X (guide.axisPosition)
            p1 = toDevice.map(QPointF(guide.axisPosition, -marginDoc));
            p2 = toDevice.map(QPointF(guide.axisPosition, docH + marginDoc));
        } else {
            // Guia horizontal: atravessa da esquerda para a direita ao longo do eixo Y (guide.axisPosition)
            p1 = toDevice.map(QPointF(-marginDoc, guide.axisPosition));
            p2 = toDevice.map(QPointF(docW + marginDoc, guide.axisPosition));
        }

        // Desenha primeiro o fundo escuro de contraste
        painter->setPen(shadowPen);
        painter->drawLine(p1, p2);

        // Desenha por cima a linha magenta tracejada de alta visibilidade
        painter->setPen(guidePen);
        painter->drawLine(p1, p2);
    }

    painter->restore();
}

void CanvasView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        const QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        m_panOffset += QPointF(delta);
        update();
        event->accept();
        return;
    }

    const QPointF docPos = deviceToDoc().map(QPointF(event->pos()));

    if (m_tool != CanvasTool::Select) {
        if (m_activeTool) {
            m_activeTool->mouseMove(event, docPos, makeToolContext());
        }
        emit cursorMoved(docPos);
        return;
    }

    if (m_activeTool) {
        m_activeTool->mouseMove(event, docPos, makeToolContext());
    }

    if (m_gesture != Gesture::None && m_document) {
        if (m_document && !m_selectedId.isNull()) {
            Layer* layer = m_document->findLayer(m_selectedId);
            if (!layer) {
                event->accept();
                return;
            }
            const QPointF docNow = deviceToDoc().map(QPointF(event->pos()));
            AffineTransform t = m_gestureStart;

            if (m_gesture == Gesture::Move) {
                const QPointF delta = docNow - m_gestureStartDoc;
                t.position += delta;
                if (m_multiSelection.size() > 1) {
                    for (int i = 0; i < m_multiSelection.size(); ++i) {
                        if (m_multiSelection[i] == m_selectedId) continue;
                        if (i < m_multiGestureStarts.size()) {
                            AffineTransform mt = m_multiGestureStarts[i];
                            mt.position += delta;
                            m_document->setLayerTransform(m_multiSelection[i], mt);
                        }
                    }
                }
            } else if (m_gesture == Gesture::Rotate) {
                const double angle = std::atan2(docNow.y() - m_rotateCenter.y(),
                                                docNow.x() - m_rotateCenter.x());
                t.rotationDeg = m_gestureStart.rotationDeg
                                + qRadiansToDegrees(angle - m_rotateStartAngle);
                t.position = m_gestureStart.position;
            } else {
                // Scale-based resize for all layer types (images, text, shapes).
                const QTransform inv =
                    m_gestureStart.matrix(m_gestureBounds).inverted();
                const QPointF localNow = inv.map(docNow);
                const QPointF center = m_gestureBounds.center();
                const double dx = localNow.x() - m_fixedLocal.x();
                const double dy = localNow.y() - m_fixedLocal.y();
                const double px = m_localPress.x() - m_fixedLocal.x();
                const double py = m_localPress.y() - m_fixedLocal.y();

                if (m_activeHandle == 5 || m_activeHandle == 7) {
                    if (qAbs(px) > 1e-6)
                        t.scaleX = m_gestureStart.scaleX * (dx / px);
                } else if (m_activeHandle == 4 || m_activeHandle == 6) {
                    if (qAbs(py) > 1e-6)
                        t.scaleY = m_gestureStart.scaleY * (dy / py);
                } else {
                    double ratio = 1.0;
                    if (qAbs(px) >= qAbs(py) && qAbs(px) > 1e-6)
                        ratio = dx / px;
                    else if (qAbs(py) > 1e-6)
                        ratio = dy / py;
                    t.scaleX = m_gestureStart.scaleX * ratio;
                    t.scaleY = m_gestureStart.scaleY * ratio;
                }

                if (qAbs(t.scaleX) < 0.01)
                    t.scaleX = t.scaleX < 0 ? -0.01 : 0.01;
                if (qAbs(t.scaleY) < 0.01)
                    t.scaleY = t.scaleY < 0 ? -0.01 : 0.01;

                const double rad = qDegreesToRadians(m_gestureStart.rotationDeg);
                const double cosR = std::cos(rad);
                const double sinR = std::sin(rad);
                const QPointF v(m_fixedLocal.x() - center.x(),
                                m_fixedLocal.y() - center.y());
                const QPointF scaled(v.x() * t.scaleX, v.y() * t.scaleY);
                const QPointF rotated(scaled.x() * cosR - scaled.y() * sinR,
                                      scaled.x() * sinR + scaled.y() * cosR);
                t.position = m_fixedDoc - rotated;
            }

            if (m_gesture == Gesture::Move) {
                // Se o usuário segurar Alt durante o movimento, desativa temporariamente o snap magnético
                // para permitir ajustes finos manuais sem interferência das guias.
                const bool snapDisabled = (event->modifiers() & Qt::AltModifier);

                const bool isMockup = layer->name.startsWith(QStringLiteral("Mockup"), Qt::CaseInsensitive);
                if (!snapDisabled && !isMockup) {
                    // 1. Sensibilidade dinâmica baseada no zoom da tela:
                    // Definimos um limiar confortável de cerca de 10 pixels físicos na tela.
                    // Em zoom 100% -> limiar de 10px em coordenadas do documento.
                    // Em zoom 50% -> limiar de 20px em coordenadas do documento.
                    // Isso evita que em zoom reduzido o usuário precise acertar uma faixa minúscula e perca o snap.
                    const double screenThresholdPx = 10.0;
                    m_snapEngine.setThreshold(screenThresholdPx / std::max(0.01, m_zoom));

                    // 2. Calcula a caixa delimitadora atual da camada em coordenadas do documento
                    const QRectF bounds = layer->contentBounds();
                    const QRectF currentDocRect = t.matrix(bounds).mapRect(bounds);
                    const QRectF canvasRect(0, 0, m_document->width(), m_document->height());

                    // 3. Coleta todas as outras camadas visíveis para comparar posições de bordas e centros
                    QList<Layer*> others;
                    if (GroupLayer* root = m_document->rootGroup())
                        collectSnapTargets(root, m_selectedId, others);

                    // 4. Computa se há alinhamento com o centro/bordas do documento ou com outras camadas
                    const SnapResult snap = m_snapEngine.computeSnap(*layer, currentDocRect,
                                                                     canvasRect, others);

                    // 5. Aplica a atração magnética (delta) diretamente na posição da camada para alinhá-la
                    t.position += snap.delta;

                    // 6. Salva as linhas guias calculadas para que o paintEvent as desenhe imediatamente
                    m_activeGuides = snap.guides;
                } else {
                    m_activeGuides.clear();
                }
            } else {
                m_activeGuides.clear();
            }

            m_document->setLayerTransform(m_selectedId, t);
            update();
            event->accept();
            return;
        }
    }

    if (m_rubberBanding && m_document) {
        m_rubberBandCurrent = deviceToDoc().map(QPointF(event->pos()));
        update();
        event->accept();
        return;
    }

    if (m_gesture == Gesture::None)
        updateCursor(QPointF(event->pos()));

    if (m_document)
        emit cursorMoved(deviceToDoc().map(QPointF(event->pos())));
    event->ignore();
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_panning
        && (event->button() == Qt::MiddleButton
            || event->button() == Qt::LeftButton)) {
        m_panning = false;
        setCursor(m_spacePanning ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }

    const QPointF docPos = deviceToDoc().map(QPointF(event->pos()));

    if (m_tool != CanvasTool::Select) {
        if (m_activeTool) {
            m_activeTool->mouseRelease(event, docPos, makeToolContext());
        }
        return;
    }

    if (m_activeTool) {
        m_activeTool->mouseRelease(event, docPos, makeToolContext());
    }

    if (m_rubberBanding && event->button() == Qt::LeftButton) {
        m_rubberBanding = false;
        if (m_document) {
            const QRectF selRect = QRectF(m_rubberBandStart, m_rubberBandCurrent).normalized();
            if (selRect.width() > 4 || selRect.height() > 4) {
                QList<Layer*> hits = hitTestRubberBand(selRect);
                QList<LayerId> ids;
                for (Layer* l : hits)
                    ids.append(l->id());
                setMultiSelection(ids);
            }
        }
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_gesture != Gesture::None) {
        if (m_document && !m_selectedId.isNull()) {
            if (m_multiSelection.size() > 1) {
                // A3: UM passo de undo para o gesto inteiro — antes cada
                // camada arrastada virava um comando separado e o Ctrl+Z
                // movia uma camada por vez, criando estados que nunca
                // existiram.
                if (m_history) {
                    auto compound = std::make_unique<CompoundCommand>(
                        QStringLiteral("layer.transform"));
                    for (int i = 0; i < m_multiSelection.size(); ++i) {
                        Layer* ml = m_document->findLayer(m_multiSelection[i]);
                        if (!ml) continue;
                        const AffineTransform oldT =
                            (m_multiSelection[i] == m_selectedId)
                                ? m_gestureStart
                                : (i < m_multiGestureStarts.size()
                                       ? m_multiGestureStarts[i]
                                       : ml->transform);
                        if (oldT != ml->transform)
                            compound->addCommand(
                                std::make_unique<SetLayerTransformCommand>(
                                    *m_document, m_multiSelection[i], oldT,
                                    ml->transform));
                    }
                    if (!compound->isEmpty())
                        m_history->execute(std::move(compound));
                } else {
                    for (int i = 0; i < m_multiSelection.size(); ++i) {
                        if (m_multiSelection[i] == m_selectedId) continue;
                        if (Layer* ml = m_document->findLayer(m_multiSelection[i])) {
                            if (i < m_multiGestureStarts.size())
                                emit transformCommitted(m_multiSelection[i],
                                                        m_multiGestureStarts[i], ml->transform);
                        }
                    }
                }
                // Caixa de texto da camada principal segue o fluxo existente.
                if (Layer* layer = m_document->findLayer(m_selectedId)) {
                    if (layer->type() == LayerType::Text) {
                        const QSizeF newBox = static_cast<TextLayer*>(layer)->box;
                        if (newBox != m_gestureStartBox)
                            emit textBoxCommitted(m_selectedId, m_gestureStartBox,
                                                  newBox);
                    }
                }
            } else if (Layer* layer = m_document->findLayer(m_selectedId)) {
                emit transformCommitted(m_selectedId, m_gestureStart,
                                        layer->transform);
                if (layer->type() == LayerType::Text) {
                    const QSizeF newBox = static_cast<TextLayer*>(layer)->box;
                    if (newBox != m_gestureStartBox)
                        emit textBoxCommitted(m_selectedId, m_gestureStartBox,
                                              newBox);
                }
            }
        }
        // Finaliza o gesto interativo e limpa o handle ativo
        m_gesture = Gesture::None;
        m_activeHandle = -1;

        // Limpa as guias inteligentes da tela assim que o usuário solta o botão do mouse
        // e solicita uma repintura imediata para remover as linhas de guia
        m_activeGuides.clear();
        update();
        event->accept();
        return;
    }

    event->ignore();
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_mockupPlacementActive && event->button() == Qt::LeftButton) {
        finishMockupPlacement();
        event->accept();
        return;
    }
    if (m_activeTool) {
        const QPointF docPos = deviceToDoc().map(QPointF(event->pos()));
        m_activeTool->mouseDoubleClick(event, docPos, makeToolContext());
        if (event->isAccepted())
            return;
    }

    if (event->button() == Qt::LeftButton && m_document) {
        const QPointF docPos = deviceToDoc().map(QPointF(event->pos()));
        Layer* layer = hitTestLayer(docPos);
        if (layer && layer->type() == LayerType::Text) {
            beginTextEdit(layer->id());
            event->accept();
            return;
        }
    }
    event->ignore();
}

bool CanvasView::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_textEditor && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            m_textEditor->hide(); // cancel: no commit
            m_editingTextId = LayerId();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CanvasView::keyPressEvent(QKeyEvent* event)
{
    if (m_mockupPlacementActive && event->key() == Qt::Key_Return && m_mockupCorners.size() >= 3) {
        finishMockupPlacement();
        event->accept();
        return;
    }
    if (m_activeTool) {
        m_activeTool->keyPress(event, makeToolContext());
        if (event->isAccepted())
            return;
    }

    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
        && !m_selectedId.isNull()) {
        if (!m_document || !m_document->effectiveLocks(m_selectedId).all) {
            emit deleteRequested(m_selectedId);
        }
        event->accept();
        return;
    }

    // Nudge: movimentação fina de 1px (ou 10px segurando Shift) com as setas do teclado
    if (!m_selectedId.isNull() && m_document &&
        (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right ||
         event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
        Layer* layer = m_document->findLayer(m_selectedId);
        const auto locks = m_document->effectiveLocks(m_selectedId);
        if (layer && !locks.position && !locks.all && layer->type() != LayerType::Background) {
            const double step = (event->modifiers() & Qt::ShiftModifier) ? 10.0 : 1.0;
            const AffineTransform oldT = layer->transform;
            AffineTransform newT = oldT;
            if (event->key() == Qt::Key_Left)
                newT.position.rx() -= step;
            else if (event->key() == Qt::Key_Right)
                newT.position.rx() += step;
            else if (event->key() == Qt::Key_Up)
                newT.position.ry() -= step;
            else if (event->key() == Qt::Key_Down)
                newT.position.ry() += step;

            m_document->setLayerTransform(m_selectedId, newT);
            emit transformCommitted(m_selectedId, oldT, newT);
            update();
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePanning = true;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }

    switch (event->key()) {
    case Qt::Key_F: fitToViewport(); event->accept(); return;
    case Qt::Key_Plus:
    case Qt::Key_Equal: zoomIn(); event->accept(); return;
    case Qt::Key_Minus: zoomOut(); event->accept(); return;
    case Qt::Key_1: zoomTo(1.0); event->accept(); return;
    default: event->ignore();
    }
}

// Menu de contexto com botão direito diretamente sobre qualquer camada no canvas
void CanvasView::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_document)
        return;

    if (m_tool == CanvasTool::CloneStamp) {
        event->accept();
        return;
    }

    const QPointF docPos = deviceToDoc().map(QPointF(event->pos()));
    Layer* layer = hitTestLayer(docPos);
    if (!layer && !m_selectedId.isNull())
        layer = m_document->findLayer(m_selectedId);
    if (!layer || layer->type() == LayerType::Background)
        return;

    selectLayer(layer->id());

    QMenu menu(this);

    // Rótulos contextualizados e traduzidos com fallbacks seguros
    const QString duplicateText = m_i18n ? m_i18n->t("common", "panel.duplicate") : QStringLiteral("Duplicate");
    const QString deleteText = m_i18n ? m_i18n->t("common", "panel.delete") : QStringLiteral("Delete");
    const QString frontText = m_i18n ? m_i18n->t("common", "panel.front") : QStringLiteral("Bring to Front");
    const QString backText = m_i18n ? m_i18n->t("common", "panel.back") : QStringLiteral("Send to Back");
    const QString flipHText = m_i18n ? m_i18n->t("common", "menu.layer.flipH") : QStringLiteral("Flip Horizontal");
    const QString flipVText = m_i18n ? m_i18n->t("common", "menu.layer.flipV") : QStringLiteral("Flip Vertical");
    const QString centerText = m_i18n ? m_i18n->t("common", "menu.layer.align.centerBoth") : QStringLiteral("Center on Canvas");
    const QString lockText = layer->locked
        ? (m_i18n ? m_i18n->t("common", "panel.unlock") : QStringLiteral("Unlock"))
        : (m_i18n ? m_i18n->t("common", "panel.lock") : QStringLiteral("Lock"));

    QAction* duplicateAction = menu.addAction(duplicateText);
    QAction* deleteAction = menu.addAction(deleteText);
    menu.addSeparator();
    QAction* frontAction = menu.addAction(frontText);
    QAction* backAction = menu.addAction(backText);
    menu.addSeparator();
    QAction* flipHAction = menu.addAction(flipHText);
    QAction* flipVAction = menu.addAction(flipVText);
    QAction* centerAction = menu.addAction(centerText);
    menu.addSeparator();
    QAction* lockAction = menu.addAction(lockText);

    QAction* removeBgAiDialogAction = nullptr;
    QAction* removeBgAiQuickAction = nullptr;
    QAction* maskInsideAction = nullptr;
    QAction* maskOutsideAction = nullptr;
    QAction* copySelectionAction = nullptr;
    QAction* cutSelectionAction = nullptr;
    QAction* invertSelectionAction = nullptr;
    QAction* clearSelectionAction = nullptr;
    if (layer->type() == LayerType::Image) {
        if (!m_document->pixelSelection().isNull()) {
            menu.addSeparator();
            maskInsideAction = menu.addAction(QStringLiteral("Mask: keep inside selection"));
            maskOutsideAction = menu.addAction(QStringLiteral("Mask: keep outside selection"));
            copySelectionAction = menu.addAction(QStringLiteral("Copy selection to new layer"));
            cutSelectionAction = menu.addAction(QStringLiteral("Cut selection to new layer"));
            menu.addSeparator();
            invertSelectionAction = menu.addAction(QStringLiteral("Invert selection"));
            clearSelectionAction = menu.addAction(QStringLiteral("Cancel selection"));
        }
        menu.addSeparator();
        const QString aiDialogText = m_i18n ? m_i18n->t("editor", "canvas.context.aiDialog")
                                            : QStringLiteral("✨ Remove Background with AI...");
        const QString aiQuickText = m_i18n ? m_i18n->t("editor", "canvas.context.aiQuick")
                                           : QStringLiteral("⚡ Quick Background Removal (1-Click)");
        removeBgAiDialogAction = menu.addAction(aiDialogText);
        removeBgAiQuickAction = menu.addAction(aiQuickText);
    }

    QAction* chosen = menu.exec(event->globalPos());
    if (!chosen)
        return;

    const LayerId id = layer->id();
    if (chosen == invertSelectionAction) {
        m_document->setPixelSelection(
            SelectionOperations::inverted(m_document->pixelSelection()));
        update();
    } else if (chosen == clearSelectionAction) {
        m_document->clearPixelSelection();
        update();
    } else if (chosen == copySelectionAction || chosen == cutSelectionAction) {
        LayerId createdId;
        if (SelectionOperations::extractToLayer(
                *m_document, m_history, id, chosen == cutSelectionAction, &createdId)) {
            if (chosen == cutSelectionAction) {
                m_selectedId = createdId;
                emit selectionChanged(m_selectedId);
            }
            update();
        }
    } else if (chosen == maskInsideAction || chosen == maskOutsideAction) {
        if (SelectionOperations::applyMask(
                *m_document, m_history, id, chosen == maskInsideAction)) {
            update();
        }
    } else if (chosen == removeBgAiDialogAction) {
        openAiBackgroundRemoval(id);
    } else if (chosen == removeBgAiQuickAction) {
        removeBackgroundAiQuick(id);
    } else if (chosen == duplicateAction) {
        emit duplicateRequested(id);
    } else if (chosen == deleteAction) {
        emit deleteRequested(id);
    } else if (chosen == frontAction) {
        // A4: antes mutava direto reorderLayer(root) — (1) sem comando de
        // undo e (2) arrancava a camada do seu grupo pai para a raiz.
        if (m_history) {
            m_history->execute(std::make_unique<ReorderLayerCommand>(
                *m_document, id, m_document->parentOf(id), -1));
        } else {
            m_document->reorderLayer(id, m_document->parentOf(id), -1);
        }
    } else if (chosen == backAction) {
        if (m_history) {
            m_history->execute(std::make_unique<ReorderLayerCommand>(
                *m_document, id, m_document->parentOf(id), 0));
        } else {
            m_document->reorderLayer(id, m_document->parentOf(id), 0);
        }
    } else if (chosen == flipHAction) {
        const AffineTransform oldT = layer->transform;
        AffineTransform newT = oldT;
        newT.scaleX = -newT.scaleX;
        m_document->setLayerTransform(id, newT);
        emit transformCommitted(id, oldT, newT);
    } else if (chosen == flipVAction) {
        const AffineTransform oldT = layer->transform;
        AffineTransform newT = oldT;
        newT.scaleY = -newT.scaleY;
        m_document->setLayerTransform(id, newT);
        emit transformCommitted(id, oldT, newT);
    } else if (chosen == centerAction) {
        const AffineTransform oldT = layer->transform;
        AffineTransform newT = oldT;
        newT.position = QPointF(m_document->width() / 2.0, m_document->height() / 2.0);
        m_document->setLayerTransform(id, newT);
        emit transformCommitted(id, oldT, newT);
    } else if (chosen == lockAction) {
        // A4: mesmo comando do painel de camadas — antes era mutação
        // silenciosa, sem passo no undo.
        if (m_history) {
            m_history->execute(std::make_unique<LayerLockCommand>(
                *m_document, id, QStringLiteral("layer.lock"),
                &Document::setLayerLocked, layer->locked, !layer->locked));
        } else {
            m_document->setLayerLocked(id, !layer->locked);
        }
    }
}

void CanvasView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    event->ignore();
}

void CanvasView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    if (!m_document)
        return;

    // Modo "ajustado à janela": continua ajustando ao redimensionar.
    if (m_autoFit) {
        fitToViewport();
        return;
    }

    // Zoom manual: preserva o zoom e mantém o conteúdo centralizado.
    // Antes, todo resize refazia o fit e o usuário perdia o zoom.
    if (event->oldSize().isValid()) {
        const QSize delta = event->size() - event->oldSize();
        m_panOffset += QPointF(delta.width() / 2.0, delta.height() / 2.0);
    }
    update();
}

void CanvasView::leaveEvent(QEvent* event)
{
    update();
    QWidget::leaveEvent(event);
}

void CanvasView::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        update();
    }
}

void CanvasView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void CanvasView::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void CanvasView::dropEvent(QDropEvent* event)
{
    const auto urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        const QString path = url.toLocalFile();
        if (!path.isEmpty()) {
            emit fileDropped(path);
            event->acceptProposedAction();
            return;
        }
    }
}

void CanvasView::setTool(CanvasTool tool)
{
    if (m_tool == tool)
        return;

    const ToolContext ctx = makeToolContext();
    if (m_activeTool) {
        m_activeTool->deactivate(ctx);
    }

    m_tool = tool;

    switch (m_tool) {
    case CanvasTool::Select:
        m_activeTool = m_selectTool.get();
        break;
    case CanvasTool::Crop:
        m_activeTool = m_cropTool.get();
        break;
    case CanvasTool::Scissors:
        m_activeTool = m_scissorsTool.get();
        break;
    case CanvasTool::MagicWand:
        m_activeTool = m_wandTool.get();
        break;
    case CanvasTool::CloneStamp:
        m_activeTool = m_cloneTool.get();
        break;
    case CanvasTool::Paint:
        m_activeTool = m_paintTool.get();
        break;
    case CanvasTool::FloodFill:
        m_activeTool = m_floodTool.get();
        break;
    case CanvasTool::Eyedropper:
        m_activeTool = nullptr;
        setCursor(Qt::CrossCursor);
        break;
    case CanvasTool::Marquee:
        m_activeTool = m_marqueeTool.get();
        break;
    case CanvasTool::Gradient:
        m_activeTool = m_gradientTool.get();
        break;
    case CanvasTool::Retouch:
        m_activeTool = m_retouchTool.get();
        break;
    case CanvasTool::Healing:
        m_activeTool = m_healingTool.get();
        break;
    }

    if (m_tool != CanvasTool::Eyedropper && !m_activeTool) {
        unsetCursor();
    }

    if (m_activeTool) {
        m_activeTool->activate(ctx);
    }

    update();
    emit toolChanged(m_tool);
}

void CanvasView::beginMockupPlacement()
{
    if (!m_document)
        return;
    m_mockupCorners.clear();
    m_mockupPlacementActive = true;
    setCursor(Qt::CrossCursor);
    emit statusMessageRequested(QStringLiteral("Mockup: clique nos quatro cantos da área (canto superior esquerdo, superior direito, inferior direito, inferior esquerdo). Clique direito para cancelar."));
    update();
}

void CanvasView::cancelMockupPlacement()
{
    if (!m_mockupPlacementActive)
        return;
    m_mockupPlacementActive = false;
    m_mockupCorners.clear();
    setCursor(Qt::ArrowCursor);
    emit statusMessageRequested(QStringLiteral("Posicionamento do mockup cancelado."));
    update();
}

QColor CanvasView::sampleColorAt(const QPointF& docPos) const
{
    if (!m_document)
        return {};

    // 1. Se uma camada de imagem estiver selecionada, prioriza a amostragem direta dos pixels da camada
    if (!m_selectedId.isNull()) {
        if (auto* layer = m_document->findLayer(m_selectedId)) {
            if (layer->type() == LayerType::Image) {
                const auto* imgLayer = static_cast<const ImageLayer*>(layer);
                const QRectF bounds = imgLayer->contentBounds();
                if (!bounds.isEmpty()) {
                    const QPointF local = imgLayer->transform.matrix(bounds).inverted().map(docPos);
                    if (bounds.contains(local)) {
                        const QImage img = (!m_previewImage.isNull() && m_previewLayerId == imgLayer->id())
                            ? m_previewImage
                            : m_document->assets().decodedImage(imgLayer->assetId);
                        if (!img.isNull()) {
                            const int imgX = qBound(0, qFloor(local.x() * img.width() / imgLayer->naturalWidth), img.width() - 1);
                            const int imgY = qBound(0, qFloor(local.y() * img.height() / imgLayer->naturalHeight), img.height() - 1);
                            return img.pixelColor(imgX, imgY);
                        }
                    }
                }
            }
        }
    }

    // 2. Tenta amostragem em qualquer camada clicada (de cima para baixo)
    if (Layer* hit = hitTestLayer(docPos)) {
        if (hit->type() == LayerType::Image) {
            const auto* imgLayer = static_cast<const ImageLayer*>(hit);
            const QRectF bounds = imgLayer->contentBounds();
            if (!bounds.isEmpty()) {
                const QPointF local = imgLayer->transform.matrix(bounds).inverted().map(docPos);
                const QImage img = m_document->assets().decodedImage(imgLayer->assetId);
                if (!img.isNull() && bounds.contains(local)) {
                    const int imgX = qBound(0, qFloor(local.x() * img.width() / imgLayer->naturalWidth), img.width() - 1);
                    const int imgY = qBound(0, qFloor(local.y() * img.height() / imgLayer->naturalHeight), img.height() - 1);
                    return img.pixelColor(imgX, imgY);
                }
            }
        }
    }

    // 3. Fallback: Render 1x1 pixel into an offscreen image using renderDocument
    const int px = qFloor(docPos.x());
    const int py = qFloor(docPos.y());
    if (px < 0 || px >= m_document->width() || py < 0 || py >= m_document->height())
        return {};

    QImage pixelImg(1, 1, QImage::Format_ARGB32_Premultiplied);
    pixelImg.fill(Qt::transparent);

    QPainter p(&pixelImg);
    p.translate(-px, -py);

    RenderOptions opt;
    opt.previewLayerId = m_previewLayerId;
    opt.previewImage = m_previewImage;
    renderDocument(*m_document, &p, QTransform(), opt);
    p.end();

    const QRgb rgb = pixelImg.pixel(0, 0);
    return QColor::fromRgba(rgb);
}

void CanvasView::finishMockupPlacement()
{
    if (!m_mockupPlacementActive || m_mockupCorners.size() < 3)
        return;
    const QPolygonF completed = m_mockupCorners;
    m_mockupPlacementActive = false;
    m_mockupCorners.clear();
    setCursor(Qt::ArrowCursor);
    update();
    emit mockupAreaSelected(completed);
}

void CanvasView::setCropAspectRatio(double ratio)
{
    if (m_cropTool)
        m_cropTool->setAspectRatio(ratio, makeToolContext());
    update();
}

void CanvasView::applyCrop()
{
    if (m_cropTool)
        m_cropTool->applyCrop(makeToolContext());
    update();
}

void CanvasView::cancelCrop()
{
    if (m_cropTool)
        m_cropTool->cancel(makeToolContext());
    update();
}

void CanvasView::setScissorsKeepInside(bool keepInside)
{
    if (m_scissorsTool)
        m_scissorsTool->setKeepInside(keepInside);
    update();
}

void CanvasView::setScissorsAutoCrop(bool autoCrop)
{
    if (m_scissorsTool)
        m_scissorsTool->setAutoCrop(autoCrop);
    update();
}

void CanvasView::applyScissorsCut()
{
    if (m_scissorsTool)
        m_scissorsTool->applyCut(makeToolContext());
    update();
}

void CanvasView::cancelScissorsCut()
{
    if (m_scissorsTool)
        m_scissorsTool->cancel(makeToolContext());
    update();
}

void CanvasView::setWandTolerance(int tolerance)
{
    if (m_wandTool)
        m_wandTool->setTolerance(tolerance);
    if (m_floodTool)
        m_floodTool->setTolerance(tolerance);
}

void CanvasView::setWandContiguous(bool contiguous)
{
    if (m_wandTool)
        m_wandTool->setContiguous(contiguous);
}

void CanvasView::openSelectionActionsMenu()
{
    if (!m_document || m_document->pixelSelection().isNull() || m_selectedId.isNull())
        return;
    const QPoint center = rect().center();
    QContextMenuEvent event(QContextMenuEvent::Mouse, center, mapToGlobal(center));
    contextMenuEvent(&event);
}

void CanvasView::setCloneRadius(int radius)
{
    if (m_cloneTool)
        m_cloneTool->setRadius(radius);
    update();
}

void CanvasView::setCloneHardness(qreal hardness)
{
    if (m_cloneTool)
        m_cloneTool->setHardness(hardness);
}

void CanvasView::setCloneOpacity(qreal opacity)
{
    if (m_cloneTool)
        m_cloneTool->setOpacity(opacity);
}

void CanvasView::setPaintBrush(int brushType)
{
    if (m_paintTool)
        m_paintTool->setBrushType(brushType);
    update();
}

void CanvasView::setPaintColor(const QColor& color)
{
    if (m_paintTool)
        m_paintTool->setColor(color);
    if (m_floodTool)
        m_floodTool->setColor(color);
    update();
}

void CanvasView::setPaintSize(int size)
{
    if (m_paintTool)
        m_paintTool->setSize(size);
    update();
}

void CanvasView::setPaintOpacity(qreal opacity)
{
    if (m_paintTool)
        m_paintTool->setOpacity(opacity);
    update();
}

MarqueeTool* CanvasView::marqueeTool() const
{
    return m_marqueeTool.get();
}

GradientTool* CanvasView::gradientTool() const
{
    return m_gradientTool.get();
}

RetouchTool* CanvasView::retouchTool() const
{
    return m_retouchTool.get();
}

HealingTool* CanvasView::healingTool() const
{
    return m_healingTool.get();
}

void CanvasView::setMarqueeEllipse(bool ellipse)
{
    if (m_marqueeTool) m_marqueeTool->setEllipse(ellipse);
}

void CanvasView::setMarqueeFeather(int feather)
{
    if (m_marqueeTool) m_marqueeTool->setFeather(feather);
}

void CanvasView::setGradientRadial(bool radial)
{
    if (m_gradientTool) m_gradientTool->setRadial(radial);
}

void CanvasView::setGradientStartColor(const QColor& color)
{
    if (m_gradientTool) m_gradientTool->setStartColor(color);
}

void CanvasView::setGradientEndColor(const QColor& color)
{
    if (m_gradientTool) m_gradientTool->setEndColor(color);
}

void CanvasView::setGradientOpacity(qreal opacity)
{
    if (m_gradientTool) m_gradientTool->setOpacity(opacity);
}

void CanvasView::setRetouchMode(int mode)
{
    if (m_retouchTool) m_retouchTool->setMode(mode);
}

void CanvasView::setRetouchSize(int size)
{
    if (m_retouchTool) m_retouchTool->setSize(size);
}

void CanvasView::setRetouchStrength(qreal strength)
{
    if (m_retouchTool) m_retouchTool->setStrength(strength);
}

void CanvasView::setHealingMode(int mode)
{
    if (m_healingTool) m_healingTool->setMode(mode);
}

void CanvasView::setHealingRadius(int radius)
{
    if (m_healingTool) m_healingTool->setRadius(radius);
}

void CanvasView::setHealingStrength(qreal strength)
{
    if (m_healingTool) m_healingTool->setStrength(strength);
}

void CanvasView::openAiBackgroundRemoval(const LayerId& id)
{
    if (!m_document)
        return;
    Layer* layer = m_document->findLayer(id);
    if (!layer || layer->type() != LayerType::Image)
        return;

    auto* imgLayer = static_cast<ImageLayer*>(layer);
    const QImage srcImg = m_document->assets().decodedImage(imgLayer->assetId);
    if (srcImg.isNull())
        return;

    AiBackgroundDialog dlg(srcImg, m_i18n, this);
    if (dlg.exec() == QDialog::Accepted) {
        const QImage result = dlg.finalImage();
        if (!result.isNull() && result.size() == srcImg.size()) {
            const QString cmdName = m_i18n ? m_i18n->t("editor", "canvas.command.aiBackground")
                                           : QStringLiteral("AI Background Removal");
            const QImage oldMask = imgLayer->alphaMask;
            const QImage newMask = composeAlphaMask(oldMask, alphaMaskFromImage(result));
            if (m_history)
                m_history->execute(std::make_unique<LayerPropertyCommand<QImage>>(
                    *m_document, imgLayer->id(), cmdName,
                    &Document::setLayerAlphaMask, oldMask, newMask));
            update();
        }
    }
}
bool CanvasView::isQuickAiRunning() const
{
    return m_quickAiThread && m_quickAiThread->isRunning();
}

void CanvasView::removeBackgroundAiQuick(const LayerId& id)
{
    if (!m_document)
        return;
    Layer* layer = m_document->findLayer(id);
    if (!layer || layer->type() != LayerType::Image)
        return;

    // Photocraft: locks bloqueiam a remoção de fundo por IA também.
    if (!m_document->effectiveLocks(id).pixels
        && !m_document->effectiveLocks(id).all) {
        // ok, sem locks
    } else {
        emit statusMessageRequested(
            m_i18n ? m_i18n->t("editor", "canvas.status.layerLocked")
                   : QStringLiteral("The layer is locked (image pixels)."));
        return;
    }

    auto* imgLayer = static_cast<ImageLayer*>(layer);
    const QImage srcImg = m_document->assets().decodedImage(imgLayer->assetId);
    if (srcImg.isNull())
        return;

    // Se já havia uma tarefa em execução, cancela cooperativamente e agenda a próxima sem travar a UI
    if (m_quickAiThread && m_quickAiThread->isRunning()) {
        m_pendingQuickAiLayerId = id;
        if (m_quickAiCancelFlag) {
            m_quickAiCancelFlag->store(true);
        }
        return;
    }

    m_pendingQuickAiLayerId = LayerId();
    m_quickAiCancelFlag = std::make_shared<std::atomic<bool>>(false);
    auto cancelFlag = m_quickAiCancelFlag;

    emit quickAiBusyChanged(true);

    const QString processingMsg = m_i18n ? m_i18n->t("editor", "canvas.status.aiQuickProcessing")
                                         : QStringLiteral("⏳ Removing background with AI in background...");
    emit statusMessageRequested(processingMsg);

    const LayerId layerId = id;
    const LayerId oldAssetId = imgLayer->assetId;
    const int oldW = imgLayer->naturalWidth;
    const int oldH = imgLayer->naturalHeight;
    const AffineTransform oldT = layer->transform;

    QPointer<CanvasView> self(this);

    auto* thread = QThread::create([self, cancelFlag, layerId, oldAssetId, oldW, oldH, oldT, srcImg]() {
        BackgroundRemover remover;
        const QImage result = remover.isLoaded()
                                  ? remover.removeBackground(srcImg, cancelFlag.get())
                                  : QImage();

        if (cancelFlag->load())
            return;

        QMetaObject::invokeMethod(qApp, [self, cancelFlag, layerId, oldAssetId, oldW, oldH, oldT, result]() {
            if (!self || cancelFlag->load())
                return;

            if (!self->m_document || result.isNull()) {
                const QString failMsg = self->m_i18n ? self->m_i18n->t("editor", "canvas.status.aiQuickFailed")
                                                     : QStringLiteral("Failed to remove background with AI.");
                emit self->statusMessageRequested(failMsg);
                return;
            }
            const QString cmdName = self->m_i18n ? self->m_i18n->t("editor", "canvas.command.aiBackgroundQuick")
                                                 : QStringLiteral("AI Background Removal (Quick)");
            Layer* currentLayer = self->m_document->findLayer(layerId);
            if (currentLayer && currentLayer->type() == LayerType::Image
                && result.size() == QSize(oldW, oldH)) {
                auto* imageLayer = static_cast<ImageLayer*>(currentLayer);
                const QImage oldMask = imageLayer->alphaMask;
                const QImage newMask = composeAlphaMask(oldMask, alphaMaskFromImage(result));
                if (self->m_history)
                    self->m_history->execute(std::make_unique<LayerPropertyCommand<QImage>>(
                        *self->m_document, layerId, cmdName,
                        &Document::setLayerAlphaMask, oldMask, newMask));
            }
            const QString successMsg = self->m_i18n ? self->m_i18n->t("editor", "canvas.status.aiQuickSuccess")
                                                   : QStringLiteral("✓ Background removed successfully with AI!");
            emit self->statusMessageRequested(successMsg);
            self->update();
        });
    });

    m_quickAiThread = thread;
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [self]() {
        if (!self)
            return;
        self->m_quickAiThread = nullptr;
        if (!self->m_pendingQuickAiLayerId.isNull()) {
            const LayerId nextId = self->m_pendingQuickAiLayerId;
            self->m_pendingQuickAiLayerId = LayerId();
            self->removeBackgroundAiQuick(nextId);
        } else {
            emit self->quickAiBusyChanged(false);
        }
    });
    thread->start();
}

void CanvasView::drawMultiSelectionOverlay(QPainter* painter)
{
    if (!m_document) return;
    painter->save();
    painter->resetTransform();
    const QColor accent(0x2f, 0x6f, 0xed);
    QPen dashed(accent, 1.0, Qt::DashLine);
    painter->setPen(dashed);
    painter->setBrush(Qt::NoBrush);

    for (const auto& lid : m_multiSelection) {
        if (lid == m_selectedId) continue;
        Layer* layer = m_document->findLayer(lid);
        if (!layer) continue;
        const HandleSet set = handlePositions(*layer);
        if (!set.valid) continue;
        QPolygonF box;
        box << set.points[0] << set.points[1] << set.points[2] << set.points[3];
        painter->drawPolygon(box);
    }
    painter->restore();
}

void CanvasView::drawRubberBand(QPainter* painter)
{
    painter->save();
    const QTransform toScreen = docToDevice();
    const QPointF p1 = toScreen.map(m_rubberBandStart);
    const QPointF p2 = toScreen.map(m_rubberBandCurrent);
    const QRectF rect = QRectF(p1, p2).normalized();

    painter->resetTransform();
    QColor selFill = Design::Accent();
    selFill.setAlpha(35);
    painter->setPen(QPen(Design::Accent(), 1.0, Qt::DashLine));
    painter->setBrush(selFill);
    painter->drawRect(rect);
    painter->restore();
}

void CanvasView::drawGridOverlay(QPainter* painter)
{
    if (!m_document || m_gridSpacing <= 0) return;
    painter->save();
    const QTransform toScreen = docToDevice();
    const int docW = m_document->width();
    const int docH = m_document->height();

    QPen gridPen(QColor(0xff, 0xff, 0xff, 22), 0);
    painter->setPen(gridPen);
    painter->setBrush(Qt::NoBrush);

    for (int x = 0; x <= docW; x += m_gridSpacing) {
        const QPointF p1 = toScreen.map(QPointF(x, 0));
        const QPointF p2 = toScreen.map(QPointF(x, docH));
        painter->drawLine(p1, p2);
    }
    for (int y = 0; y <= docH; y += m_gridSpacing) {
        const QPointF p1 = toScreen.map(QPointF(0, y));
        const QPointF p2 = toScreen.map(QPointF(docW, y));
        painter->drawLine(p1, p2);
    }
    painter->restore();
}

void CanvasView::drawSafeZoneOverlay(QPainter* painter)
{
    if (!m_document || m_safeZoneMode == 0) return;
    painter->save();
    const QTransform toScreen = docToDevice();
    const double docW = m_document->width();
    const double docH = m_document->height();

    QPen zonePen(QColor(0xff, 0x44, 0x44, 160), 1.5, Qt::DashDotLine);
    painter->setPen(zonePen);
    painter->setBrush(Qt::NoBrush);
    QFont labelFont = painter->font();
    labelFont.setPointSize(9);
    labelFont.setBold(true);
    painter->setFont(labelFont);

    auto drawZoneRect = [&](double x, double y, double w, double h, const QString& label, const QColor& fillColor = QColor(0, 0, 0, 0)) {
        const QRectF docRect(x, y, w, h);
        const QPolygonF screenPoly = toScreen.map(QPolygonF(docRect));
        painter->setPen(zonePen);
        // A transparent fill keeps the artwork visible; only special zones
        // such as headers/timestamps receive a subtle tint.
        painter->setBrush(fillColor.alpha() == 0 ? Qt::NoBrush : QBrush(fillColor));
        painter->drawPolygon(screenPoly);
        if (!label.isEmpty()) {
            const QPointF labelPos = toScreen.map(QPointF(x + 6, y + 16));
            // Give guide labels a subtle backing so they remain readable
            // without obscuring the artwork beneath them.
            const QFontMetrics metrics(labelFont);
            const QRectF textRect = metrics.boundingRect(label);
            const QRectF background(labelPos.x() - 3.0, labelPos.y() - metrics.ascent(),
                                    textRect.width() + 6.0, metrics.height());
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(20, 20, 30, 150));
            painter->drawRoundedRect(background, 3.0, 3.0);
            painter->setPen(QColor(0xff, 0x99, 0x99));
            painter->drawText(labelPos, label);
        }
    };

    switch (m_safeZoneMode) {
    case 1: {
        const double margin = docW * 0.05;
        drawZoneRect(margin, margin, docW - 2 * margin, docH - 2 * margin,
                     QStringLiteral("YouTube Safe Title Margin"));
        const double tsW = 140.0;
        const double tsH = 44.0;
        drawZoneRect(docW - tsW - 16, docH - tsH - 16, tsW, tsH,
                     QStringLiteral("Timestamp [0:00]"), QColor(255, 68, 68, 45));
        break;
    }
    case 2: {
        if (docW != docH) {
            const double side = std::min(docW, docH);
            const double x = (docW - side) / 2.0;
            const double y = (docH - side) / 2.0;
            drawZoneRect(x, y, side, side, QStringLiteral("Feed Grid (1:1)"), QColor(50, 150, 255, 20));
        }
        const double m = std::min(docW, docH) * 0.08;
        drawZoneRect(m, m, docW - 2 * m, docH - 2 * m, QStringLiteral("Instagram Safe"));
        break;
    }
    case 3: {
        const double topH = docH * 0.085;
        drawZoneRect(0, 0, docW, topH, QStringLiteral("Header / Status"), QColor(255, 68, 68, 40));
        const double botH = docH * 0.17;
        drawZoneRect(0, docH - botH, docW, botH, QStringLiteral("Caption / Sound"), QColor(255, 68, 68, 40));
        const double rightW = docW * 0.15;
        drawZoneRect(docW - rightW, topH, rightW, docH - topH - botH,
                     QStringLiteral("Buttons"), QColor(255, 68, 68, 40));
        const double safeMargin = docW * 0.08;
        drawZoneRect(safeMargin, topH + 20, docW - rightW - safeMargin * 1.5,
                     docH - topH - botH - 40, QStringLiteral("Core Safe Zone"));
        break;
    }
    }
    painter->restore();
}

void CanvasView::drawMockupPlacementOverlay(QPainter* painter)
{
    painter->save();
    painter->resetTransform();
    const QTransform toScreen = docToDevice();
    QPolygonF screen;
    for (const QPointF& point : m_mockupCorners)
        screen << toScreen.map(point);

    QColor mkFill = Design::Accent();
    mkFill.setAlpha(32);
    painter->setPen(QPen(Design::Accent(), 2.0, Qt::DashLine));
    painter->setBrush(mkFill);
    if (screen.size() >= 3)
        painter->drawPolygon(screen);
    else if (screen.size() == 2)
        painter->drawLine(screen[0], screen[1]);
    for (int i = 0; i < screen.size(); ++i) {
        painter->setBrush(Qt::white);
        painter->drawEllipse(screen[i], 5, 5);
        painter->setBrush(QColor(0x22, 0xa6, 0xf2));
        painter->drawText(screen[i] + QPointF(8, -8), QString::number(i + 1));
    }
    const QString help = QStringLiteral("MOCKUP: %1 pontos — duplo clique/Enter confirma — botão direito cancela")
        .arg(m_mockupCorners.size());
    QFont font = painter->font(); font.setBold(true); font.setPointSize(10); painter->setFont(font);
    const QRectF message(14, 14, 430, 30);
    painter->setPen(Qt::NoPen); painter->setBrush(QColor(15, 23, 42, 220));
    painter->drawRoundedRect(message, 6, 6);
    painter->setPen(Qt::white); painter->drawText(message, Qt::AlignCenter, help);
    painter->restore();
}

} // namespace cc
