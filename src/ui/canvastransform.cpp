#include "canvastransform.h"

#include <algorithm>

namespace cc {

namespace {
constexpr double kFitMarginFactor = 0.92;
}

CanvasTransform::CanvasTransform(QObject* parent)
    : QObject(parent)
{
}

QTransform CanvasTransform::docToDevice() const
{
    QTransform transform;
    transform.translate(m_panOffset.x(), m_panOffset.y());
    transform.scale(m_zoom, m_zoom);
    return transform;
}

QTransform CanvasTransform::deviceToDoc() const
{
    return docToDevice().inverted();
}

void CanvasTransform::setViewportSize(const QSize& size)
{
    m_viewportSize = size;
}

void CanvasTransform::setDocumentSize(const QSize& size)
{
    if (m_documentSize == size)
        return;
    m_documentSize = size;
    m_needsFit = true;
}

bool CanvasTransform::hasGeometry() const
{
    return m_viewportSize.width() > 0 && m_viewportSize.height() > 0 &&
           m_documentSize.width() > 0 && m_documentSize.height() > 0;
}

void CanvasTransform::fitIfNeeded()
{
    if (m_needsFit && hasGeometry())
        fit();
}

void CanvasTransform::fit()
{
    if (!hasGeometry())
        return;

    const double zoomX = m_viewportSize.width() * kFitMarginFactor /
                         static_cast<double>(m_documentSize.width());
    const double zoomY = m_viewportSize.height() * kFitMarginFactor /
                         static_cast<double>(m_documentSize.height());
    m_zoom = std::clamp(std::min(zoomX, zoomY), kMinZoom, kMaxZoom);
    m_panOffset = QPointF(
        (m_viewportSize.width() - m_documentSize.width() * m_zoom) / 2.0,
        (m_viewportSize.height() - m_documentSize.height() * m_zoom) / 2.0);
    m_needsFit = false;
    m_autoFit = true;

    emit zoomChanged(m_zoom);
}

void CanvasTransform::zoomIn()
{
    zoomAt(QPointF(m_viewportSize.width() / 2.0, m_viewportSize.height() / 2.0),
           kZoomStep);
}

void CanvasTransform::zoomOut()
{
    zoomAt(QPointF(m_viewportSize.width() / 2.0, m_viewportSize.height() / 2.0),
           1.0 / kZoomStep);
}

void CanvasTransform::zoomTo(double zoom)
{
    zoom = std::clamp(zoom, kMinZoom, kMaxZoom);
    if (qFuzzyCompare(zoom, m_zoom))
        return;
    zoomAt(QPointF(m_viewportSize.width() / 2.0, m_viewportSize.height() / 2.0),
           zoom / m_zoom);
}

void CanvasTransform::zoomAt(const QPointF& widgetPos, double factor)
{
    const double newZoom = std::clamp(m_zoom * factor, kMinZoom, kMaxZoom);
    if (qFuzzyCompare(newZoom, m_zoom))
        return;

    // Zoom manual: o usuário assumiu o controle — resize não refaz o fit.
    m_autoFit = false;

    const double effective = newZoom / m_zoom;
    m_panOffset = widgetPos - (widgetPos - m_panOffset) * effective;
    m_zoom = newZoom;

    emit zoomChanged(m_zoom);
}

void CanvasTransform::centerOn(const QPointF& documentPos)
{
    m_panOffset = QPointF(m_viewportSize.width() / 2.0, m_viewportSize.height() / 2.0)
                  - documentPos * m_zoom;
}

void CanvasTransform::panBy(const QPointF& widgetDelta)
{
    m_panOffset += widgetDelta;
}

void CanvasTransform::keepCenteredOnResize(const QSize& oldSize, const QSize& newSize)
{
    if (!oldSize.isValid() || oldSize == newSize)
        return;
    m_panOffset += QPointF((newSize.width() - oldSize.width()) / 2.0,
                           (newSize.height() - oldSize.height()) / 2.0);
}

} // namespace cc