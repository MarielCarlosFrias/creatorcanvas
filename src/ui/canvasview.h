#pragma once

#include <QPointF>
#include <QWidget>

#include <QPointer>
#include "core/Document.h"
#include "core/snap/SnapEngine.h"

class QLineEdit;

namespace cc {

class I18nService;

enum class CanvasTool {
    Select,
    Crop,
    Scissors,
    MagicWand,
    CloneStamp
};

/// Interactive document viewport: pan, zoom, selection with transform
/// handles, gestures, inline text editing, software rendering, and raster tools.
class CanvasView final : public QWidget
{
    Q_OBJECT
public:
    explicit CanvasView(QWidget* parent = nullptr);

    void setDocument(Document* document);
    void setI18n(I18nService* i18n);
    void clearSelection();
    void setSelectedLayer(const LayerId& id);
    void beginTextEdit(const LayerId& id);

    void setTool(CanvasTool tool);
    CanvasTool currentTool() const { return m_tool; }

    // Configurações do Corte (Crop)
    void setCropAspectRatio(double ratio);
    void applyCrop();
    void cancelCrop();

    // Configurações da Tesoura (Scissors Cut)
    void setScissorsKeepInside(bool keepInside);
    void setScissorsAutoCrop(bool autoCrop);
    void applyScissorsCut();
    void cancelScissorsCut();

    // Configurações da Varinha Mágica (Magic Wand)
    void setWandTolerance(int tolerance);
    void setWandContiguous(bool contiguous);

    // Configurações do Carimbo (Clone Stamp)
    void setCloneRadius(int radius);
    void setCloneHardness(qreal hardness);
    void setCloneOpacity(qreal opacity);

    double zoom() const { return m_zoom; }
    QPointF panOffset() const { return m_panOffset; }
    void centerOn(const QPointF& documentPos);

public slots:
    void zoomIn();
    void zoomOut();
    void zoomTo(double zoom);
    void fitToViewport();

signals:
    void toolChanged(CanvasTool tool);
    void zoomChanged(double zoom);
    void cursorMoved(const QPointF& documentPos);
    void fileDropped(const QString& filePath);
    void selectionChanged(const cc::LayerId& id);
    void transformCommitted(const cc::LayerId& id,
                            const cc::AffineTransform& oldValue,
                            const cc::AffineTransform& newValue);
    void textCommitted(const cc::LayerId& id,
                       const QString& oldValue,
                       const QString& newValue);
    void deleteRequested(const cc::LayerId& id);
    void duplicateRequested(const cc::LayerId& id);
    void textBoxCommitted(const cc::LayerId& id,
                          const QSizeF& oldValue, const QSizeF& newValue);
    void imageLayerModified(const cc::LayerId& id,
                            const cc::LayerId& oldAssetId, int oldWidth, int oldHeight,
                            const cc::AffineTransform& oldTransform,
                            const cc::LayerId& newAssetId, int newWidth, int newHeight,
                            const cc::AffineTransform& newTransform,
                            const QString& actionName);
    void statusMessageRequested(const QString& message);

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dragMoveEvent(QDragMoveEvent*) override;
    void dropEvent(QDropEvent*) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

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
    void drawSnapGuides(QPainter* painter);
    void collectSnapTargets(GroupLayer* group, const LayerId& excludeId,
                            QList<Layer*>& out) const;
    void selectLayer(const LayerId& id);
    void updateCursor(const QPointF& widgetPos);
    void commitTextEdit();
    void hideTextEdit();

    QPointer<Document> m_document;
    double m_zoom = 1.0;
    QPointF m_panOffset{0, 0};

    bool m_panning = false;
    bool m_spacePanning = false;
    SnapEngine m_snapEngine;
    QList<GuideLine> m_activeGuides;
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
    QSizeF m_gestureStartBox;

    QLineEdit* m_textEditor = nullptr;
    LayerId m_editingTextId;
    I18nService* m_i18n = nullptr;

    // Ferramentas Raster (Crop, Scissors, Magic Wand, Clone Stamp)
    void drawCropOverlay(QPainter* painter);
    void drawScissorsOverlay(QPainter* painter);
    void drawCloneOverlay(QPainter* painter);
    int cropHandleAt(const QPointF& widgetPos) const;
    void applyMagicWand(const QPointF& docPos);

    CanvasTool m_tool = CanvasTool::Select;

    // Corte (Crop)
    QRectF m_cropRect;
    double m_cropAspectRatio = 0.0;
    int m_activeCropHandle = -1;
    QPointF m_cropDragStartLocal;
    QRectF m_cropStartRect;

    // Corte com Tesoura (Scissors Cut)
    QPolygonF m_scissorsPolygon;
    QPointF m_scissorsCurrentHover;
    bool m_scissorsKeepInside = true;
    bool m_scissorsAutoCrop = true;
    bool m_isScissorsDrawing = false;

    // Varinha Mágica (Magic Wand)
    int m_wandTolerance = 25;
    bool m_wandContiguous = true;

    // Carimbo de Clonagem (Clone Stamp)
    int m_cloneRadius = 20;
    qreal m_cloneHardness = 0.8;
    qreal m_cloneOpacity = 1.0;
    bool m_hasCloneSrc = false;
    QPoint m_cloneSrcPoint;
    LayerId m_cloneSrcLayerId;
    QPoint m_cloneLastDstPoint;
    QImage m_cloneWorkingImage;
    bool m_isCloning = false;
    QPointF m_cloneHoverDocPos;
};

} // namespace cc
