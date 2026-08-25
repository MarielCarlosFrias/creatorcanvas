#pragma once

#include <QPointF>
#include <QWidget>

#include "core/Document.h"

namespace cc {

/// Interactive document viewport: pan, zoom, selection with transform
/// handles, gestures, and painting via the software renderer.
class CanvasView final : public QWidget
{
    Q_OBJECT
public:
    explicit CanvasView(QWidget* parent = nullptr);

    void setDocument(Document* document);
    void clearSelection();
    void centerOn(const QPointF& documentPos);
    void setSelectedLayer(const LayerId& id);

    double zoom() const { return m_zoom; }
    QPointF panOffset() const { return m_panOffset; }

public slots:
    void zoomIn();
    void zoomOut();
    void zoomTo(double zoom);
    void fitToViewport();

signals:
    void zoomChanged(double zoom);
    void cursorMoved(const QPointF& documentPos);
    void fileDropped(const QString& filePath);
    void selectionChanged(const cc::LayerId& id);
    void transformCommitted(const cc::LayerId& id,
                            const cc::AffineTransform& oldValue,
                            const cc::AffineTransform& newValue);
    void deleteRequested(const cc::LayerId& id);

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dragMoveEvent(QDragMoveEvent*) override;
    void dropEvent(QDropEvent*) override;

private:
    struct HandleSet
    {
        QPointF points[9];
        bool valid = false;
    };

    enum class Gesture { None, Move, Scale, Rotate };

    QTransform docToDevice() const;
    QTransform deviceToDoc() const;
    void zoomAt(const QPointF& widgetPos, double factor);
    void emitZoomChanged();
    Layer* hitTestLayer(const QPointF& docPos) const;
    HandleSet handlePositions(const Layer& layer) const;
    int handleAt(const QPointF& widgetPos) const;
    void drawSelectionOverlay(QPainter* painter);
    void selectLayer(const LayerId& id);
    void updateCursor(const QPointF& widgetPos);

    Document* m_document = nullptr;
    double m_zoom = 1.0;
    QPointF m_panOffset{0, 0};

    bool m_panning = false;
    bool m_spacePanning = false;
    QPoint m_lastMousePos;
    bool m_needsFit = true;

    LayerId m_selectedId;
    Gesture m_gesture = Gesture::None;
    int m_activeHandle = -1;
    AffineTransform m_gestureStart;
    QRectF m_gestureBounds;
    QPointF m_gestureStartDoc;
    QPointF m_fixedDoc;
    QPointF m_fixedLocal;
    QPointF m_localPress;
    QPointF m_rotateCenter;
    double m_rotateStartAngle = 0.0;
};

} // namespace cc
