#include "layerspanel.h"

// Inclusão dos serviços de internacionalização e pilha de comandos de histórico
#include "localization/i18nservice.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"

// Componentes gráficos Qt utilizados para a interface do painel de camadas
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace cc {

namespace {

// Retorna um nome descritivo padrão caso a camada não possua nome explícito
QString fallbackName(const Layer& layer)
{
    if (!layer.name.isEmpty())
        return layer.name;
    switch (layer.type()) {
    case LayerType::Group:      return QStringLiteral("Group");
    case LayerType::Image:      return QStringLiteral("Image");
    case LayerType::Text:       return QStringLiteral("Text");
    case LayerType::Shape:      return QStringLiteral("Shape");
    case LayerType::Background: return QStringLiteral("Background");
    }
    return QStringLiteral("Layer");
}

} // namespace

// QListWidget customizado que emite o sinal moved() após o usuário soltar a camada reordenada
class LayerListWidget final : public QListWidget
{
    Q_OBJECT
public:
    using QListWidget::QListWidget;

signals:
    void moved();

protected:
    void dropEvent(QDropEvent* event) override
    {
        QListWidget::dropEvent(event);
        emit moved();
    }
};

LayersPanel::LayersPanel(I18nService* i18n, CommandStack* history, QWidget* parent)
    : QWidget(parent)
    , m_history(history)
    , m_i18n(i18n)
{
    buildUi();

    // Conecta atualização de idioma para traduzir labels e itens do combobox de mesclagem
    if (m_i18n) {
        connect(m_i18n, &I18nService::languageChanged,
                this, &LayersPanel::retranslateUi);
    }
    retranslateUi();
}

void LayersPanel::setHistory(CommandStack* history)
{
    m_history = history;
}

void LayersPanel::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    // Frame superior com controles de Modo de Mesclagem e Opacidade
    auto* controlsFrame = new QFrame(this);
    controlsFrame->setFrameShape(QFrame::NoFrame);
    auto* controlsLayout = new QVBoxLayout(controlsFrame);
    controlsLayout->setContentsMargins(2, 2, 2, 2);
    controlsLayout->setSpacing(4);

    // Linha 1: Modo de Mesclagem (Normal, Multiplicação, Sobreposição, etc.)
    auto* blendRow = new QHBoxLayout;
    m_blendLabel = new QLabel(controlsFrame);
    m_blendCombo = new QComboBox(controlsFrame);
    m_blendCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    blendRow->addWidget(m_blendLabel);
    blendRow->addWidget(m_blendCombo);
    controlsLayout->addLayout(blendRow);

    // Linha 2: Opacidade (Slider de 0% a 100% acompanhado de SpinBox numérico)
    auto* opacityRow = new QHBoxLayout;
    m_opacityLabel = new QLabel(controlsFrame);
    m_opacitySlider = new QSlider(Qt::Horizontal, controlsFrame);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(100);

    m_opacitySpin = new QSpinBox(controlsFrame);
    m_opacitySpin->setRange(0, 100);
    m_opacitySpin->setValue(100);
    m_opacitySpin->setSuffix(QStringLiteral("%"));
    m_opacitySpin->setFixedWidth(64);

    opacityRow->addWidget(m_opacityLabel);
    opacityRow->addWidget(m_opacitySlider);
    opacityRow->addWidget(m_opacitySpin);
    controlsLayout->addLayout(opacityRow);

    layout->addWidget(controlsFrame);

    // Divisor horizontal sutil
    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    // Lista interativa de camadas
    m_list = new LayerListWidget(this);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_list, &QListWidget::currentRowChanged,
            this, &LayersPanel::onCurrentRowChanged);
    connect(m_list, &QListWidget::itemChanged,
            this, &LayersPanel::onItemChanged);
    connect(m_list, &QListWidget::itemDoubleClicked,
            this, &LayersPanel::onItemDoubleClicked);
    connect(static_cast<LayerListWidget*>(m_list), &LayerListWidget::moved,
            this, &LayersPanel::onMoved);
    connect(m_list, &QListWidget::customContextMenuRequested,
            this, &LayersPanel::showContextMenu);

    layout->addWidget(m_list);

    // Eventos do Modo de Mesclagem (executado via CommandStack para Undo/Redo)
    connect(m_blendCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
        if (m_updating || !m_document || m_selectedId.isNull() || index < 0)
            return;
        Layer* layer = m_document->findLayer(m_selectedId);
        if (!layer)
            return;
        const auto newMode = static_cast<BlendMode>(index);
        if (layer->blendMode == newMode)
            return;

        if (m_history) {
            m_history->execute(std::make_unique<LayerBlendModeCommand>(
                *m_document, m_selectedId, QStringLiteral("layer.blendMode"),
                &Document::setLayerBlendMode, layer->blendMode, newMode));
        } else {
            m_document->setLayerBlendMode(m_selectedId, newMode);
        }
    });

    // Salva a opacidade inicial quando o usuário começa a arrastar o slider
    connect(m_opacitySlider, &QSlider::sliderPressed, this, [this] {
        if (!m_document || m_selectedId.isNull())
            return;
        if (const Layer* l = m_document->findLayer(m_selectedId))
            m_opacityBeforeSlide = l->opacity();
    });

    // Atualização fluida em tempo real da opacidade enquanto o slider se move
    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_updating || !m_document || m_selectedId.isNull())
            return;
        const bool prev = m_opacitySpin->blockSignals(true);
        m_opacitySpin->setValue(value);
        m_opacitySpin->blockSignals(prev);

        m_document->setLayerOpacity(m_selectedId, static_cast<float>(value) / 100.0f);
    });

    // Registra comando de histórico com Undo/Redo apenas quando o usuário solta o slider
    connect(m_opacitySlider, &QSlider::sliderReleased, this, [this] {
        if (!m_document || m_selectedId.isNull() || !m_history)
            return;
        Layer* l = m_document->findLayer(m_selectedId);
        if (!l)
            return;
        const float newOp = l->opacity();
        if (!qFuzzyCompare(m_opacityBeforeSlide, newOp)) {
            // Reverte momentaneamente para registrar old -> new no comando
            l->setOpacity(m_opacityBeforeSlide);
            m_history->execute(std::make_unique<LayerOpacityCommand>(
                *m_document, m_selectedId, QStringLiteral("layer.opacity"),
                &Document::setLayerOpacity, m_opacityBeforeSlide, newOp));
        }
    });

    // Mudança numérica direta no SpinBox
    connect(m_opacitySpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        if (m_updating || !m_document || m_selectedId.isNull())
            return;
        const bool prev = m_opacitySlider->blockSignals(true);
        m_opacitySlider->setValue(value);
        m_opacitySlider->blockSignals(prev);

        Layer* l = m_document->findLayer(m_selectedId);
        if (!l)
            return;
        const float oldOp = l->opacity();
        const float newOp = static_cast<float>(value) / 100.0f;
        if (qFuzzyCompare(oldOp, newOp))
            return;

        if (m_history) {
            m_history->execute(std::make_unique<LayerOpacityCommand>(
                *m_document, m_selectedId, QStringLiteral("layer.opacity"),
                &Document::setLayerOpacity, oldOp, newOp));
        } else {
            m_document->setLayerOpacity(m_selectedId, newOp);
        }
    });

    updateControlsForSelection();
}

