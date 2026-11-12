#pragma once

#include "core/document/WebSlice.h"

#include <QWidget>

#include <vector>

namespace cc {

/// Miniatura interactiva do documento para o painel de fatias: desenha o
/// documento, as fatias guardadas e o rectângulo pendente, e permite
/// ARRASTAR esse pendente e redimensioná-lo pelos cantos com o rato —
/// escrevendo as novas coordenadas de volta nos campos (modo avançado).
class SlicePreviewWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit SlicePreviewWidget(QWidget* parent = nullptr);

    /// Rect do pendente em coordenadas de WIDGET (para testes e depuração).
    QRectF pendingWidgetRect() const;

    /// Define o conteúdo: imagem do documento já renderizada, fatias e o
    /// pendente (em coordenadas de documento).
    void setDocumentImage(const QImage& image);
    void setSlices(const std::vector<WebSlice>& slices);
    void setPending(const QRectF& docRect);

    /// Activa a interacção com o rato (modo avançado).
    void setInteractive(bool interactive);
    bool isInteractive() const { return m_interactive; }

    QSize sizeHint() const override { return QSize(320, 220); }

signals:
    /// Emitido a cada alteração do pendente pelo rato, em coordenadas de
    /// DOCUMENTO (o diálogo escreve de volta nos campos).
    void pendingEdited(const QRectF& docRect);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    enum class Drag { None, Move, TopLeft, TopRight, BottomLeft, BottomRight };

    QRectF documentRect() const;                 // rect do documento dentro do widget
    QPointF docFromWidget(const QPoint& p) const;
    Drag hitTest(const QPoint& widgetPos) const;
    void applyCursor(Drag mode);

    QImage m_documentImage;
    std::vector<WebSlice> m_slices;
    QRectF m_pending;
    bool m_interactive = false;

    Drag m_drag = Drag::None;
    QPointF m_grabOffset;
    QSizeF m_grabSize;
};

} // namespace cc
