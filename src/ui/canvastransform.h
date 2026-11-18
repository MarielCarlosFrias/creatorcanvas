#pragma once

#include <QObject>
#include <QPointF>
#include <QSize>
#include <QTransform>

namespace cc {

/// Viewport transform of a CanvasView: zoom factor, pan offset, "fit to
/// window" bookkeeping and the pixel math around them.
///
/// Extracted from CanvasView (which used to carry this state inline) so the
/// math is a small, independently testable unit with no widget attached:
/// fitting, zoom clamping, anchor-point zooming and resize re-centering are
/// all pure functions of (viewport size, document size, zoom, pan).
class CanvasTransform final : public QObject
{
    Q_OBJECT
public:
    static constexpr double kMinZoom = 0.05;
    static constexpr double kMaxZoom = 32.0;
    static constexpr double kZoomStep = 1.2;

    explicit CanvasTransform(QObject* parent = nullptr);

    [[nodiscard]] double zoom() const { return m_zoom; }
    [[nodiscard]] QPointF panOffset() const { return m_panOffset; }
    /// True while a window resize must re-fit the document.
    [[nodiscard]] bool autoFit() const { return m_autoFit; }

    /// Document -> widget pixel mapping (pan then scale).
    [[nodiscard]] QTransform docToDevice() const;
    [[nodiscard]] QTransform deviceToDoc() const;

    /// Widget size in pixels (drives fit and centering).
    void setViewportSize(const QSize& size);
    /// Document size in pixels; a change re-arms the pending fit.
    void setDocumentSize(const QSize& size);
    /// A new document arrived: re-fit on the next paint (lazy, so a document
    /// can be set before the widget has its final geometry).
    void resetForDocument() { m_needsFit = true; }

    /// Fits only if a document was (re)loaded and the geometry is known.
    void fitIfNeeded();

    /// Fits the document into the viewport (92 % margin), centred.
    void fit();

    /// Turns the "re-fit on resize" mode on/off (a manual pan or zoom turns
    /// it off; fitting turns it back on).
    void setAutoFit(bool on) { m_autoFit = on; }

    void zoomIn();
    void zoomOut();
    /// Absolute zoom (clamped); anchored at the viewport centre.
    void zoomTo(double zoom);
    /// Multiplicative zoom anchored at a widget position (wheel, gestures).
    void zoomAt(const QPointF& widgetPos, double factor);
    /// Scrolls so a document point lands at the viewport centre.
    void centerOn(const QPointF& documentPos);
    /// Drag-pan by a widget-space delta.
    void panBy(const QPointF& widgetDelta);
    /// Keeps the content centred after a viewport resize while in manual
    /// zoom (re-fit is handled by fit(), driven by autoFit()).
    void keepCenteredOnResize(const QSize& oldSize, const QSize& newSize);

signals:
    void zoomChanged(double zoom);

private:
    /// True when both the viewport and the document have a usable size.
    [[nodiscard]] bool hasGeometry() const;

    double m_zoom = 1.0;
    QPointF m_panOffset{0.0, 0.0};
    bool m_autoFit = true;
    bool m_needsFit = true;
    QSize m_viewportSize;
    QSize m_documentSize;
};

} // namespace cc