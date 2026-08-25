#include "layerspanel.h"

#include "localization/i18nservice.h"

#include <QListWidget>
#include <QMenu>
#include <QVBoxLayout>

namespace cc {

namespace {
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

// QListWidget with a signal after an internal-move drop finishes.
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

LayersPanel::LayersPanel(I18nService* i18n, QWidget* parent)
    : QWidget(parent)
    , m_i18n(i18n)
{
    buildUi();

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &LayersPanel::retranslateUi);
    retranslateUi();
}

void LayersPanel::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_list = new LayerListWidget(this);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_list, &QListWidget::currentRowChanged,
            this, &LayersPanel::onCurrentRowChanged);
    connect(m_list, &QListWidget::itemChanged,
            this, &LayersPanel::onItemChanged);
    connect(static_cast<LayerListWidget*>(m_list), &LayerListWidget::moved,
            this, &LayersPanel::onMoved);
    connect(m_list, &QListWidget::customContextMenuRequested,
            this, &LayersPanel::showContextMenu);

    layout->addWidget(m_list);
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
    // Strings are resolved on demand by showContextMenu; nothing cached here.
}

} // namespace cc

#include "layerspanel.moc"
