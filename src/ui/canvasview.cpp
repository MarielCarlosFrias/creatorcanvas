#include "canvasview.h"

#include "aibackgrounddialog.h"
#include "core/image/BackgroundRemover.h"
#include "core/image/ImageProcessing.h"
#include "rendering/CanvasRenderer.h"
#include "tools/SelectTool.h"
#include "tools/CropTool.h"
#include "tools/ScissorsTool.h"
#include "tools/PaintTool.h"
#include "tools/RasterTools.h"
#include <QThread>

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
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

    if (m_tool == CanvasTool::Crop && m_document && !m_selectedId.isNull()) {
        Layer* layer = m_document->findLayer(m_selectedId);
        if (layer && layer->type() == LayerType::Image) {
            auto* img = static_cast<ImageLayer*>(layer);
            if (m_cropAspectRatio > 0.0) {
                m_cropRect = ImageProcessing::calculateAspectCropRect(
                    QSize(img->naturalWidth, img->naturalHeight), m_cropAspectRatio);
            } else {
                m_cropRect = QRectF(0, 0, img->naturalWidth, img->naturalHeight);
            }
        }
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

QList<Layer*> CanvasView::hitTestRubberBand(const QRectF& docRect) const
{
    QList<Layer*> result;
    if (!m_document) return result;
    const auto& children = m_document->rootGroup()->children;
    for (const auto& child : children) {
        Layer* layer = child.get();
        if (!layer->visible || layer->locked) continue;
        if (layer->type() == LayerType::Background) continue;
        const QRectF bounds = layer->contentBounds();
        if (bounds.isEmpty()) continue;
        const QRectF layerDocRect = layer->transform.matrix(bounds).mapRect(bounds);
        if (docRect.intersects(layerDocRect))
            result.append(layer);
    }
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

Layer* CanvasView::hitTestLayer(const QPointF& docPos) const
{
    const auto& children = m_document->rootGroup()->children;
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        Layer* layer = it->get();
        if (!layer->visible || layer->locked)
            continue;
        if (layer->type() == LayerType::Background)
            continue;
        const QRectF bounds = layer->contentBounds();
        if (bounds.isEmpty())
            continue;
        const QPointF local = layer->transform.matrix(bounds).inverted().map(docPos);
        if (bounds.contains(local))
            return layer;
    }
    return nullptr;
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
    const QColor accent(0x2f, 0x6f, 0xed);

    painter->setPen(QPen(accent, 1.0));
    painter->setBrush(Qt::NoBrush);
    QPolygonF box;
    box << set.points[0] << set.points[1] << set.points[2] << set.points[3];
    painter->drawPolygon(box);

    for (int i = 0; i < 8; ++i) {
        const QRectF rect(set.points[i] - QPointF(4, 4), QSizeF(8, 8));
        painter->fillRect(rect, Qt::white);
        painter->drawRect(rect);
    }
    painter->setBrush(accent);
    painter->drawEllipse(set.points[8], 4, 4);
    painter->restore();
}

void CanvasView::updateCursor(const QPointF& widgetPos)
{
    if (m_panning || m_gesture != Gesture::None)
        return;

    if (m_tool == CanvasTool::Crop) {
        const int ch = cropHandleAt(widgetPos);
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
    painter.fillRect(rect(), QColor(0x17, 0x18, 0x1b));

    if (!m_document)
        return;

    RenderOptions options;
    renderDocument(*m_document, &painter, docToDevice(), options);

    // Feedback visual ao vivo para pintura do Carimbo de Clonagem
    if (m_tool == CanvasTool::CloneStamp && m_isCloning && !m_cloneWorkingImage.isNull() && !m_selectedId.isNull()) {
        Layer* layer = m_document->findLayer(m_selectedId);
        if (layer && layer->type() == LayerType::Image) {
            painter.save();
            painter.setTransform(layer->transform.matrix(layer->contentBounds()) * docToDevice());
            painter.drawImage(0, 0, m_cloneWorkingImage);
            painter.restore();
        }
    }

    // Feedback visual ao vivo para a Ferramenta de Pintura
    if (m_tool == CanvasTool::Paint && m_isPainting && !m_paintWorkingImage.isNull() && !m_paintActiveLayerId.isNull()) {
        Layer* layer = m_document->findLayer(m_paintActiveLayerId);
        if (layer && layer->type() == LayerType::Image) {
            painter.save();
            painter.setTransform(layer->transform.matrix(layer->contentBounds()) * docToDevice());
            painter.drawImage(0, 0, m_paintWorkingImage);
            painter.restore();
        }
    }

    if (m_tool == CanvasTool::Crop) {
        drawCropOverlay(&painter);
    } else if (m_tool == CanvasTool::Scissors) {
        drawScissorsOverlay(&painter);
    } else if (m_tool == CanvasTool::CloneStamp) {
        drawCloneOverlay(&painter);
    } else {
        if (!m_selectedId.isNull())
            drawSelectionOverlay(&painter);
    }

    if (m_activeTool) {
        m_activeTool->drawOverlay(&painter, makeToolContext());
    }

    if (m_multiSelection.size() > 1)
        drawMultiSelectionOverlay(&painter);

    if (m_rubberBanding)
        drawRubberBand(&painter);

    if (m_showGrid)
        drawGridOverlay(&painter);

    if (m_safeZoneMode > 0)
        drawSafeZoneOverlay(&painter);

    drawSnapGuides(&painter);

    if ((m_tool == CanvasTool::Paint || m_tool == CanvasTool::CloneStamp)
        && !m_isPainting && !m_isCloning)
        drawBrushCursor(&painter);
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
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton && m_tool == CanvasTool::CloneStamp && m_document) {
        const QPointF docPos = deviceToDoc().map(QPointF(event->pos()));
        Layer* layer = nullptr;
        if (!m_selectedId.isNull())
            layer = m_document->findLayer(m_selectedId);

        if (!layer || layer->type() != LayerType::Image) {
            Layer* hit = hitTestLayer(docPos);
            if (hit && hit->type() == LayerType::Image) {
                selectLayer(hit->id());
                layer = hit;
            }
        }

        if (layer && layer->type() == LayerType::Image) {
            const QTransform matrix = layer->transform.matrix(layer->contentBounds());
            const QPointF localPos = matrix.inverted().map(docPos);
            m_cloneSrcPoint = localPos.toPoint();
            m_cloneSrcLayerId = layer->id();
            m_hasCloneSrc = true;
            const QString msg = m_i18n ? m_i18n->t("editor", "canvas.status.cloneOriginSet").arg(m_cloneSrcPoint.x()).arg(m_cloneSrcPoint.y())
                                       : QStringLiteral("Clone stamp origin set at (%1, %2). Now drag to paint.")
                                             .arg(m_cloneSrcPoint.x()).arg(m_cloneSrcPoint.y());
            emit statusMessageRequested(msg);
            update();
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton && m_document) {
        const QPointF docPos = deviceToDoc().map(QPointF(event->pos()));

        if (m_activeTool) {
            m_activeTool->mousePress(event, docPos, makeToolContext());
        }

        // Interação da ferramenta de Corte (Crop)
        if (m_tool == CanvasTool::Crop) {
            if (!m_selectedId.isNull()) {
                Layer* layer = m_document->findLayer(m_selectedId);
                if (layer && layer->type() == LayerType::Image) {
                    const int cropH = cropHandleAt(event->pos());
                    if (cropH >= 0) {
                        m_activeCropHandle = cropH;
                        m_cropStartRect = m_cropRect;
                        const QTransform matrix = layer->transform.matrix(layer->contentBounds());
                        m_cropDragStartLocal = matrix.inverted().map(docPos);
                        event->accept();
                        return;
                    }
                }
            }
            Layer* hit = hitTestLayer(docPos);
            if (hit && hit->type() == LayerType::Image) {
                selectLayer(hit->id());
                event->accept();
                return;
            }
            event->accept();
            return;
        }

        // Interação do Corte com Tesoura (Scissors Cut)
        if (m_tool == CanvasTool::Scissors) {
            Layer* layer = nullptr;
            if (!m_selectedId.isNull())
                layer = m_document->findLayer(m_selectedId);

            if (!layer || layer->type() != LayerType::Image) {
                Layer* hit = hitTestLayer(docPos);
                if (hit && hit->type() == LayerType::Image) {
                    selectLayer(hit->id());
                    layer = hit;
                }
            }

            if (layer && layer->type() == LayerType::Image) {
                const QTransform matrix = layer->transform.matrix(layer->contentBounds());
                const QPointF localPos = matrix.inverted().map(docPos);
                m_scissorsPolygon << localPos;
                m_scissorsCurrentHover = localPos;
                m_isScissorsDrawing = true;
                update();
                event->accept();
                return;
            }
            event->accept();
            return;
        }

        // Interação da Varinha Mágica (Magic Wand)
        if (m_tool == CanvasTool::MagicWand) {
            applyMagicWand(docPos);
            event->accept();
            return;
        }

        // Interação do Carimbo de Clonagem (Clone Stamp)
        if (m_tool == CanvasTool::CloneStamp) {
            Layer* layer = nullptr;
            if (!m_selectedId.isNull())
                layer = m_document->findLayer(m_selectedId);

            if (!layer || layer->type() != LayerType::Image) {
                Layer* hit = hitTestLayer(docPos);
                if (hit && hit->type() == LayerType::Image) {
                    selectLayer(hit->id());
                    layer = hit;
                }
            }

            if (layer && layer->type() == LayerType::Image) {
                auto* img = static_cast<ImageLayer*>(layer);
                const QTransform matrix = layer->transform.matrix(layer->contentBounds());
                const QPointF localPos = matrix.inverted().map(docPos);

                const bool isSourceModifier = (event->modifiers() & (Qt::AltModifier | Qt::ShiftModifier | Qt::ControlModifier));
                if (isSourceModifier) {
                    m_cloneSrcPoint = localPos.toPoint();
                    m_cloneSrcLayerId = layer->id();
                    m_hasCloneSrc = true;
                    const QString msg = m_i18n ? m_i18n->t("editor", "canvas.status.cloneOriginSet").arg(m_cloneSrcPoint.x()).arg(m_cloneSrcPoint.y())
                                               : QStringLiteral("Clone stamp origin set at (%1, %2). Now drag to paint.")
                                                     .arg(m_cloneSrcPoint.x()).arg(m_cloneSrcPoint.y());
                    emit statusMessageRequested(msg);
                    update();
                    event->accept();
                    return;
                } else if (m_hasCloneSrc) {
                    m_cloneLastDstPoint = localPos.toPoint();
                    m_cloneWorkingImage = m_document->assets().decodedImage(img->assetId);
                    m_isCloning = true;

                    m_cloneWorkingImage = ImageProcessing::cloneStamp(
                        m_cloneWorkingImage, m_cloneWorkingImage,
                        m_cloneSrcPoint, m_cloneLastDstPoint,
                        m_cloneRadius, m_cloneOpacity, m_cloneHardness);
                    update();
                    event->accept();
                    return;
                } else {
                    const QString msg = m_i18n ? m_i18n->t("editor", "canvas.status.cloneNoOrigin")
                                               : QStringLiteral("⚠️ Origin not set! Right-Click (or Shift+Click) on the image to set origin before cloning.");
                    emit statusMessageRequested(msg);
                    update();
                    event->accept();
                    return;
                }
            } else {
                const QString msg = m_i18n ? m_i18n->t("editor", "canvas.status.cloneClickImage")
                                           : QStringLiteral("Click on an image to use the Clone Stamp.");
                emit statusMessageRequested(msg);
            }
            event->accept();
            return;
        }

        // Interação da Ferramenta de Pintura Estilo Paint (Paint Tool)
        if (m_tool == CanvasTool::Paint) {
            Layer* layer = nullptr;
            if (!m_selectedId.isNull())
                layer = m_document->findLayer(m_selectedId);

            // Se a camada selecionada não for ImageLayer, procura uma camada raster sob o cursor
            if (!layer || layer->type() != LayerType::Image) {
                Layer* hit = hitTestLayer(docPos);
                if (hit && hit->type() == LayerType::Image) {
                    selectLayer(hit->id());
                    layer = hit;
                }
            }

            // Se ainda não houver camada de imagem, cria automaticamente uma nova camada transparente
            if (!layer || layer->type() != LayerType::Image) {
                auto newImgLayer = std::make_unique<ImageLayer>();
                newImgLayer->name = m_i18n ? m_i18n->t("editor", "canvas.layer.paint") : QStringLiteral("Painting");
                newImgLayer->naturalWidth = m_document->width();
                newImgLayer->naturalHeight = m_document->height();
                newImgLayer->transform.position = QPointF(m_document->width() / 2.0, m_document->height() / 2.0);

                QImage blank(m_document->width(), m_document->height(), QImage::Format_ARGB32_Premultiplied);
                blank.fill(Qt::transparent);
                LayerId assetId = m_document->assets().addImage(blank);
                newImgLayer->assetId = assetId;

                const LayerId createdId = newImgLayer->id();
                m_document->addLayer(std::move(newImgLayer));
                selectLayer(createdId);
                layer = m_document->findLayer(createdId);
            }

            if (layer && layer->type() == LayerType::Image) {
                auto* img = static_cast<ImageLayer*>(layer);
                const QTransform matrix = layer->transform.matrix(layer->contentBounds());
                const QPointF localPos = matrix.inverted().map(docPos);

                m_paintActiveLayerId = layer->id();
                m_paintOrigAssetId = img->assetId;
                m_paintOrigWidth = img->naturalWidth;
                m_paintOrigHeight = img->naturalHeight;
                m_paintOrigTransform = layer->transform;

                m_paintWorkingImage = m_document->assets().decodedImage(img->assetId);
                if (m_paintWorkingImage.isNull()) {
                    m_paintWorkingImage = QImage(img->naturalWidth > 0 ? img->naturalWidth : m_document->width(),
                                                img->naturalHeight > 0 ? img->naturalHeight : m_document->height(),
                                                QImage::Format_ARGB32_Premultiplied);
                    m_paintWorkingImage.fill(Qt::transparent);
                }

                m_paintPrevPoint = localPos;
                m_isPainting = true;

                // Pinta o primeiro ponto imediatamente
                m_paintWorkingImage = ImageProcessing::paintStroke(
                    m_paintWorkingImage, m_paintPrevPoint, localPos,
                    static_cast<ImageProcessing::BrushType>(m_paintBrushType),
                    m_paintColor, m_paintSize, m_paintOpacity);

                update();
            }
            event->accept();
            return;
        }

        // Interação do Balde de Tinta (Flood Fill)
        if (m_tool == CanvasTool::FloodFill) {
            Layer* layer = nullptr;
            if (!m_selectedId.isNull())
                layer = m_document->findLayer(m_selectedId);

            if (!layer || layer->type() != LayerType::Image) {
                Layer* hit = hitTestLayer(docPos);
                if (hit && hit->type() == LayerType::Image) {
                    selectLayer(hit->id());
                    layer = hit;
                }
            }

            if (layer && layer->type() == LayerType::Image) {
                auto* img = static_cast<ImageLayer*>(layer);
                const QTransform matrix = layer->transform.matrix(layer->contentBounds());
                const QPoint seedPt = matrix.inverted().map(docPos).toPoint();

                QImage orig = m_document->assets().decodedImage(img->assetId);
                if (!orig.isNull() && orig.rect().contains(seedPt)) {
                    QImage filled = ImageProcessing::floodFill(orig, seedPt, m_paintColor, m_wandTolerance);
                    LayerId newAssetId = m_document->assets().addImage(filled);
                    const QString cmdName = m_i18n ? m_i18n->t("editor", "canvas.command.paintBucket")
                                                   : QStringLiteral("Paint Bucket");
                    emit imageLayerModified(img->id(),
                                            img->assetId, img->naturalWidth, img->naturalHeight, layer->transform,
                                            newAssetId, img->naturalWidth, img->naturalHeight, layer->transform,
                                            cmdName);
                    update();
                }
            } else {
                const QString msg = m_i18n ? m_i18n->t("editor", "canvas.status.fillClickImage")
                                           : QStringLiteral("Click on an image to fill with Paint Bucket.");
                emit statusMessageRequested(msg);
            }
            event->accept();
            return;
        }

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
            if (event->modifiers() & Qt::ShiftModifier) {
                if (m_multiSelection.contains(layer->id()))
                    removeFromSelection(layer->id());
                else
                    addToSelection(layer->id());
            } else {
                if (!m_multiSelection.contains(layer->id())) {
                    m_multiSelection.clear();
                    selectLayer(layer->id());
                }
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

    if (m_activeTool) {
        const QPointF docPos = deviceToDoc().map(QPointF(event->pos()));
        m_activeTool->mouseMove(event, docPos, makeToolContext());
    }

    // Movimentação/redimensionamento interativo da ferramenta de Corte (Crop)
    if (m_tool == CanvasTool::Crop && m_activeCropHandle >= 0 && m_document && !m_selectedId.isNull()) {
        Layer* layer = m_document->findLayer(m_selectedId);
        if (layer && layer->type() == LayerType::Image) {
            auto* img = static_cast<ImageLayer*>(layer);
            const QPointF docNow = deviceToDoc().map(QPointF(event->pos()));
            const QTransform matrix = layer->transform.matrix(layer->contentBounds());
            const QPointF localNow = matrix.inverted().map(docNow);
            const double dx = localNow.x() - m_cropDragStartLocal.x();
            const double dy = localNow.y() - m_cropDragStartLocal.y();

            QRectF r = m_cropStartRect;
            const double maxW = img->naturalWidth;
            const double maxH = img->naturalHeight;

            if (m_activeCropHandle == 8) {
                r.translate(dx, dy);
                if (r.left() < 0) r.moveLeft(0);
                if (r.top() < 0) r.moveTop(0);
                if (r.right() > maxW) r.moveRight(maxW);
                if (r.bottom() > maxH) r.moveBottom(maxH);
            } else {
                double left = r.left();
                double top = r.top();
                double right = r.right();
                double bottom = r.bottom();

                switch (m_activeCropHandle) {
                case 0: left += dx; top += dy; break; // TL
                case 1: right += dx; top += dy; break; // TR
                case 2: right += dx; bottom += dy; break; // BR
                case 3: left += dx; bottom += dy; break; // BL
                case 4: top += dy; break; // T
                case 5: right += dx; break; // R
                case 6: bottom += dy; break; // B
                case 7: left += dx; break; // L
                default: break;
                }

                left = std::clamp(left, 0.0, std::max(0.0, right - 10.0));
                top = std::clamp(top, 0.0, std::max(0.0, bottom - 10.0));
                right = std::clamp(right, left + 10.0, maxW);
                bottom = std::clamp(bottom, top + 10.0, maxH);

                if (m_cropAspectRatio > 0.0) {
                    double currentW = right - left;
                    double currentH = bottom - top;
                    if (m_activeCropHandle == 4 || m_activeCropHandle == 6) {
                        currentW = currentH * m_cropAspectRatio;
                        right = std::min(maxW, left + currentW);
                    } else {
                        currentH = currentW / m_cropAspectRatio;
                        bottom = std::min(maxH, top + currentH);
                    }
                }
                r = QRectF(QPointF(left, top), QPointF(right, bottom));
            }
            m_cropRect = r;
            update();
            event->accept();
            return;
        }
    }

    // Desenho interativo do Corte com Tesoura (Scissors Cut)
    if (m_tool == CanvasTool::Scissors && m_document && !m_selectedId.isNull()) {
        Layer* layer = m_document->findLayer(m_selectedId);
        if (layer && layer->type() == LayerType::Image) {
            const QPointF docNow = deviceToDoc().map(QPointF(event->pos()));
            const QTransform matrix = layer->transform.matrix(layer->contentBounds());
            m_scissorsCurrentHover = matrix.inverted().map(docNow);
            if (m_isScissorsDrawing && (event->buttons() & Qt::LeftButton)) {
                if (m_scissorsPolygon.isEmpty() ||
                    QLineF(m_scissorsPolygon.last(), m_scissorsCurrentHover).length() > 4.0) {
                    m_scissorsPolygon << m_scissorsCurrentHover;
                }
            }
            update();
            event->accept();
            return;
        }
    }

    // Pintura contínua do Carimbo de Clonagem (Clone Stamp)
    if (m_tool == CanvasTool::CloneStamp && m_document) {
        const QPointF docNow = deviceToDoc().map(QPointF(event->pos()));
        m_cloneHoverDocPos = docNow;
        m_cloneHoverValid = true;
        if (m_isCloning && !m_selectedId.isNull()) {
            Layer* layer = m_document->findLayer(m_selectedId);
            if (layer && layer->type() == LayerType::Image) {
                const QTransform matrix = layer->transform.matrix(layer->contentBounds());
                const QPoint dstPt = matrix.inverted().map(docNow).toPoint();
                const int dist = std::max(std::abs(dstPt.x() - m_cloneLastDstPoint.x()),
                                          std::abs(dstPt.y() - m_cloneLastDstPoint.y()));
                const int steps = std::max(1, dist / std::max(1, static_cast<int>(m_cloneRadius * 0.3)));
                for (int s = 1; s <= steps; ++s) {
                    const double frac = static_cast<double>(s) / steps;
                    const QPoint curDst(
                        m_cloneLastDstPoint.x() + qRound(frac * (dstPt.x() - m_cloneLastDstPoint.x())),
                        m_cloneLastDstPoint.y() + qRound(frac * (dstPt.y() - m_cloneLastDstPoint.y()))
                    );
                    const QPoint curSrc = m_cloneSrcPoint + (curDst - m_cloneLastDstPoint);
                    m_cloneWorkingImage = ImageProcessing::cloneStamp(
                        m_cloneWorkingImage, m_cloneWorkingImage,
                        curSrc, curDst,
                        m_cloneRadius, m_cloneOpacity, m_cloneHardness);
                }
                m_cloneLastDstPoint = dstPt;
            }
        }
        update();
        event->accept();
        return;
    }

    // Pintura contínua com a Ferramenta de Pintura (Paint Tool)
    if (m_tool == CanvasTool::Paint && m_document && m_isPainting && !m_paintActiveLayerId.isNull()) {
        Layer* layer = m_document->findLayer(m_paintActiveLayerId);
        if (layer && layer->type() == LayerType::Image) {
            const QPointF docNow = deviceToDoc().map(QPointF(event->pos()));
            const QTransform matrix = layer->transform.matrix(layer->contentBounds());
            const QPointF curPt = matrix.inverted().map(docNow);

            m_paintWorkingImage = ImageProcessing::paintStroke(
                m_paintWorkingImage, m_paintPrevPoint, curPt,
                static_cast<ImageProcessing::BrushType>(m_paintBrushType),
                m_paintColor, m_paintSize, m_paintOpacity);

            m_paintPrevPoint = curPt;
            update();
        }
        event->accept();
        return;
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
            } else if (layer->type() == LayerType::Text) {
                // Text: handles resize the WRAP BOX, never the glyph size.
                const QTransform inv =
                    m_gestureStart.matrix(m_gestureBounds).inverted();
                const QPointF localNow = inv.map(docNow);
                QSizeF box = m_gestureStartBox;
                if (box.isEmpty())
                    box = m_gestureBounds.size();
                const bool corner = m_activeHandle <= 3;
                const bool doW = corner || m_activeHandle == 5 || m_activeHandle == 7;
                const bool doH = corner || m_activeHandle == 4 || m_activeHandle == 6;
                double newW = box.width();
                double newH = box.height();
                if (doW)
                    newW = qMax(24.0, qAbs(localNow.x() - m_fixedLocal.x()));
                if (doH)
                    newH = qMax(12.0, qAbs(localNow.y() - m_fixedLocal.y()));
                static_cast<TextLayer*>(layer)->box = QSizeF(newW, newH);

                // anchor the fixed point under the new box
                const double rad = qDegreesToRadians(m_gestureStart.rotationDeg);
                const double cosR = std::cos(rad);
                const double sinR = std::sin(rad);
                const QPointF c2(newW / 2.0, newH / 2.0);
                const QPointF v(m_fixedLocal.x() - c2.x(),
                                m_fixedLocal.y() - c2.y());
                const QPointF rotated(v.x() * cosR - v.y() * sinR,
                                      v.x() * sinR + v.y() * cosR);
                t.position = m_fixedDoc - rotated;
            } else {
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

                if (!snapDisabled) {
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

    if (m_activeTool) {
        const QPointF docPos = deviceToDoc().map(QPointF(event->pos()));
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
            if (Layer* layer = m_document->findLayer(m_selectedId)) {
                emit transformCommitted(m_selectedId, m_gestureStart,
                                        layer->transform);
                if (layer->type() == LayerType::Text) {
                    const QSizeF newBox = static_cast<TextLayer*>(layer)->box;
                    if (newBox != m_gestureStartBox)
                        emit textBoxCommitted(m_selectedId, m_gestureStartBox,
                                              newBox);
                }
            }
            if (m_multiSelection.size() > 1) {
                for (int i = 0; i < m_multiSelection.size(); ++i) {
                    if (m_multiSelection[i] == m_selectedId) continue;
                    if (Layer* ml = m_document->findLayer(m_multiSelection[i])) {
                        if (i < m_multiGestureStarts.size())
                            emit transformCommitted(m_multiSelection[i],
                                                   m_multiGestureStarts[i], ml->transform);
                    }
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

    if (m_tool == CanvasTool::Crop) {
        m_activeCropHandle = -1;
        event->accept();
        return;
    }

    if (m_tool == CanvasTool::CloneStamp) {
        if (m_isCloning && m_document && !m_selectedId.isNull()) {
            m_isCloning = false;
            Layer* layer = m_document->findLayer(m_selectedId);
            if (layer && layer->type() == LayerType::Image && !m_cloneWorkingImage.isNull()) {
                auto* img = static_cast<ImageLayer*>(layer);
                LayerId newAssetId = m_document->assets().addImage(m_cloneWorkingImage);
                emit imageLayerModified(img->id(),
                                        img->assetId, img->naturalWidth, img->naturalHeight, layer->transform,
                                        newAssetId, img->naturalWidth, img->naturalHeight, layer->transform,
                                        m_i18n ? m_i18n->t("editor", "canvas.command.cloneStamp") : QStringLiteral("Clone Stamp"));
                m_cloneWorkingImage = QImage();
            }
        }
        update();
        event->accept();
        return;
    }

    if (m_tool == CanvasTool::Paint) {
        if (m_isPainting && m_document && !m_paintActiveLayerId.isNull()) {
            m_isPainting = false;
            Layer* layer = m_document->findLayer(m_paintActiveLayerId);
            if (layer && layer->type() == LayerType::Image && !m_paintWorkingImage.isNull()) {
                auto* img = static_cast<ImageLayer*>(layer);
                LayerId newAssetId = m_document->assets().addImage(m_paintWorkingImage);
                emit imageLayerModified(img->id(),
                                        m_paintOrigAssetId, m_paintOrigWidth, m_paintOrigHeight, m_paintOrigTransform,
                                        newAssetId, img->naturalWidth, img->naturalHeight, layer->transform,
                                        m_i18n ? m_i18n->t("editor", "canvas.layer.paint") : QStringLiteral("Painting"));
                m_paintWorkingImage = QImage();
            }
        }
        update();
        event->accept();
        return;
    }

    event->ignore();
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_activeTool) {
        const QPointF docPos = deviceToDoc().map(QPointF(event->pos()));
        m_activeTool->mouseDoubleClick(event, docPos, makeToolContext());
    }

    if (event->button() == Qt::LeftButton) {
        if (m_tool == CanvasTool::Scissors) {
            applyScissorsCut();
            event->accept();
            return;
        }
        if (m_tool == CanvasTool::Crop) {
            applyCrop();
            event->accept();
            return;
        }
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
    if (m_tool == CanvasTool::Crop) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            applyCrop();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            cancelCrop();
            event->accept();
            return;
        }
    }

    if (m_tool == CanvasTool::Scissors) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            applyScissorsCut();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            cancelScissorsCut();
            event->accept();
            return;
        }
    }

    if (m_tool == CanvasTool::CloneStamp) {
        if (event->key() == Qt::Key_Escape) {
            m_hasCloneSrc = false;
            const QString msg = m_i18n ? m_i18n->t("editor", "canvas.status.cloneOriginReset")
                                       : QStringLiteral("Clone stamp origin reset.");
            emit statusMessageRequested(msg);
            update();
            event->accept();
            return;
        }
    }

    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
        && !m_selectedId.isNull()) {
        emit deleteRequested(m_selectedId);
        event->accept();
        return;
    }

    // Nudge: movimentação fina de 1px (ou 10px segurando Shift) com as setas do teclado
    if (!m_selectedId.isNull() && m_document &&
        (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right ||
         event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
        Layer* layer = m_document->findLayer(m_selectedId);
        if (layer && !layer->locked && layer->type() != LayerType::Background) {
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
    if (layer->type() == LayerType::Image) {
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
    if (chosen == removeBgAiDialogAction) {
        openAiBackgroundRemoval(id);
    } else if (chosen == removeBgAiQuickAction) {
        removeBackgroundAiQuick(id);
    } else if (chosen == duplicateAction) {
        emit duplicateRequested(id);
    } else if (chosen == deleteAction) {
        emit deleteRequested(id);
    } else if (chosen == frontAction) {
        m_document->reorderLayer(id, m_document->rootGroup(), -1);
    } else if (chosen == backAction) {
        m_document->reorderLayer(id, m_document->rootGroup(), 0);
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
        m_document->setLayerLocked(id, !layer->locked);
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
    if (m_needsFit)
        update();
}

void CanvasView::leaveEvent(QEvent* event)
{
    m_cloneHoverValid = false;
    update();
    QWidget::leaveEvent(event);
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

    const CanvasTool prevTool = m_tool;
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
    }

    if (m_activeTool) {
        m_activeTool->activate(ctx);
    }

    // Limpa estado da ferramenta anterior sem chamar setTool para evitar recursão
    if (prevTool == CanvasTool::Crop) {
        m_cropRect = QRectF();
        m_activeCropHandle = -1;
    } else if (prevTool == CanvasTool::Scissors) {
        m_scissorsPolygon.clear();
        m_isScissorsDrawing = false;
    } else if (prevTool == CanvasTool::CloneStamp) {
        m_isCloning = false;
        m_cloneWorkingImage = QImage();
    }

    // Ao ativar a ferramenta de corte, calcula o retângulo inicial da camada de imagem
    if (m_tool == CanvasTool::Crop) {
        if (m_document && !m_selectedId.isNull()) {
            Layer* layer = m_document->findLayer(m_selectedId);
            if (layer && layer->type() == LayerType::Image) {
                auto* img = static_cast<ImageLayer*>(layer);
                if (m_cropAspectRatio > 0.0) {
                    m_cropRect = ImageProcessing::calculateAspectCropRect(
                        QSize(img->naturalWidth, img->naturalHeight), m_cropAspectRatio);
                } else {
                    m_cropRect = QRectF(0, 0, img->naturalWidth, img->naturalHeight);
                }
            }
        }
    }

    if (m_tool == CanvasTool::CloneStamp) {
        const QString msg = m_i18n ? m_i18n->t("editor", "canvas.status.cloneHelp")
                                   : QStringLiteral("Clone Stamp: Right-Click or Shift+Click to set origin, then drag to paint.");
        emit statusMessageRequested(msg);
    }

    update();
    emit toolChanged(m_tool);
}

void CanvasView::setCropAspectRatio(double ratio)
{
    m_cropAspectRatio = ratio;
    if (m_tool == CanvasTool::Crop && m_document && !m_selectedId.isNull()) {
        Layer* layer = m_document->findLayer(m_selectedId);
        if (layer && layer->type() == LayerType::Image) {
            auto* img = static_cast<ImageLayer*>(layer);
            if (m_cropAspectRatio > 0.0) {
                m_cropRect = ImageProcessing::calculateAspectCropRect(
                    QSize(img->naturalWidth, img->naturalHeight), m_cropAspectRatio);
            } else {
                m_cropRect = QRectF(0, 0, img->naturalWidth, img->naturalHeight);
            }
            update();
        }
    }
}

void CanvasView::applyCrop()
{
    if (!m_document || m_selectedId.isNull() || m_cropRect.isEmpty()) {
        cancelCrop();
        return;
    }

    Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer || layer->type() != LayerType::Image) {
        cancelCrop();
        return;
    }

    auto* img = static_cast<ImageLayer*>(layer);
    QImage orig = m_document->assets().decodedImage(img->assetId);
    if (orig.isNull()) {
        cancelCrop();
        return;
    }

    const QRect cropR = m_cropRect.toRect().intersected(orig.rect());
    if (cropR.isEmpty()) {
        cancelCrop();
        return;
    }

    QImage cropped = ImageProcessing::cropImage(orig, cropR);
    if (cropped.isNull()) {
        cancelCrop();
        return;
    }

    LayerId newAssetId = m_document->assets().addImage(cropped);

    // Ajusta o centro no espaço de coordenadas do documento
    const QPointF oldCenter(img->naturalWidth / 2.0, img->naturalHeight / 2.0);
    const QPointF cropCenter = cropR.center();
    const QPointF deltaCenter = cropCenter - oldCenter;

    QTransform rotScale;
    rotScale.rotate(layer->transform.rotationDeg);
    rotScale.scale(layer->transform.scaleX, layer->transform.scaleY);
    const QPointF worldDelta = rotScale.map(deltaCenter);

    AffineTransform newTransform = layer->transform;
    newTransform.position = layer->transform.position + worldDelta;

    emit imageLayerModified(img->id(),
                            img->assetId, img->naturalWidth, img->naturalHeight, layer->transform,
                            newAssetId, cropped.width(), cropped.height(), newTransform,
                            m_i18n ? m_i18n->t("editor", "canvas.command.cropImage") : QStringLiteral("Crop Image"));

    cancelCrop();
}

void CanvasView::cancelCrop()
{
    m_cropRect = QRectF();
    m_activeCropHandle = -1;
    if (m_tool == CanvasTool::Crop) {
        setTool(CanvasTool::Select);
    } else {
        update();
    }
}

void CanvasView::setScissorsKeepInside(bool keepInside)
{
    m_scissorsKeepInside = keepInside;
    update();
}

void CanvasView::setScissorsAutoCrop(bool autoCrop)
{
    m_scissorsAutoCrop = autoCrop;
    update();
}

void CanvasView::applyScissorsCut()
{
    if (!m_document || m_selectedId.isNull() || m_scissorsPolygon.size() < 3) {
        cancelScissorsCut();
        return;
    }

    Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer || layer->type() != LayerType::Image) {
        cancelScissorsCut();
        return;
    }

    auto* img = static_cast<ImageLayer*>(layer);
    QImage orig = m_document->assets().decodedImage(img->assetId);
    if (orig.isNull()) {
        cancelScissorsCut();
        return;
    }

    ScissorsCutResult result = ImageProcessing::scissorsCut(orig, m_scissorsPolygon,
                                                           m_scissorsKeepInside, m_scissorsAutoCrop);
    if (result.image.isNull()) {
        cancelScissorsCut();
        return;
    }

    LayerId newAssetId = m_document->assets().addImage(result.image);

    AffineTransform newTransform = layer->transform;
    if (m_scissorsKeepInside && m_scissorsAutoCrop && (result.image.size() != orig.size())) {
        const QPointF oldCenter(img->naturalWidth / 2.0, img->naturalHeight / 2.0);
        const QRect bounding = m_scissorsPolygon.boundingRect().toAlignedRect().intersected(orig.rect());
        const QPointF newCenter = bounding.center();
        const QPointF deltaCenter = newCenter - oldCenter;

        QTransform rotScale;
        rotScale.rotate(layer->transform.rotationDeg);
        rotScale.scale(layer->transform.scaleX, layer->transform.scaleY);
        const QPointF worldDelta = rotScale.map(deltaCenter);

        newTransform.position = layer->transform.position + worldDelta;
    }

    emit imageLayerModified(img->id(),
                            img->assetId, img->naturalWidth, img->naturalHeight, layer->transform,
                            newAssetId, result.image.width(), result.image.height(), newTransform,
                            m_i18n ? m_i18n->t("editor", "canvas.command.scissorsCut") : QStringLiteral("Scissors Cut"));

    cancelScissorsCut();
}

void CanvasView::cancelScissorsCut()
{
    m_scissorsPolygon.clear();
    m_isScissorsDrawing = false;
    if (m_tool == CanvasTool::Scissors) {
        setTool(CanvasTool::Select);
    } else {
        update();
    }
}

void CanvasView::setWandTolerance(int tolerance)
{
    m_wandTolerance = std::clamp(tolerance, 0, 100);
}

void CanvasView::setWandContiguous(bool contiguous)
{
    m_wandContiguous = contiguous;
}

void CanvasView::applyMagicWand(const QPointF& docPos)
{
    if (!m_document)
        return;

    Layer* layer = nullptr;
    if (!m_selectedId.isNull())
        layer = m_document->findLayer(m_selectedId);

    if (!layer || layer->type() != LayerType::Image) {
        Layer* hit = hitTestLayer(docPos);
        if (hit && hit->type() == LayerType::Image) {
            selectLayer(hit->id());
            layer = hit;
        } else {
            return;
        }
    }

    auto* img = static_cast<ImageLayer*>(layer);
    QTransform matrix = layer->transform.matrix(layer->contentBounds());
    QPointF localPos = matrix.inverted().map(docPos);
    QPoint seedPt = localPos.toPoint();

    // Se o ponto clicado não está na imagem selecionada, verifica se clicou em outra camada de imagem
    if (!QRect(0, 0, img->naturalWidth, img->naturalHeight).contains(seedPt)) {
        Layer* hit = hitTestLayer(docPos);
        if (hit && hit->type() == LayerType::Image && hit != layer) {
            selectLayer(hit->id());
            layer = hit;
            img = static_cast<ImageLayer*>(layer);
            matrix = layer->transform.matrix(layer->contentBounds());
            localPos = matrix.inverted().map(docPos);
            seedPt = localPos.toPoint();
            if (!QRect(0, 0, img->naturalWidth, img->naturalHeight).contains(seedPt))
                return;
        } else {
            return;
        }
    }

    QImage orig = m_document->assets().decodedImage(img->assetId);
    if (orig.isNull())
        return;

    QImage result = ImageProcessing::removeBackground(orig, seedPt, m_wandTolerance, m_wandContiguous);
    if (result.isNull())
        return;

    LayerId newAssetId = m_document->assets().addImage(result);

    emit imageLayerModified(img->id(),
                            img->assetId, img->naturalWidth, img->naturalHeight, layer->transform,
                            newAssetId, img->naturalWidth, img->naturalHeight, layer->transform,
                            m_i18n ? m_i18n->t("editor", "canvas.command.magicWandRemoval") : QStringLiteral("Magic Wand Background Removal"));
    update();
}

void CanvasView::setCloneRadius(int radius)
{
    m_cloneRadius = std::clamp(radius, 1, 200);
    update();
}

void CanvasView::setCloneHardness(qreal hardness)
{
    m_cloneHardness = std::clamp(hardness, 0.0, 1.0);
}

void CanvasView::setCloneOpacity(qreal opacity)
{
    m_cloneOpacity = std::clamp(opacity, 0.0, 1.0);
}

void CanvasView::setPaintBrush(int brushType)
{
    m_paintBrushType = std::clamp(brushType, 0, 4);
    update();
}

void CanvasView::setPaintColor(const QColor& color)
{
    if (color.isValid()) {
        m_paintColor = color;
        update();
    }
}

void CanvasView::setPaintSize(int size)
{
    m_paintSize = std::clamp(size, 1, 300);
    update();
}

void CanvasView::setPaintOpacity(qreal opacity)
{
    m_paintOpacity = std::clamp(opacity, 0.0, 1.0);
    update();
}

int CanvasView::cropHandleAt(const QPointF& widgetPos) const
{
    if (!m_document || m_selectedId.isNull() || m_cropRect.isEmpty())
        return -1;
    Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer || layer->type() != LayerType::Image)
        return -1;

    const QTransform layerToDoc = layer->transform.matrix(layer->contentBounds());
    const QTransform layerToDevice = layerToDoc * docToDevice();

    const double w = m_cropRect.width();
    const double h = m_cropRect.height();
    const double x = m_cropRect.x();
    const double y = m_cropRect.y();

    const QPointF pts[8] = {
        layerToDevice.map(m_cropRect.topLeft()),
        layerToDevice.map(m_cropRect.topRight()),
        layerToDevice.map(m_cropRect.bottomRight()),
        layerToDevice.map(m_cropRect.bottomLeft()),
        layerToDevice.map(QPointF(x + w / 2.0, y)),
        layerToDevice.map(QPointF(x + w, y + h / 2.0)),
        layerToDevice.map(QPointF(x + w / 2.0, y + h)),
        layerToDevice.map(QPointF(x, y + h / 2.0))
    };

    for (int i = 0; i < 8; ++i) {
        if (QLineF(widgetPos, pts[i]).length() <= 8.0)
            return i;
    }

    const QPointF localPos = layerToDevice.inverted().map(widgetPos);
    if (m_cropRect.contains(localPos))
        return 8;

    return -1;
}

void CanvasView::drawCropOverlay(QPainter* painter)
{
    if (!m_document || m_selectedId.isNull() || m_cropRect.isEmpty())
        return;
    Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer || layer->type() != LayerType::Image)
        return;

    auto* img = static_cast<ImageLayer*>(layer);
    const QTransform layerToDoc = layer->transform.matrix(layer->contentBounds());
    const QTransform layerToDevice = layerToDoc * docToDevice();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF fullRect(0, 0, img->naturalWidth, img->naturalHeight);

    QPainterPath darkPath;
    darkPath.setFillRule(Qt::OddEvenFill);
    darkPath.addPolygon(layerToDevice.map(QPolygonF(fullRect)));
    darkPath.addPolygon(layerToDevice.map(QPolygonF(m_cropRect)));
    painter->fillPath(darkPath, QColor(0, 0, 0, 160));

    const QRectF screenCrop = layerToDevice.mapRect(m_cropRect);
    QPen borderPen(Qt::white, 2.0);
    borderPen.setCosmetic(true);
    painter->setPen(borderPen);
    painter->drawRect(screenCrop);

    QPen gridPen(QColor(255, 255, 255, 120), 1.0);
    gridPen.setStyle(Qt::DashLine);
    gridPen.setCosmetic(true);
    painter->setPen(gridPen);

    const double sw = screenCrop.width();
    const double sh = screenCrop.height();
    const double sx = screenCrop.x();
    const double sy = screenCrop.y();

    painter->drawLine(QPointF(sx + sw / 3.0, sy), QPointF(sx + sw / 3.0, sy + sh));
    painter->drawLine(QPointF(sx + 2.0 * sw / 3.0, sy), QPointF(sx + 2.0 * sw / 3.0, sy + sh));
    painter->drawLine(QPointF(sx, sy + sh / 3.0), QPointF(sx + sw, sy + sh / 3.0));
    painter->drawLine(QPointF(sx, sy + 2.0 * sh / 3.0), QPointF(sx + sw, sy + 2.0 * sh / 3.0));

    const double w = m_cropRect.width();
    const double h = m_cropRect.height();
    const double x = m_cropRect.x();
    const double y = m_cropRect.y();

    const QPointF pts[8] = {
        layerToDevice.map(m_cropRect.topLeft()),
        layerToDevice.map(m_cropRect.topRight()),
        layerToDevice.map(m_cropRect.bottomRight()),
        layerToDevice.map(m_cropRect.bottomLeft()),
        layerToDevice.map(QPointF(x + w / 2.0, y)),
        layerToDevice.map(QPointF(x + w, y + h / 2.0)),
        layerToDevice.map(QPointF(x + w / 2.0, y + h)),
        layerToDevice.map(QPointF(x, y + h / 2.0))
    };

    painter->setPen(QPen(Qt::black, 1));
    painter->setBrush(Qt::white);
    for (int i = 0; i < 8; ++i) {
        painter->drawRect(QRectF(pts[i].x() - 5, pts[i].y() - 5, 10, 10));
    }

    painter->restore();
}

void CanvasView::drawScissorsOverlay(QPainter* painter)
{
    if (!m_document || m_selectedId.isNull())
        return;
    Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer || layer->type() != LayerType::Image)
        return;

    const QTransform layerToDoc = layer->transform.matrix(layer->contentBounds());
    const QTransform layerToDevice = layerToDoc * docToDevice();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (m_scissorsPolygon.size() > 0) {
        QPolygonF screenPoly;
        for (const QPointF& pt : m_scissorsPolygon) {
            screenPoly << layerToDevice.map(pt);
        }

        if (screenPoly.size() >= 3) {
            painter->setBrush(QColor(0, 220, 255, 45));
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(screenPoly);
        }

        QPen shadowPen(QColor(0, 0, 0, 180), 3.0);
        shadowPen.setCosmetic(true);
        painter->setPen(shadowPen);
        painter->drawPolyline(screenPoly);

        QPen linePen(QColor(0, 220, 255), 1.5, Qt::DashLine);
        linePen.setCosmetic(true);
        painter->setPen(linePen);
        painter->drawPolyline(screenPoly);

        if (m_isScissorsDrawing) {
            QPointF lastPt = screenPoly.last();
            QPointF hoverPt = layerToDevice.map(m_scissorsCurrentHover);
            painter->setPen(QPen(QColor(255, 255, 255, 200), 1.0, Qt::DotLine));
            painter->drawLine(lastPt, hoverPt);
        }

        painter->setBrush(Qt::white);
        painter->setPen(QPen(QColor(0, 180, 220), 1.5));
        for (const QPointF& pt : screenPoly) {
            painter->drawEllipse(pt, 3.5, 3.5);
        }
    }

    painter->restore();
}

void CanvasView::drawCloneOverlay(QPainter* painter)
{
    if (!m_document)
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Se houver origem definida, desenha o marcador de origem (mira vermelha)
    if (m_hasCloneSrc) {
        Layer* srcLayer = m_document->findLayer(m_cloneSrcLayerId);
        if (!srcLayer && !m_selectedId.isNull())
            srcLayer = m_document->findLayer(m_selectedId);

        if (srcLayer && srcLayer->type() == LayerType::Image) {
            const QTransform layerToDoc = srcLayer->transform.matrix(srcLayer->contentBounds());
            const QTransform layerToDevice = layerToDoc * docToDevice();
            const QPointF srcScreen = layerToDevice.map(QPointF(m_cloneSrcPoint));

            QPen srcPen(QColor(255, 60, 60), 2.0);
            srcPen.setCosmetic(true);
            painter->setPen(srcPen);
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(srcScreen, 6, 6);
            painter->drawLine(srcScreen.x() - 10, srcScreen.y(), srcScreen.x() + 10, srcScreen.y());
            painter->drawLine(srcScreen.x(), srcScreen.y() - 10, srcScreen.x(), srcScreen.y() + 10);
        }
    }

    // Desenha o círculo tracejado do pincel no cursor
    if (m_cloneHoverValid) {
        const QPointF hoverScreen = docToDevice().map(m_cloneHoverDocPos);
        const double screenRadius = m_cloneRadius * m_zoom;

        QPen brushPen(QColor(255, 255, 255, 220), 1.5, Qt::DashLine);
        brushPen.setCosmetic(true);
        painter->setPen(brushPen);
        painter->setBrush(QColor(255, 255, 255, 25));
        painter->drawEllipse(hoverScreen, screenRadius, screenRadius);
    }

    // Banner de instrução no topo do Canvas
    const int bannerWidth = 480;
    const int bannerHeight = 32;
    const QRect bannerRect(width() / 2 - bannerWidth / 2, 14, bannerWidth, bannerHeight);

    painter->setPen(Qt::NoPen);
    if (!m_hasCloneSrc) {
        painter->setBrush(QColor(20, 24, 30, 215));
        painter->drawRoundedRect(bannerRect, 6, 6);

        painter->setPen(QColor(255, 210, 80));
        QFont font = painter->font();
        font.setPointSize(9);
        font.setBold(true);
        painter->setFont(font);
        painter->drawText(bannerRect, Qt::AlignCenter,
            m_i18n ? m_i18n->t("editor", "canvas.status.clonePromptOrigin")
                   : QStringLiteral("ℹ️ Right-Click (or Shift+Click) to set origin"));
    } else {
        painter->setBrush(QColor(20, 30, 25, 200));
        painter->drawRoundedRect(bannerRect, 6, 6);

        painter->setPen(QColor(130, 240, 160));
        QFont font = painter->font();
        font.setPointSize(9);
        font.setBold(false);
        painter->setFont(font);
        painter->drawText(bannerRect, Qt::AlignCenter,
            m_i18n ? m_i18n->t("editor", "canvas.status.cloneOriginActive")
                   : QStringLiteral("✓ Origin set! Click and drag to clone (Right-Click resets)"));
    }

    painter->restore();
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
        if (!result.isNull()) {
            LayerId newAssetId = m_document->assets().addImage(result);
            const QString cmdName = m_i18n ? m_i18n->t("editor", "canvas.command.aiBackground")
                                           : QStringLiteral("AI Background Removal");
            emit imageLayerModified(imgLayer->id(),
                                    imgLayer->assetId, imgLayer->naturalWidth, imgLayer->naturalHeight, layer->transform,
                                    newAssetId, result.width(), result.height(), layer->transform,
                                    cmdName);
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
        const QImage result = remover.removeBackground(srcImg, cancelFlag.get());

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
            LayerId newAssetId = self->m_document->assets().addImage(result);
            const QString cmdName = self->m_i18n ? self->m_i18n->t("editor", "canvas.command.aiBackgroundQuick")
                                                 : QStringLiteral("AI Background Removal (Quick)");
            emit self->imageLayerModified(layerId,
                                          oldAssetId, oldW, oldH, oldT,
                                          newAssetId, result.width(), result.height(), oldT,
                                          cmdName);
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
    painter->setPen(QPen(QColor(0x2f, 0x6f, 0xed), 1.0, Qt::DashLine));
    painter->setBrush(QColor(0x2f, 0x6f, 0xed, 35));
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

    auto drawZoneRect = [&](double x, double y, double w, double h, const QString& label, const QColor& fillColor = QColor(255, 68, 68, 20)) {
        const QRectF docRect(x, y, w, h);
        const QPolygonF screenPoly = toScreen.map(QPolygonF(docRect));
        painter->setPen(zonePen);
        painter->setBrush(fillColor);
        painter->drawPolygon(screenPoly);
        if (!label.isEmpty()) {
            const QPointF labelPos = toScreen.map(QPointF(x + 6, y + 16));
            painter->setPen(QColor(0xff, 0x77, 0x77));
            painter->drawText(labelPos, label);
        }
    };

    switch (m_safeZoneMode) {
    case 1: {
        const double margin = docW * 0.05;
        drawZoneRect(margin, margin, docW - 2 * margin, docH - 2 * margin,
                     QStringLiteral("YouTube Safe Title Margin"), Qt::NoBrush);
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
        drawZoneRect(m, m, docW - 2 * m, docH - 2 * m, QStringLiteral("Instagram Safe"), Qt::NoBrush);
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
                     docH - topH - botH - 40, QStringLiteral("Core Safe Zone"), Qt::NoBrush);
        break;
    }
    }
    painter->restore();
}

void CanvasView::drawBrushCursor(QPainter* painter)
{
    if (!m_document) return;
    const QPoint widgetPos = mapFromGlobal(QCursor::pos());
    if (!rect().contains(widgetPos)) return;

    painter->save();
    painter->resetTransform();

    int radius = 0;
    if (m_tool == CanvasTool::Paint)
        radius = m_paintSize;
    else if (m_tool == CanvasTool::CloneStamp)
        radius = m_cloneRadius;

    if (radius <= 0) { painter->restore(); return; }

    const double screenRadius = std::max(2.0, radius * m_zoom);

    QPen cursorPen(QColor(255, 255, 255, 200), 1.0);
    painter->setPen(cursorPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPointF(widgetPos), screenRadius, screenRadius);

    cursorPen.setColor(QColor(0, 0, 0, 120));
    cursorPen.setStyle(Qt::DotLine);
    painter->setPen(cursorPen);
    painter->drawEllipse(QPointF(widgetPos), screenRadius + 1.0, screenRadius + 1.0);

    painter->restore();
}

} // namespace cc
