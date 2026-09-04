#include "canvasview.h"

#include "rendering/CanvasRenderer.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QLineEdit>
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
    hideTextEdit();
    m_document = document;
    m_selectedId = LayerId();
    m_needsFit = true;
    update();
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
    drawSnapGuides(&painter);
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
                t.position += docNow - m_gestureStartDoc;
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
