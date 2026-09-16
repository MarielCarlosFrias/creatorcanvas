#pragma once

#include <QWidget>
#include <QPointer>
#include "core/Document.h"

class QListWidget;
class QListWidgetItem;
class QComboBox;
class QSlider;
class QSpinBox;
class QLabel;
class QToolButton;

namespace cc {

class I18nService;
class CommandStack;

/// Painel de camadas moderno estilo Photoshop / Figma:
/// - Miniaturas de alta definição em tempo real
/// - Ícones de identificação por tipo (Texto, Imagem, Forma, Pintura)
/// - Controles visuais inline de Visibilidade (olho) e Bloqueio (cadeado)
/// - Modos de Mesclagem e Opacidade com histórico Undo/Redo
/// - Barra de ações inferior: Nova Camada (+), Duplicar, Subir, Descer, Excluir
class LayersPanel final : public QWidget
{
    Q_OBJECT
public:
    explicit LayersPanel(I18nService* i18n, CommandStack* history = nullptr,
                         QWidget* parent = nullptr);

    void setDocument(Document* document);
    void setHistory(CommandStack* history);
    void refresh();
    void setSelectedLayer(const LayerId& id);
    void setSelectedLayers(const QList<LayerId>& ids);

signals:
    void selectionRequested(const cc::LayerId& id);
    void multiSelectionRequested(const QList<cc::LayerId>& ids);
    void duplicateRequested(const cc::LayerId& id);
    void deleteRequested(const cc::LayerId& id);
    void focusRequested(const QPointF& documentPos);
    void addTextRequested();
    void addShapeRequested();

private:
    void buildUi();
    void retranslateUi();
    void rebuild();
    void updateControlsForSelection();
    void onCurrentRowChanged(int row);
    void onItemDoubleClicked(QListWidgetItem* item);
    void onMoved();
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

    // Barra de ações inferior
    QToolButton* m_addLayerBtn = nullptr;
    QToolButton* m_duplicateBtn = nullptr;
    QToolButton* m_moveUpBtn = nullptr;
    QToolButton* m_moveDownBtn = nullptr;
    QToolButton* m_deleteBtn = nullptr;
};

} // namespace cc
