#include "SelectTool.h"
#include <QMouseEvent>
#include <QPainter>
#include <cmath>
#include "core/layers/Layer.h"
#include "core/history/DocumentCommands.h"
#include "core/history/CommandStack.h"
#include "core/snap/SnapEngine.h"

namespace cc {

namespace {
constexpr double HANDLE_SIZE = 8.0;
constexpr double ROTATE_HANDLE_OFFSET = 24.0;

double distance(const QPointF& a, const QPointF& b) {
    const double dx = a.x() - b.x();
    const double dy = a.y() - b.y();
    return std::sqrt(dx * dx + dy * dy);
}
} // namespace

void SelectTool::setSelectedLayers(const QList<LayerId>& ids)
{
    m_selectedLayers = ids;
}

LayerId SelectTool::primarySelectedId() const
{
    return m_selectedLayers.isEmpty() ? LayerId() : m_selectedLayers.last();
}

void SelectTool::clearSelection()
{
    m_selectedLayers.clear();
    m_gesture = Gesture::None;
}

QRectF SelectTool::computeSelectionBounds(const ToolContext& ctx) const
{
    if (!ctx.document || m_selectedLayers.isEmpty())
        return {};

    QRectF combined;
    for (const auto& id : m_selectedLayers) {
        Layer* layer = ctx.document->findLayer(id);
        if (!layer) continue;
        const QRectF cb = layer->contentBounds();
        if (cb.isEmpty()) continue;
        const QRectF tb = layer->transform.matrix(cb).mapRect(cb);
        if (combined.isEmpty())
            combined = tb;
        else
            combined = combined.united(tb);
    }
    return combined;
}

SelectTool::HandleSet SelectTool::computeHandleSet(const ToolContext& ctx) const
{
    HandleSet hs;
    if (m_selectedLayers.isEmpty() || !ctx.document)
        return hs;

    if (m_selectedLayers.size() == 1) {
        Layer* layer = ctx.document->findLayer(m_selectedLayers.first());
        if (!layer) return hs;
        const QRectF cb = layer->contentBounds();
        if (cb.isEmpty()) return hs;
        const QTransform mat = layer->transform.matrix(cb);

        hs.points[0] = mat.map(cb.topLeft());
        hs.points[1] = mat.map(QPointF((cb.left() + cb.right()) * 0.5, cb.top()));
        hs.points[2] = mat.map(cb.topRight());
        hs.points[3] = mat.map(QPointF(cb.right(), (cb.top() + cb.bottom()) * 0.5));
        hs.points[4] = mat.map(cb.bottomRight());
        hs.points[5] = mat.map(QPointF((cb.left() + cb.right()) * 0.5, cb.bottom()));
        hs.points[6] = mat.map(cb.bottomLeft());
        hs.points[7] = mat.map(QPointF(cb.left(), (cb.top() + cb.bottom()) * 0.5));

        const QPointF topMid = hs.points[1];
        const double rad = layer->transform.rotationDeg * M_PI / 180.0;
        const QPointF upDir(-std::sin(rad), -std::cos(rad));
        const double zoomScale = ctx.zoom > 0.0 ? (ROTATE_HANDLE_OFFSET / ctx.zoom) : ROTATE_HANDLE_OFFSET;
        hs.points[8] = topMid + upDir * zoomScale;
        hs.valid = true;
    } else {
        const QRectF bounds = computeSelectionBounds(ctx);
        if (bounds.isEmpty()) return hs;

        hs.points[0] = bounds.topLeft();
        hs.points[1] = QPointF(bounds.center().x(), bounds.top());
        hs.points[2] = bounds.topRight();
        hs.points[3] = QPointF(bounds.right(), bounds.center().y());
        hs.points[4] = bounds.bottomRight();
        hs.points[5] = QPointF(bounds.center().x(), bounds.bottom());
        hs.points[6] = bounds.bottomLeft();
        hs.points[7] = QPointF(bounds.left(), bounds.center().y());
        hs.points[8] = QPointF(bounds.center().x(), bounds.top() - (ROTATE_HANDLE_OFFSET / (ctx.zoom > 0.0 ? ctx.zoom : 1.0)));
        hs.valid = true;
    }
    return hs;
}

int SelectTool::hitTestHandle(const QPointF& docPos, const HandleSet& handles, double zoom) const
{
    if (!handles.valid) return -1;
    const double radius = (HANDLE_SIZE / (zoom > 0.0 ? zoom : 1.0)) * 1.5;
    for (int i = 0; i < 9; ++i) {
        if (distance(docPos, handles.points[i]) <= radius)
            return i;
    }
    return -1;
}

void SelectTool::mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    if (!ctx.document || event->button() != Qt::LeftButton)
        return;

    const HandleSet handles = computeHandleSet(ctx);
    const int hit = hitTestHandle(docPos, handles, ctx.zoom);

    if (hit >= 0) {
        m_activeHandle = hit;
        m_gesture = (hit == 8) ? Gesture::Rotate : Gesture::Scale;
        m_dragStartDocPos = docPos;
        m_initialTransforms.clear();
        for (const auto& id : m_selectedLayers) {
            if (Layer* l = ctx.document->findLayer(id))
                m_initialTransforms[id] = l->transform;
        }
        return;
    }

