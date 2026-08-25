#pragma once

#include <QPointF>
#include <QWidget>

namespace cc {

class Document;

/// Interactive document viewport: pan, zoom, painting via the software
/// renderer, and image drag-and-drop. Owns the view transform only - never
/// mutates the document.
class CanvasView final : public QWidget
{
    Q_OBJECT
public:
    explicit CanvasView(QWidget* parent = nullptr);

    void setDocument(Document* document);

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
    QTransform docToDevice() const;
    QTransform deviceToDoc() const;
    void zoomAt(const QPointF& widgetPos, double factor);
    void emitZoomChanged();

    Document* m_document = nullptr;
    double m_zoom = 1.0;
    QPointF m_panOffset{0, 0};
    bool m_panning = false;
    bool m_spacePanning = false;
    QPoint m_lastMousePos;
    bool m_needsFit = true;
};

} // namespace cc
