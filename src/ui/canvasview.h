#pragma once

#include <QPointF>
#include <QPolygonF>
#include <QImage>
#include <QWidget>

#include <QPointer>
#include <atomic>
#include <memory>
#include "core/Document.h"
#include "core/DocumentController.h"
#include "core/snap/SnapEngine.h"
#include "canvasoverlayrenderer.h"
#include "canvasgesturehandler.h"
#include "canvastransform.h"
#include "tools/CanvasTool.h"

namespace cc {

class InlineTextEditor;
class I18nService;
class CommandStack;
class SelectTool;
class CropTool;
class ScissorsTool;
class PaintTool;
class MagicWandTool;
class CloneStampTool;
class FloodFillTool;
class MarqueeTool;
class GradientTool;
class RetouchTool;
class HealingTool;
class PenTool;
class VectorNodeTool;

/// Interactive document viewport: pan, zoom, selection with transform
/// handles, gestures, inline text editing, software rendering, and raster tools.
class CanvasView final : public QWidget, public IDocumentObserver
{
    Q_OBJECT
public:
    explicit CanvasView(QWidget* parent = nullptr);
    ~CanvasView() override;

    void onDocumentChanged(Document* doc) override { setDocument(doc); }
    void setDocument(Document* document);
    /// Re-lê as dimensões do documento (após Canvas Size / Rotate Canvas) e
    /// redesenha — o CanvasTransform guarda o tamanho em cache.
    void refreshDocumentGeometry();
    void setI18n(I18nService* i18n);
    void setHistory(CommandStack* history);
    void clearSelection();
    /// Remove da seleção ids que não existem mais no documento (após
    /// undo/redo de grupo/remoção) — evita estado pendurado que podia
    /// crashar o delete/ações de camada no MainWindow.
    void pruneSelection();
    void setSelectedLayer(const LayerId& id);
    LayerId selectedLayerId() const { return m_selectedId; }
    QList<LayerId> selectedLayers() const;
    void setMultiSelection(const QList<LayerId>& ids);
    void beginTextEdit(const LayerId& id);
    void setImagePreview(const LayerId& id, const QImage& image);
    void clearImagePreview();

    void setTool(CanvasTool tool);
    CanvasTool currentTool() const { return m_tool; }

    SelectTool* selectTool() const { return m_selectTool.get(); }
    CropTool* cropTool() const { return m_cropTool.get(); }
    ScissorsTool* scissorsTool() const { return m_scissorsTool.get(); }
    PaintTool* paintTool() const { return m_paintTool.get(); }
    MagicWandTool* wandTool() const { return m_wandTool.get(); }
    CloneStampTool* cloneTool() const { return m_cloneTool.get(); }
    FloodFillTool* floodTool() const { return m_floodTool.get(); }
    MarqueeTool* marqueeTool() const;
    GradientTool* gradientTool() const;
    RetouchTool* retouchTool() const;
    HealingTool* healingTool() const;

    // Opções das ferramentas novas
    void setMarqueeEllipse(bool ellipse);
    void setMarqueeFeather(int feather);
    void setGradientRadial(bool radial);
    void setGradientStartColor(const QColor& color);
    void setGradientEndColor(const QColor& color);
    void setGradientOpacity(qreal opacity);
    void setRetouchMode(int mode);
    void setRetouchSize(int size);
    void setRetouchStrength(qreal strength);
    void setHealingMode(int mode);
    void setHealingRadius(int radius);
    void setHealingStrength(qreal strength);

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
    void openSelectionActionsMenu();

    // Configurações do Carimbo (Clone Stamp)
    void setCloneRadius(int radius);
    void setCloneHardness(qreal hardness);
    void setCloneOpacity(qreal opacity);

    // Configurações de Pintura Estilo Paint
    void setPaintBrush(int brushType); // ImageProcessing::BrushType
    void setPaintColor(const QColor& color);

    /// Opções da caneta (traço do caminho Bézier).
    void setPenStrokeColor(const QColor& color);
    void setPenStrokeWidth(double width);
    void setPaintSize(int size);
    void setPaintOpacity(qreal opacity);

    // Remoção de Fundo com IA
    void openAiBackgroundRemoval(const cc::LayerId& id);
    /// Abre o mesmo motor de IA em modo seleção: a silhueta vira uma
    /// seleção de píxeis (não-destrutivo) em vez de descartar o fundo.
    void selectSubjectWithAi(const cc::LayerId& id);
    void removeBackgroundAiQuick(const cc::LayerId& id);
    bool isQuickAiRunning() const;

    double zoom() const;
    QPointF panOffset() const;
    void centerOn(const QPointF& documentPos);

    void setShowGrid(bool show);
    bool showGrid() const { return m_showGrid; }
    void setSnapToGrid(bool snap);
    bool snapToGrid() const { return m_snapToGrid; }
    void setGridSpacing(int spacing);
    int gridSpacing() const { return m_gridSpacing; }
    void setSafeZoneMode(int mode);
    int safeZoneMode() const { return m_safeZoneMode; }

    void setShowRulers(bool on);
    bool showRulers() const { return m_showRulers; }
    void setShowGuides(bool on);
    bool showGuides() const { return m_showGuides; }

    /// Starts a four-corner selection used by the mockup/logo tool. Points
    /// are collected directly on the document so the user never needs to
    /// type canvas coordinates. While placement is active the given logo is
    /// warped live onto the corners (Photoshop-like preview).
    void beginMockupPlacement(const QImage& logo, bool freeShape = false);
    void cancelMockupPlacement();