    // Hit test layers
    Layer* hitLayer = nullptr;
    const auto& children = ctx.document->rootGroup()->children;
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        Layer* layer = it->get();
        if (!layer->visible || layer->locked) continue;
        if (layer->type() == LayerType::Background) continue;
        const QRectF cb = layer->contentBounds();
        if (cb.isEmpty()) continue;
        const QPointF local = layer->transform.matrix(cb).inverted().map(docPos);
        if (cb.contains(local)) {
            hitLayer = layer;
            break;
        }
    }

    if (hitLayer) {
        if (event->modifiers() & Qt::ShiftModifier) {
            if (m_selectedLayers.contains(hitLayer->id()))
                m_selectedLayers.removeOne(hitLayer->id());
            else
                m_selectedLayers.append(hitLayer->id());
        } else if (!m_selectedLayers.contains(hitLayer->id())) {
            m_selectedLayers = { hitLayer->id() };
        }

        m_gesture = Gesture::Move;
        m_dragStartDocPos = docPos;
        m_initialTransforms.clear();
        for (const auto& id : m_selectedLayers) {
            if (Layer* l = ctx.document->findLayer(id))
                m_initialTransforms[id] = l->transform;
        }
    } else {
        if (!(event->modifiers() & Qt::ShiftModifier))
            m_selectedLayers.clear();
        m_gesture = Gesture::RubberBand;
        m_rubberBandStart = docPos;
        m_rubberBandRect = QRectF(docPos, QSizeF(0, 0));
    }
}

void SelectTool::mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event);
    if (!ctx.document || m_gesture == Gesture::None)
        return;

    if (m_gesture == Gesture::RubberBand) {
        m_rubberBandRect = QRectF(m_rubberBandStart, docPos).normalized();
        return;
    }

    const QPointF delta = docPos - m_dragStartDocPos;
    for (const auto& id : m_selectedLayers) {
        Layer* layer = ctx.document->findLayer(id);
        if (!layer || !m_initialTransforms.contains(id)) continue;

        if (m_gesture == Gesture::Move) {
            AffineTransform t = m_initialTransforms[id];
            t.position += delta;
            layer->transform = t;
        }
    }
}

void SelectTool::mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx)
{
    Q_UNUSED(event); Q_UNUSED(docPos);
    if (!ctx.document) return;

    if (m_gesture == Gesture::RubberBand) {
        if (!m_rubberBandRect.isEmpty()) {
            const auto& children = ctx.document->rootGroup()->children;
            for (const auto& child : children) {
                Layer* l = child.get();
                if (!l->visible || l->locked || l->type() == LayerType::Background) continue;
                const QRectF cb = l->contentBounds();
                if (cb.isEmpty()) continue;
                const QRectF tb = l->transform.matrix(cb).mapRect(cb);
                if (m_rubberBandRect.intersects(tb)) {
                    if (!m_selectedLayers.contains(l->id()))
                        m_selectedLayers.append(l->id());
                }
            }
        }
    } else if (m_gesture == Gesture::Move || m_gesture == Gesture::Scale || m_gesture == Gesture::Rotate) {
        if (ctx.history) {
            for (const auto& id : m_selectedLayers) {
                Layer* layer = ctx.document->findLayer(id);
                if (layer && m_initialTransforms.contains(id)) {
                    const AffineTransform oldT = m_initialTransforms[id];
                    const AffineTransform newT = layer->transform;
                    if (oldT.position != newT.position || oldT.scaleX != newT.scaleX || oldT.scaleY != newT.scaleY || oldT.rotationDeg != newT.rotationDeg) {
                        ctx.history->execute(std::make_unique<SetLayerTransformCommand>(
                            *ctx.document, id, oldT, newT
                        ));
                    }
                }
            }
        }
    }

    m_gesture = Gesture::None;
    m_activeHandle = -1;
    m_rubberBandRect = QRectF();
}

void SelectTool::drawOverlay(QPainter* painter, const ToolContext& ctx)
{
    if (!ctx.document || m_selectedLayers.isEmpty())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 1. Draw rubberband if dragging
    if (m_gesture == Gesture::RubberBand && !m_rubberBandRect.isEmpty()) {
        const QRectF devRect = ctx.docToDevice.mapRect(m_rubberBandRect);
        painter->setPen(QPen(QColor(43, 120, 228, 220), 1, Qt::DashLine));
        painter->setBrush(QColor(43, 120, 228, 40));
        painter->drawRect(devRect);
    }

    // 2. Draw bounds & handles
    const HandleSet hs = computeHandleSet(ctx);
    if (hs.valid) {
        QPolygonF poly;
        for (int i = 0; i < 8; ++i)
            poly.append(ctx.docToDevice.map(hs.points[i]));
        
        painter->setPen(QPen(QColor(43, 120, 228), 1.5));
        painter->setBrush(Qt::NoBrush);
        painter->drawPolygon(poly);

        // Handles
        for (int i = 0; i < 9; ++i) {
            const QPointF devP = ctx.docToDevice.map(hs.points[i]);
            const double r = HANDLE_SIZE * 0.5;
            painter->setPen(QPen(QColor(43, 120, 228), 1.5));
            painter->setBrush(i == 8 ? QColor(255, 220, 0) : QColor(255, 255, 255));
            painter->drawRect(QRectF(devP.x() - r, devP.y() - r, HANDLE_SIZE, HANDLE_SIZE));
        }
    }

    painter->restore();
}

void SelectTool::cancel(const ToolContext& ctx)
{
    Q_UNUSED(ctx);
    m_gesture = Gesture::None;
    m_activeHandle = -1;
    m_rubberBandRect = QRectF();
}

} // namespace cc
