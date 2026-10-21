#pragma once

#include <QWidget>
#include <QPointer>
#include "core/Document.h"
#include "core/DocumentController.h"

class QListWidget;
class QListWidgetItem;
class QComboBox;
class QSlider;
class QSpinBox;
class QLabel;
class QToolButton;
class QLineEdit;
class QResizeEvent;

namespace cc {

class I18nService;
class CommandStack;

/// Painel de camadas moderno estilo Photoshop / Figma:
/// - Miniaturas de alta definição em tempo real
/// - Ícones de identificação por tipo (Texto, Imagem, Forma, Pintura)
/// - Controles visuais inline de Visibilidade (olho) e Bloqueio (cadeado)
/// - Modos de Mesclagem e Opacidade com histórico Undo/Redo
/// - Barra de ações inferior: Nova Camada (+), Duplicar, Subir, Descer, Excluir
class LayersPanel final : public QWidget, public IDocumentObserver
{
    Q_OBJECT
public:
    explicit LayersPanel(I18nService* i18n, CommandStack* history = nullptr,
                         QWidget* parent = nullptr);

    void onDocumentChanged(Document* doc) override { setDocument(doc); }
    void setDocument(Document* document);
    void setHistory(CommandStack* history);
    void refresh();
    void refreshProperties();
    void setSelectedLayer(const LayerId& id);
    void setSelectedLayers(const QList<LayerId>& ids);
    LayerId selectedLayer() const { return m_selectedId; }

signals:
    void selectionRequested(const cc::LayerId& id);
    void multiSelectionRequested(const QList<cc::LayerId>& ids);
    void duplicateRequested(const cc::LayerId& id);
    void deleteRequested(const cc::LayerId& id);
    void focusRequested(const QPointF& documentPos);
    void addTextRequested();
    void addShapeRequested();
    void quickExportRequested(const cc::LayerId& id);
    void mergeRequested();
    void flattenRequested();

private:
    void buildUi();
    void retranslateUi();
    void rebuild();
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void updateControlsForSelection();
    void onCurrentRowChanged(int row);
    void onItemDoubleClicked(QListWidgetItem* item);
    void onMoved();
    void onLayerReordered(const LayerId& id, int targetDocIndex);
    void showContextMenu(const QPoint& pos);
    int docIndexFromRow(int row) const;
    LayerId layerIdFromRow(int row) const;
    QPixmap renderLayerThumbnail(const Layer& layer, int size = 32) const;

    QPointer<Document> m_document;
    CommandStack* m_history = nullptr;
    I18nService* m_i18n = nullptr;
    QListWidget* m_list = nullptr;
    bool m_updating = false;
    LayerId m_selectedId;
    float m_opacityBeforeSlide = 1.0f;

    // Controles superiores de mesclagem e opacidade
    QLabel* m_blendLabel = nullptr;
    QComboBox* m_blendCombo = nullptr;
    QLabel* m_opacityLabel = nullptr;
    QSlider* m_opacitySlider = nullptr;
    QSpinBox* m_opacitySpin = nullptr;

    // Fill Opacity + Clipped + Locks granulares (Photocraft/Photoshop)
    QLabel* m_fillOpacityLabel = nullptr;
    QSlider* m_fillOpacitySlider = nullptr;
    QSpinBox* m_fillOpacitySpin = nullptr;
    QToolButton* m_lockTransparencyBtn = nullptr;
    QToolButton* m_lockPixelsBtn = nullptr;
    QToolButton* m_lockPositionBtn = nullptr;
    QToolButton* m_lockAllBtn = nullptr;
    QToolButton* m_clippedBtn = nullptr;
    QWidget* m_lockRow = nullptr;
    float m_fillOpacityBeforeSlide = 1.0f;
    bool m_clippedBefore = false;

    // Parâmetros de Adjustment Layer (contextual: só visível com ajuste
    // selecionado — como o painel Properties do Photoshop)
    QWidget* m_adjustParamsWidget = nullptr;
    QSlider* m_adjustParam1Slider = nullptr;
    QSpinBox* m_adjustParam1Spin = nullptr;
    QLabel* m_adjustParam1Label = nullptr;
    QSlider* m_adjustParam2Slider = nullptr;
    QSpinBox* m_adjustParam2Spin = nullptr;
    QLabel* m_adjustParam2Label = nullptr;
    QString m_adjustParam1Key;
    QString m_adjustParam2Key;
    double m_adjustParam1Before = 0.0;
    double m_adjustParam2Before = 0.0;

    // Barra de ações inferior
    QToolButton* m_addLayerBtn = nullptr;
    QToolButton* m_duplicateBtn = nullptr;
    QToolButton* m_moveUpBtn = nullptr;
    QToolButton* m_moveDownBtn = nullptr;
    QToolButton* m_deleteBtn = nullptr;
    QToolButton* m_applyMaskBtn = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_typeFilter = nullptr;
};

} // namespace cc
