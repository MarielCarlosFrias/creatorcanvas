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

namespace cc {

class I18nService;
class CommandStack;

/// Lista de camadas estilo Adobe Express / Figma:
/// - Controles superiores de Modo de Mesclagem (Blend Mode) e Opacidade (0-100%)
/// - Lista de camadas com ordenação por arrastar e soltar (drag & drop)
/// - Ícone de olho para visibilidade e cadeado para bloqueio
/// - Menu de contexto com ações rápidas da camada
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

signals:
    void selectionRequested(const cc::LayerId& id);
    void duplicateRequested(const cc::LayerId& id);
    void deleteRequested(const cc::LayerId& id);
    void focusRequested(const QPointF& documentPos);

private:
    void buildUi();
    void retranslateUi();
    void rebuild();
    void updateControlsForSelection();
    void onCurrentRowChanged(int row);
    void onItemChanged(QListWidgetItem* item);
    void onMoved();
    void showContextMenu(const QPoint& pos);
    void onItemDoubleClicked(QListWidgetItem* item);
    int docIndexFromRow(int row) const;
    LayerId layerIdFromRow(int row) const;

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
};

} // namespace cc