    /// Amostra a cor composta na coordenada do documento
    QColor sampleColorAt(const QPointF& docPos) const;

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
    void multiSelectionChanged(const QList<cc::LayerId>& ids);
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
    void statusMessageRequested(const QString& message);
    void quickAiBusyChanged(bool busy);
    void mockupAreaSelected(const QPolygonF& corners);
    void mockupPlacementCancelled();
    void colorPicked(const QColor& color);

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
    void leaveEvent(QEvent*) override;
    void changeEvent(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dragMoveEvent(QDragMoveEvent*) override;
    void dropEvent(QDropEvent*) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    using HandleSet = CanvasHandles;
    using Gesture = CanvasGestureType;

public:
    /// Conversão de coordenadas (exposta para testes e sincronia de UI).
    QTransform docToDevice() const;
    QTransform deviceToDoc() const;

private:
    Layer* hitTestLayer(const QPointF& docPos, bool includeLocked = false) const;
    Layer* hitTestChildrenRecursive(const GroupLayer& group, const QPointF& docPos,
                                    bool includeLocked = false,
                                    const QTransform& parentToDoc = QTransform(),
                                    float parentOpacity = 1.0f) const;
    bool imagePixelTransparent(const ImageLayer& image, const QPointF& local) const;
    HandleSet handlePositions(const Layer& layer) const;
    int handleAt(const QPointF& widgetPos) const;
    void collectSnapTargets(GroupLayer* group, const LayerId& excludeId,
                            QList<Layer*>& out) const;
    void selectLayer(const LayerId& id);
    void updateCursor(const QPointF& widgetPos);
    void commitTextEdit();
    void hideTextEdit();

    void addToSelection(const LayerId& id);
    void removeFromSelection(const LayerId& id);
    void finishMockupPlacement();
    QList<Layer*> hitTestRubberBand(const QRectF& docRect,
                                    bool containment = false) const;

    QPointer<Document> m_document;
    CommandStack* m_history = nullptr;
    /// Zoom/pan/fit state and math (see canvastransform.h).
    CanvasTransform* m_transform = nullptr;

    bool m_panning = false;
    bool m_spacePanning = false;
    SnapEngine m_snapEngine;
    QList<GuideLine> m_activeGuides;
    QPoint m_lastMousePos;

    LayerId m_selectedId;
    CanvasGestureHandler m_gestureHandler;

    InlineTextEditor* m_inlineTextEditor = nullptr;
    I18nService* m_i18n = nullptr;

    // Multi-selection
    QList<LayerId> m_multiSelection;
    bool m_rubberBanding = false;
    QPointF m_rubberBandStart;
    QPointF m_rubberBandCurrent;
    QList<AffineTransform> m_multiGestureStarts;

    // Grid & Safe Zones
    bool m_showGrid = false;
    bool m_snapToGrid = false;
    LayerId m_previewLayerId;
    QImage m_previewImage;
    int m_gridSpacing = 50;
    int m_safeZoneMode = 0; // 0=None, 1=YouTube, 2=Instagram, 3=TikTok
    bool m_showRulers = false;
    bool m_showGuides = true;
    bool m_draggingGuide = false;
    bool m_dragGuideHorizontal = true;
    double m_dragGuidePosition = 0.0;

    bool m_mockupPlacementActive = false;
    QPolygonF m_mockupCorners;
    QImage m_mockupPlacementLogo;
    bool m_mockupPlacementFreeShape = false;
    /// Detector de duplo clique: o segundo press de um duplo clique chega
    /// aqui como MouseButtonPress — não deve criar um ponto novo.
    qint64 m_lastMockupPressMs = 0;
    QPointF m_lastMockupPressPos;

    ToolContext makeToolContext() const;

    CanvasTool m_tool = CanvasTool::Select;
    std::unique_ptr<SelectTool> m_selectTool;
    std::unique_ptr<CropTool> m_cropTool;
    std::unique_ptr<ScissorsTool> m_scissorsTool;
    std::unique_ptr<PaintTool> m_paintTool;
    std::unique_ptr<MagicWandTool> m_wandTool;
    std::unique_ptr<CloneStampTool> m_cloneTool;
    std::unique_ptr<FloodFillTool> m_floodTool;
    std::unique_ptr<MarqueeTool> m_marqueeTool;
    std::unique_ptr<GradientTool> m_gradientTool;
    std::unique_ptr<RetouchTool> m_retouchTool;
    std::unique_ptr<HealingTool> m_healingTool;
    std::unique_ptr<PenTool> m_penTool;
    std::unique_ptr<VectorNodeTool> m_vectorNodeTool;
    ICanvasTool* m_activeTool = nullptr;

    // AI Quick Background Removal cancel flag and thread tracking
    std::shared_ptr<std::atomic<bool>> m_quickAiCancelFlag;
    bool m_advancedMode = false;

    QPointer<QThread> m_quickAiThread;

    /// Cache da sombra ambiente da prancheta: 16 fillRects por pintura
    /// custavam ~13 ms num canvas grande; agora é um pixmap blitado,
    /// recalculado só quando o tamanho do canvas ou o tema mudam.
    /// Overlay do contorno da selecção de pixéis: era um loop de CPU sobre
    /// toda a máscara em CADA pintura (~3M pixels numa foto grande). Agora é
    /// uma imagem cacheada, refeita só quando a máscara muda.
    QImage m_selectionOverlay;
    qint64 m_selectionOverlayKey = 0;

    QPixmap m_shadowCache;
    QSize m_shadowCacheSize;
    bool m_shadowCacheDark = false;
    LayerId m_pendingQuickAiLayerId;
};

} // namespace cc