// Atualiza o estado habilitado/desabilitado e valores dos controles de acordo com a seleção
void LayersPanel::updateControlsForSelection()
{
    Layer* layer = (m_document && !m_selectedId.isNull())
                       ? m_document->findLayer(m_selectedId)
                       : nullptr;

    const bool hasSelection = (layer != nullptr && layer->type() != LayerType::Background);
    m_blendLabel->setEnabled(hasSelection);
    m_blendCombo->setEnabled(hasSelection);
    m_opacityLabel->setEnabled(hasSelection);
    m_opacitySlider->setEnabled(hasSelection);
    m_opacitySpin->setEnabled(hasSelection);

    if (hasSelection && layer) {
        const bool prevUpdating = m_updating;
        m_updating = true;
        m_blendCombo->setCurrentIndex(static_cast<int>(layer->blendMode));
        const int opInt = qBound(0, qRound(layer->opacity() * 100.0f), 100);
        m_opacitySlider->setValue(opInt);
        m_opacitySpin->setValue(opInt);
        m_updating = prevUpdating;
    }
}

void LayersPanel::setDocument(Document* document)
{
    if (m_document == document)
        return;
    m_document = document;
    m_selectedId = LayerId();
    refresh();
}

void LayersPanel::refresh()
{
    rebuild();
}

void LayersPanel::rebuild()
{
    m_updating = true;
    m_list->clear();

    if (m_document) {
        const auto& children = m_document->rootGroup()->children;
        // As camadas mais altas são exibidas no topo da lista (ordem inversa da renderização)
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            Layer* layer = it->get();
            auto* item = new QListWidgetItem(fallbackName(*layer), m_list);
            item->setData(Qt::UserRole, QVariant::fromValue(layer->id()));
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled
                           | Qt::ItemIsDragEnabled | Qt::ItemIsUserCheckable);
            item->setCheckState(layer->visible ? Qt::Checked : Qt::Unchecked);
            if (layer->locked) {
                item->setText(item->text() + QStringLiteral("  🔒"));
                QFont font = item->font();
                font.setItalic(true);
                item->setFont(font);
            }
            if (layer->id() == m_selectedId)
                m_list->setCurrentRow(m_list->count() - 1);
        }
    }
    m_updating = false;

    updateControlsForSelection();
}

int LayersPanel::docIndexFromRow(int row) const
{
    const int count = static_cast<int>(m_document->rootGroup()->children.size());
    return count - 1 - row;
}

