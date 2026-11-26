#pragma once

#include <QImage>
#include <QPointF>
#include <QTransform>
#include <QWidget>

#include <functional>

namespace cc {

class Document;

/// Painel Navigator: miniatura do documento com o retângulo do viewport.
/// Clicar/arrasar na miniatura centra o canvas nesse ponto do documento.
class NavigatorPanel final : public QWidget
{
    Q_OBJECT
public:
    explicit NavigatorPanel(QWidget* parent = nullptr);

    void setDocument(const Document* doc);
    /// Atualiza zoom/transformada/retângulo visível do canvas.
    void setViewport(double zoom, const QTransform& docToDevice,
                     const QSize& canvasWidgetSize);
    /// Fonte viva do estado do viewport (chamada por um timer interno para
    ///apanhar também o pan, que não tem sinal próprio).
    void setViewProvider(std::function<void()> provider);

signals:
    void navigateRequested(const QPointF& docPos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    QSize sizeHint() const override { return {200, 150}; }

private:
    void rebuildThumbnail();
    QPointF mapToDoc(const QPointF& widgetPos) const;
    QRectF thumbRect() const;

    const Document* m_doc = nullptr;
    QImage m_thumb;
    quint64 m_thumbRevision = 0;
    QSize m_thumbDocSize;

    double m_zoom = 1.0;
    QTransform m_docToDevice;
    QSize m_canvasSize;
    bool m_dragging = false;
    std::function<void()> m_viewProvider;
    class QTimer* m_timer = nullptr;
};

} // namespace cc
