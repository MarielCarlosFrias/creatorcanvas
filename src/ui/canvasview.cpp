#include "canvasview.h"

#include "rendering/CanvasRenderer.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QLineF>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QUrl>
#include <QWheelEvent>

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
}

void CanvasView::setDocument(Document* document)
{
    if (m_document == document)
        return;
    m_document = document;
    m_selectedId = LayerId();
    m_needsFit = true;
    update();
}

void CanvasView::clearSelection()
{
    selectLayer(LayerId());
}

void CanvasView::setSelectedLayer(const LayerId& id)
{
    selectLayer(id);
}

void CanvasView::selectLayer(const LayerId& id)
{
    if (m_selectedId == id)
        return;
    m_selectedId = id;
    emit selectionChanged(m_selectedId);
    update();
}

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

    if (!m_selectedId.isNull())
        drawSelectionOverlay(&painter);
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

    if (event->button() == Qt::LeftButton && m_document) {
        const QPointF docPos = deviceToDoc().map(QPointF(event->pos()));

        const int handle = handleAt(event->pos());
        if (handle >= 0) {
            if (Layer* layer = m_document->findLayer(m_selectedId)) {
                m_gestureStart = layer->transform;
                m_gestureBounds = layer->contentBounds();
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
            selectLayer(layer->id());
            m_gestureStart = layer->transform;
            m_gestureStartDoc = docPos;
            m_gesture = Gesture::Move;
            event->accept();
            return;
        }

        selectLayer(LayerId());
    }
    event->ignore();
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

    if (m_gesture != Gesture::None && m_document) {
        if (m_document && !m_selectedId.isNull()) {
            const QPointF docNow = deviceToDoc().map(QPointF(event->pos()));
            AffineTransform t = m_gestureStart;

            if (m_gesture == Gesture::Move) {
                t.position += docNow - m_gestureStartDoc;
            } else if (m_gesture == Gesture::Rotate) {
                const double angle = std::atan2(docNow.y() - m_rotateCenter.y(),
                                                docNow.x() - m_rotateCenter.x());
                t.rotationDeg = m_gestureStart.rotationDeg
                                + qRadiansToDegrees(angle - m_rotateStartAngle);
                t.position = m_gestureStart.position;
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

            m_document->setLayerTransform(m_selectedId, t);
            update();
            event->accept();
            return;
        }
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

    if (event->button() == Qt::LeftButton && m_gesture != Gesture::None) {
        if (m_document && !m_selectedId.isNull()) {
            if (Layer* layer = m_document->findLayer(m_selectedId))
                emit transformCommitted(m_selectedId, m_gestureStart,
                                        layer->transform);
        }
        m_gesture = Gesture::None;
        m_activeHandle = -1;
        event->accept();
        return;
    }
    event->ignore();
}

void CanvasView::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
        && !m_selectedId.isNull()) {
        emit deleteRequested(m_selectedId);
        event->accept();
        return;
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

} // namespace cc