LayerId LayersPanel::layerIdFromRow(int row) const
{
    if (row < 0 || row >= m_list->count() || !m_document)
        return {};
    const int index = docIndexFromRow(row);
    const auto& children = m_document->rootGroup()->children;
    if (index < 0 || index >= static_cast<int>(children.size()))
        return {};
    return children[static_cast<std::size_t>(index)]->id();
}

void LayersPanel::setSelectedLayer(const LayerId& id)
{
    m_selectedId = id;
    updateControlsForSelection();

    if (m_updating || !m_document)
        return;
    for (int row = 0; row < m_list->count(); ++row) {
        const QVariant data = m_list->item(row)->data(Qt::UserRole);
        if (data.value<LayerId>() == id) {
            m_list->setCurrentRow(row);
            return;
        }
    }
}

void LayersPanel::onCurrentRowChanged(int row)
{
    if (m_updating)
        return;
    const LayerId id = layerIdFromRow(row);
    m_selectedId = id;
    updateControlsForSelection();
    if (!id.isNull())
        emit selectionRequested(id);
}

void LayersPanel::onItemChanged(QListWidgetItem* item)
{
    if (m_updating || !m_document || !item)
        return;
    const LayerId id = item->data(Qt::UserRole).value<LayerId>();
    if (id.isNull())
        return;
    m_document->setLayerVisible(id, item->checkState() == Qt::Checked);
}

void LayersPanel::onItemDoubleClicked(QListWidgetItem* item)
{
    if (!m_document || !item)
        return;
    const LayerId id = item->data(Qt::UserRole).value<LayerId>();
    Layer* layer = m_document->findLayer(id);
    if (!layer)
        return;
    emit focusRequested(layer->transform.position);
}

void LayersPanel::onMoved()
{
    if (m_updating || !m_document)
        return;
    const int row = m_list->currentRow();
    const LayerId id = layerIdFromRow(row);
    if (id.isNull())
        return;
    m_document->reorderLayer(id, m_document->rootGroup(), docIndexFromRow(row));
    m_selectedId = id;
    rebuild();
}

void LayersPanel::showContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_list->itemAt(pos);
    if (!item || !m_document || !m_i18n)
        return;
    const LayerId id = item->data(Qt::UserRole).value<LayerId>();
    Layer* layer = m_document->findLayer(id);
    if (!layer)
        return;

    QMenu menu(this);
    QAction* lockAction = menu.addAction(layer->locked
        ? m_i18n->t("common", "panel.unlock")
        : m_i18n->t("common", "panel.lock"));
    menu.addSeparator();
    QAction* frontAction = menu.addAction(m_i18n->t("common", "panel.front"));
    QAction* backAction = menu.addAction(m_i18n->t("common", "panel.back"));
    menu.addSeparator();
    QAction* duplicateAction = menu.addAction(m_i18n->t("common", "panel.duplicate"));
    QAction* deleteAction = menu.addAction(m_i18n->t("common", "panel.delete"));

    QAction* chosen = menu.exec(m_list->mapToGlobal(pos));
    if (chosen == lockAction) {
        m_document->setLayerLocked(id, !layer->locked);
        rebuild();
    } else if (chosen == frontAction) {
        m_document->reorderLayer(id, m_document->rootGroup(), -1);
        rebuild();
    } else if (chosen == backAction) {
        m_document->reorderLayer(id, m_document->rootGroup(), 0);
        rebuild();
    } else if (chosen == duplicateAction) {
        emit duplicateRequested(id);
    } else if (chosen == deleteAction) {
        emit deleteRequested(id);
    }
}

void LayersPanel::retranslateUi()
{
    if (!m_i18n)
        return;

    m_blendLabel->setText(m_i18n->t("common", "panel.blendMode"));
    m_opacityLabel->setText(m_i18n->t("common", "panel.opacity"));

    // Atualiza os nomes traduzidos dos Modos de Mesclagem no ComboBox
    const bool prevUpdating = m_updating;
    m_updating = true;
    const int currentIdx = m_blendCombo ? m_blendCombo->currentIndex() : 0;
    m_blendCombo->clear();
    m_blendCombo->addItem(m_i18n->t("common", "blend.normal"));
    m_blendCombo->addItem(m_i18n->t("common", "blend.multiply"));
    m_blendCombo->addItem(m_i18n->t("common", "blend.screen"));
    m_blendCombo->addItem(m_i18n->t("common", "blend.overlay"));
    m_blendCombo->addItem(m_i18n->t("common", "blend.darken"));
    m_blendCombo->addItem(m_i18n->t("common", "blend.lighten"));
    m_blendCombo->addItem(m_i18n->t("common", "blend.add"));
    if (currentIdx >= 0 && currentIdx < m_blendCombo->count())
        m_blendCombo->setCurrentIndex(currentIdx);
    m_updating = prevUpdating;
}

} // namespace cc

#include "layerspanel.moc"
