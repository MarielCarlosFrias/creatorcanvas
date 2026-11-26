#include "navigatorpanel.h"

#include "core/Document.h"
#include "rendering/CanvasRenderer.h"
#include "ui/designtokens.h"

#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

namespace cc {

NavigatorPanel::NavigatorPanel(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(140, 100);
    m_timer = new QTimer(this);
    m_timer->setInterval(250);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        if (m_viewProvider)
            m_viewProvider();
        update();
    });
}

void NavigatorPanel::setDocument(const Document* doc)
{
    m_doc = doc;
    m_thumbRevision = 0; // força rebuild
    m_thumb = QImage();
    update();
}

void NavigatorPanel::setViewport(double zoom, const QTransform& docToDevice,
                                 const QSize& canvasWidgetSize)
{
    m_zoom = zoom;
    m_docToDevice = docToDevice;
    m_canvasSize = canvasWidgetSize;
    update();
}

void NavigatorPanel::setViewProvider(std::function<void()> provider)
{
    m_viewProvider = std::move(provider);
}

void NavigatorPanel::showEvent(QShowEvent*)
{
    if (m_timer)
        m_timer->start();
}

void NavigatorPanel::hideEvent(QHideEvent*)
{
    if (m_timer)
        m_timer->stop();
}

QRectF NavigatorPanel::thumbRect() const
{
    // O rect depende do DOCUMENTO e do widget, nunca da miniatura existente.
    // Exigir m_thumb aqui impedia rebuildThumbnail() de criar a primeira.
    if (!m_doc || m_doc->width() <= 0 || m_doc->height() <= 0)
        return {};
    const double margin = 8.0;
    const double availW = width() - 2 * margin;
    const double availH = height() - 2 * margin;
    const double scale = qMin(availW / m_doc->width(), availH / m_doc->height());
    const QSizeF sz(m_doc->width() * scale, m_doc->height() * scale);
    return QRectF((width() - sz.width()) / 2.0, (height() - sz.height()) / 2.0,
                  sz.width(), sz.height());
}

void NavigatorPanel::rebuildThumbnail()
{
    if (!m_doc)
        return;
    if (!m_thumb.isNull() && m_thumbRevision == m_doc->revision()
        && m_thumbDocSize == QSize(m_doc->width(), m_doc->height()))
        return;

    const QRectF tr = thumbRect();
    if (tr.isEmpty())
        return;
    QImage img(tr.size().toSize(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter painter(&img);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        QTransform t;
        t.scale(tr.width() / m_doc->width(), tr.height() / m_doc->height());
        RenderOptions options;
        options.drawCheckerboard = true;
        options.canvasBorder = Qt::transparent;
        renderDocument(*m_doc, &painter, t, options);
    }
    m_thumb = std::move(img);
    m_thumbRevision = m_doc->revision();
    m_thumbDocSize = QSize(m_doc->width(), m_doc->height());
}

QPointF NavigatorPanel::mapToDoc(const QPointF& widgetPos) const
{
    const QRectF tr = thumbRect();
    if (tr.isEmpty() || !m_doc)
        return {};
    return QPointF((widgetPos.x() - tr.x()) / tr.width() * m_doc->width(),
                   (widgetPos.y() - tr.y()) / tr.height() * m_doc->height());
}

void NavigatorPanel::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    const bool dark = palette().color(QPalette::Window).value() < 128;
    painter.fillRect(rect(), dark ? Design::DWindow() : Design::LWindow());

    rebuildThumbnail();
    const QRectF tr = thumbRect();
    if (tr.isEmpty())
        return;

    painter.fillRect(tr, Qt::white);
    if (!m_thumb.isNull())
        painter.drawImage(tr, m_thumb);
    painter.setPen(QPen(dark ? QColor(0x50, 0x51, 0x57) : QColor(0xc8, 0xc8, 0xc8), 1));
    painter.drawRect(tr);

    // Retângulo do viewport: doc coords → device → miniatura.
    if (m_doc && m_canvasSize.isValid()) {
        bool invertible = false;
        const QTransform inv = m_docToDevice.inverted(&invertible);
        if (invertible) {
            const QRectF viewportDoc = inv.mapRect(
                QRectF(0, 0, m_canvasSize.width(), m_canvasSize.height()));
            const QRectF vpThumb(
                tr.x() + viewportDoc.x() / m_doc->width() * tr.width(),
                tr.y() + viewportDoc.y() / m_doc->height() * tr.height(),
                viewportDoc.width() / m_doc->width() * tr.width(),
                viewportDoc.height() / m_doc->height() * tr.height());
            QPen vpPen(Design::Accent());
            vpPen.setWidthF(1.5);
            vpPen.setCosmetic(true);
            painter.setPen(vpPen);
            painter.drawRect(vpThumb);
        }
    }
    Q_UNUSED(m_zoom);
}

void NavigatorPanel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        emit navigateRequested(mapToDoc(event->pos()));
    }
}

void NavigatorPanel::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton))
        emit navigateRequested(mapToDoc(event->pos()));
}

void NavigatorPanel::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        emit navigateRequested(mapToDoc(event->pos()));
    }
}

} // namespace cc
