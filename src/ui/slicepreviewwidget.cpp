#include "slicepreviewwidget.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace cc {

SlicePreviewWidget::SlicePreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(160, 120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SlicePreviewWidget::setDocumentImage(const QImage& image, const QSize& documentSize)
{
    m_documentImage = image;
    if (documentSize.isValid() && documentSize.width() > 0 && documentSize.height() > 0)
        m_documentSize = documentSize;
    else if (!m_documentSize.isValid() && !image.isNull())
        m_documentSize = image.size();
    update();
}

void SlicePreviewWidget::setDocumentSize(const QSize& size)
{
    m_documentSize = size;
    update();
}

qreal SlicePreviewWidget::docWidth() const
{
    if (m_documentSize.isValid() && m_documentSize.width() > 0)
        return m_documentSize.width();
    return m_documentImage.isNull() ? 1.0 : m_documentImage.width();
}

qreal SlicePreviewWidget::docHeight() const
{
    if (m_documentSize.isValid() && m_documentSize.height() > 0)
        return m_documentSize.height();
    return m_documentImage.isNull() ? 1.0 : m_documentImage.height();
}

void SlicePreviewWidget::setSlices(const std::vector<WebSlice>& slices)
{
    m_slices = slices;
    update();
}

void SlicePreviewWidget::setPending(const QRectF& docRect)
{
    m_pending = docRect;
    update();
}

void SlicePreviewWidget::setInteractive(bool interactive)
{
    m_interactive = interactive;
    setMouseTracking(interactive);
    if (!interactive)
        unsetCursor();
}

QRectF SlicePreviewWidget::documentRect() const
{
    if (m_documentImage.isNull())
        return {};
    const qreal margin = 8.0;
    const qreal availW = std::max(1.0, static_cast<qreal>(width()) - 2 * margin);
    const qreal availH = std::max(1.0, static_cast<qreal>(height()) - 2 * margin);
    const qreal imageRatio = static_cast<qreal>(docWidth())
                             / std::max<qreal>(1.0, docHeight());
    const qreal availRatio = availW / availH;

    qreal w = availW;
    qreal hgt = availH;
    if (imageRatio > availRatio)
        hgt = availW / imageRatio;
    else
        w = availH * imageRatio;

    const qreal x = (width() - w) / 2.0;
    const qreal y = (height() - hgt) / 2.0;
    return QRectF(x, y, w, hgt);
}

QPointF SlicePreviewWidget::docFromWidget(const QPoint& p) const
{
    const QRectF doc = documentRect();
    if (doc.isEmpty() || m_documentImage.isNull())
        return {};
    const qreal sx = docWidth() / doc.width();
    const qreal sy = docHeight() / doc.height();
    return QPointF((p.x() - doc.left()) * sx, (p.y() - doc.top()) * sy);
}

QRectF SlicePreviewWidget::pendingWidgetRect() const
{
    const QRectF doc = documentRect();
    if (doc.isEmpty() || m_pending.isNull())
        return {};
    const qreal sx = doc.width() / std::max<qreal>(1.0, docWidth());
    const qreal sy = doc.height() / std::max<qreal>(1.0, docHeight());
    return QRectF(doc.left() + m_pending.left() * sx,
                  doc.top() + m_pending.top() * sy,
                  m_pending.width() * sx,
                  m_pending.height() * sy);
}

SlicePreviewWidget::Drag SlicePreviewWidget::hitTest(const QPoint& widgetPos) const
{
    if (!m_interactive)
        return Drag::None;
    const QRectF r = pendingWidgetRect();
    if (r.isNull() || r.isEmpty())
        return Drag::None;

    const qreal grab = 7.0;
    const QPointF p(widgetPos);
    if (QRectF(r.topLeft() - QPointF(grab, grab), QSizeF(2 * grab, 2 * grab)).contains(p))
        return Drag::TopLeft;
    if (QRectF(r.topRight() - QPointF(grab, grab), QSizeF(2 * grab, 2 * grab)).contains(p))
        return Drag::TopRight;
    if (QRectF(r.bottomLeft() - QPointF(grab, grab), QSizeF(2 * grab, 2 * grab)).contains(p))
        return Drag::BottomLeft;
    if (QRectF(r.bottomRight() - QPointF(grab, grab), QSizeF(2 * grab, 2 * grab)).contains(p))
        return Drag::BottomRight;
    if (r.contains(p))
        return Drag::Move;
    return Drag::None;
}

void SlicePreviewWidget::applyCursor(Drag mode, const QPoint& pos)
{
    switch (mode) {
    case Drag::Move:
        setCursor(Qt::SizeAllCursor);
        break;
    case Drag::TopLeft:
    case Drag::BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case Drag::TopRight:
    case Drag::BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case Drag::Create:
        setCursor(Qt::CrossCursor);
        break;
    case Drag::None:
        if (!pos.isNull() && documentRect().contains(pos))
            setCursor(Qt::CrossCursor);
        else
            unsetCursor();
        break;
    }
}

void SlicePreviewWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x1a, 0x1c, 0x20));

    const QRectF doc = documentRect();
    if (m_documentImage.isNull() || doc.isEmpty()) {
        p.setPen(QColor(0x88, 0x88, 0x88));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("—"));
        return;
    }

    p.drawImage(doc, m_documentImage);
    p.setPen(QPen(QColor(0x2b, 0x30, 0x3c), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(doc);

    // Fatias guardadas: tracejado fino, cores cíclicas.
    static const QColor kPalette[] = {
        QColor(0x43, 0x78, 0xff), QColor(0x22, 0xc5, 0x5e),
        QColor(0xf5, 0x9e, 0x0b), QColor(0xec, 0x48, 0x99),
        QColor(0x8b, 0x5c, 0xf6), QColor(0x14, 0xb8, 0xa6),
    };
    QFont labelFont = p.font();
    labelFont.setPointSize(8);
    labelFont.setBold(true);
    p.setFont(labelFont);

    const qreal sx = doc.width() / std::max<qreal>(1.0, docWidth());
    const qreal sy = doc.height() / std::max<qreal>(1.0, docHeight());
    auto toWidget = [&](const QRectF& r) {
        return QRectF(doc.left() + r.left() * sx, doc.top() + r.top() * sy,
                      r.width() * sx, r.height() * sy);
    };

    for (size_t i = 0; i < m_slices.size(); ++i) {
        const QRectF r = toWidget(m_slices[i].rect);
        const QColor color = kPalette[i % (sizeof(kPalette) / sizeof(kPalette[0]))];
        p.setPen(QPen(color, 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
        const QString label = m_slices[i].name.isEmpty()
            ? m_slices[i].exportFormat.toUpper() : m_slices[i].name;
        const QFontMetrics fm(labelFont);
        const int tw = fm.horizontalAdvance(label) + 6;
        QRectF lr(r.left(), std::max(0.0, r.top() - 12), tw, 12);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(color.red(), color.green(), color.blue(), 200));
        p.drawRect(lr);
        p.setPen(Qt::white);
        p.drawText(lr, Qt::AlignCenter, label);
    }

    // Pendente: azul sólido + pegas nos cantos quando interactivo.
    const QRectF pr = toWidget(m_pending);
    p.setPen(QPen(QColor(0x43, 0x78, 0xff), 2, Qt::SolidLine));
    p.setBrush(Qt::NoBrush);
    p.drawRect(pr);
    if (m_interactive && !pr.isNull()) {
        p.setBrush(QColor(0x43, 0x78, 0xff));
        for (const QPointF& corner : {pr.topLeft(), pr.topRight(),
                                      pr.bottomLeft(), pr.bottomRight()}) {
            p.drawRect(QRectF(corner.x() - 4, corner.y() - 4, 8, 8));
        }
    }
    const QString pendingLabel = QStringLiteral("pendente");
    const QFontMetrics fm2(labelFont);
    const int tw2 = fm2.horizontalAdvance(pendingLabel) + 6;
    QRectF lr2(pr.left(), std::max(0.0, pr.top() - 12), tw2, 12);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x43, 0x78, 0xff, 220));
    p.drawRect(lr2);
    p.setPen(Qt::white);
    p.drawText(lr2, Qt::AlignCenter, pendingLabel);
}

void SlicePreviewWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_interactive || event->button() != Qt::LeftButton)
        return;
    m_drag = hitTest(event->pos());
    const QPointF docPos = docFromWidget(event->pos());
    if (m_drag == Drag::None) {
        // Se clicar em cima do documento na prévia, inicia a seleção/desenho de uma nova fatia
        const QRectF doc = documentRect();
        if (doc.contains(event->pos())) {
            m_drag = Drag::Create;
            m_createStart = docPos;
            m_pending = QRectF(docPos, QSizeF(1, 1));
            emit pendingEdited(m_pending);
            update();
            event->accept();
        }
        return;
    }
    m_grabOffset = docPos - QPointF(m_pending.left(), m_pending.top());
    m_grabSize = QSizeF(m_pending.width(), m_pending.height());
    event->accept();
}

void SlicePreviewWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_interactive)
        return;

    if (m_drag == Drag::None) {
        applyCursor(hitTest(event->pos()), event->pos());
        return;
    }

    const QPointF docPos = docFromWidget(event->pos());
    const qreal docW = docWidth();
    const qreal docH = docHeight();
    QRectF next = m_pending;

    if (m_drag == Drag::Create) {
        const qreal minX = std::clamp(std::min(m_createStart.x(), docPos.x()), 0.0, docW);
        const qreal maxX = std::clamp(std::max(m_createStart.x(), docPos.x()), 0.0, docW);
        const qreal minY = std::clamp(std::min(m_createStart.y(), docPos.y()), 0.0, docH);
        const qreal maxY = std::clamp(std::max(m_createStart.y(), docPos.y()), 0.0, docH);
        next = QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
        const qreal minSize = 4.0;
        if (next.width() < minSize)
            next.setWidth(minSize);
        if (next.height() < minSize)
            next.setHeight(minSize);
        m_pending = next;
        emit pendingEdited(next.normalized());
        update();
        event->accept();
        return;
    }

    switch (m_drag) {
    case Drag::Move:
        next = QRectF(docPos.x() - m_grabOffset.x(), docPos.y() - m_grabOffset.y(),
                      m_grabSize.width(), m_grabSize.height());
        break;
    case Drag::TopLeft:
        next = QRectF(QPointF(docPos.x() - m_grabOffset.x(), docPos.y() - m_grabOffset.y()),
                      QPointF(m_pending.right(), m_pending.bottom()));
        break;
    case Drag::TopRight:
        next = QRectF(QPointF(m_pending.left(), docPos.y() - m_grabOffset.y()),
                      QPointF(docPos.x() - m_grabOffset.x(), m_pending.bottom()));
        break;
    case Drag::BottomLeft:
        next = QRectF(QPointF(docPos.x() - m_grabOffset.x(), m_pending.top()),
                      QPointF(m_pending.right(), docPos.y() - m_grabOffset.y()));
        break;
    case Drag::BottomRight:
        next = QRectF(QPointF(m_pending.left(), m_pending.top()),
                      QPointF(docPos.x() - m_grabOffset.x(), docPos.y() - m_grabOffset.y()));
        break;
    case Drag::Create:
    case Drag::None:
        return;
    }

    // Normaliza (arrastar um canto para lá do oposto inverte) e mantém
    // dentro do documento com um mínimo de 4px.
    next = next.normalized();
    const qreal minSize = 4.0;
    if (next.width() < minSize)
        next.setWidth(minSize);
    if (next.height() < minSize)
        next.setHeight(minSize);
    const double maxLeft = std::max(0.0, docW - next.width());
    const double maxTop = std::max(0.0, docH - next.height());
    next.moveLeft(std::clamp(next.left(), 0.0, maxLeft));
    next.moveTop(std::clamp(next.top(), 0.0, maxTop));

    m_pending = next;
    emit pendingEdited(next.normalized());
    update();
    event->accept();
}

void SlicePreviewWidget::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event)
    m_drag = Drag::None;
    applyCursor(Drag::None);
}

} // namespace cc
