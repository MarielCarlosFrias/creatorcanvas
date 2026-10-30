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

namespace {

// Aplica um gesto de escala a uma única camada. O ponto oposto ao handle
// arrastado permanece fixo no documento; scaleX/scaleY são derivados da
// variação proporcional do tamanho dos bounds.
void applyScale(Layer* layer, AffineTransform& t, const QPointF& delta,
                int activeHandle, const QPointF& anchorDocPos)
{
    if (!layer) return;
    const QRectF cb = layer->contentBounds();
    if (cb.isEmpty() || cb.width() <= 0 || cb.height() <= 0) return;

    const QTransform mat = layer->transform.matrix(cb);
    const QRectF docBounds = mat.mapRect(cb);
    if (docBounds.isEmpty() || docBounds.width() <= 0 || docBounds.height() <= 0)
        return;

    QRectF r = docBounds;
    const double dx = delta.x();
    const double dy = delta.y();

    // Cada handle move lados específicos do rect.
    switch (activeHandle) {
    case 0: // TL
        r.setLeft(r.left() + dx);
        r.setTop(r.top() + dy);
        break;
    case 1: // T
        r.setTop(r.top() + dy);
        break;
    case 2: // TR
        r.setRight(r.right() + dx);
        r.setTop(r.top() + dy);
        break;
    case 3: // R
        r.setRight(r.right() + dx);
        break;
    case 4: // BR
        r.setRight(r.right() + dx);
        r.setBottom(r.bottom() + dy);
        break;
    case 5: // B
        r.setBottom(r.bottom() + dy);
        break;
    case 6: // BL
        r.setLeft(r.left() + dx);
        r.setBottom(r.bottom() + dy);
        break;
    case 7: // L
        r.setLeft(r.left() + dx);
        break;
    default:
        return;
    }

    if (r.width() <= 0 || r.height() <= 0)
        return;

    const double newScaleX = r.width() / docBounds.width();
    const double newScaleY = r.height() / docBounds.height();

    // Expressa o ponto de ancoragem em coordenadas locais da camada (antes da
    // transformação), para poder reposicionar mantendo-o fixo após a escala.
    const QPointF anchorLocal = mat.inverted().map(anchorDocPos);
    const QPointF centerLocal = cb.center();

    t.scaleX = std::max(0.01, newScaleX);
    t.scaleY = std::max(0.01, newScaleY);

    // A nova position é aquela que coloca o ponto de ancoragem no mesmo lugar
    // do documento após aplicar escala em torno do centro local.
    const QPointF offsetFromCenter = anchorLocal - centerLocal;
    const QPointF newOffset(offsetFromCenter.x() * t.scaleX, offsetFromCenter.y() * t.scaleY);
    t.position = anchorDocPos - newOffset + (centerLocal - QPointF(0, 0));
}

// Aplica um gesto de rotação a uma única camada. O ângulo é medido entre o
// vetor centro->pontoInicial e o vetor centro->pontoAtual.
void applyRotate(Layer* /*layer*/, AffineTransform& t, const QPointF& docPos,
                 const QPointF& dragStartDocPos, const QPointF& centerDocPos)
{
    const double initialAngle = std::atan2(dragStartDocPos.y() - centerDocPos.y(),
                                           dragStartDocPos.x() - centerDocPos.x());
    const double currentAngle = std::atan2(docPos.y() - centerDocPos.y(),
                                           docPos.x() - centerDocPos.x());
    double deltaDeg = (currentAngle - initialAngle) * 180.0 / M_PI;

    // Normaliza delta para [-180, 180] para evitar saltos.
    while (deltaDeg > 180.0) deltaDeg -= 360.0;
    while (deltaDeg < -180.0) deltaDeg += 360.0;

    t.rotationDeg += deltaDeg;
}

} // namespace

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
    const QRectF selectionBounds = computeSelectionBounds(ctx);
    const QPointF selectionCenter = selectionBounds.isEmpty()
        ? QPointF()
        : selectionBounds.center();

    for (const auto& id : m_selectedLayers) {
        Layer* layer = ctx.document->findLayer(id);
        if (!layer || !m_initialTransforms.contains(id)) continue;

        AffineTransform t = m_initialTransforms[id];

        if (m_gesture == Gesture::Move) {
            t.position += delta;
            // Pela API do Document: clamp + bumpRevision + sinal, em vez de
            // mutar o membro direto (que não notificava nada).
            ctx.document->setLayerTransform(id, t);
        } else if (m_gesture == Gesture::Scale) {
            // Ponto de ancoragem: oposto ao handle arrastado.
            QPointF anchor = selectionCenter;
            if (m_selectedLayers.size() == 1) {
                const QRectF cb = layer->contentBounds();
                if (!cb.isEmpty()) {
                    const QTransform mat = layer->transform.matrix(cb);
                    const QRectF docBounds = mat.mapRect(cb);
                    if (!docBounds.isEmpty()) {
                        // O ponto fixo é o canto/lado oposto ao handle ativo.
                        switch (m_activeHandle) {
                        case 0: anchor = docBounds.bottomRight(); break; // TL <-> BR
                        case 1: anchor = QPointF(docBounds.center().x(), docBounds.bottom()); break; // T <-> base
                        case 2: anchor = docBounds.bottomLeft(); break; // TR <-> BL
                        case 3: anchor = QPointF(docBounds.left(), docBounds.center().y()); break; // R <-> centro esquerdo
                        case 4: anchor = docBounds.topLeft(); break; // BR <-> TL
                        case 5: anchor = QPointF(docBounds.center().x(), docBounds.top()); break; // B <-> topo
                        case 6: anchor = docBounds.topRight(); break; // BL <-> TR
                        case 7: anchor = QPointF(docBounds.right(), docBounds.center().y()); break; // L <-> centro direito
                        default: anchor = docBounds.center(); break;
                        }
                    }
                }
            }
            applyScale(layer, t, delta, m_activeHandle, anchor);
            ctx.document->setLayerTransform(id, t);
        } else if (m_gesture == Gesture::Rotate) {
            applyRotate(layer, t, docPos, m_dragStartDocPos, selectionCenter);
            ctx.document->setLayerTransform(id, t);
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
            // Um gesto = UM passo de undo (CompoundCommand), não um por camada.
            auto compound = std::make_unique<CompoundCommand>(
                QStringLiteral("layer.transform"));
            for (const auto& id : m_selectedLayers) {
                Layer* layer = ctx.document->findLayer(id);
                if (layer && m_initialTransforms.contains(id)) {
                    const AffineTransform oldT = m_initialTransforms[id];
                    const AffineTransform newT = layer->transform;
                    if (oldT != newT) {
                        compound->addCommand(std::make_unique<SetLayerTransformCommand>(
                            *ctx.document, id, oldT, newT
                        ));
                    }
                }
            }
            if (!compound->isEmpty())
                ctx.history->execute(std::move(compound));
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
