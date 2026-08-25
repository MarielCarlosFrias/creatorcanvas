#pragma once

#include <QWidget>

#include "core/Document.h"

class QListWidget;
class QListWidgetItem;

namespace cc {

class I18nService;

/// Adobe-Express-style layer list: top layer first, drag to reorder,
/// eye toggles visibility, right-click for layer actions.
class LayersPanel final : public QWidget
{
    Q_OBJECT
public:
    explicit LayersPanel(I18nService* i18n, QWidget* parent = nullptr);

    void setDocument(Document* document);
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
    void onCurrentRowChanged(int row);
    void onItemChanged(QListWidgetItem* item);
    void onMoved();
    void showContextMenu(const QPoint& pos);
    void onItemDoubleClicked(QListWidgetItem* item);
    int docIndexFromRow(int row) const;
    LayerId layerIdFromRow(int row) const;

    Document* m_document = nullptr;
    I18nService* m_i18n = nullptr;
    QListWidget* m_list = nullptr;
    bool m_updating = false;
    LayerId m_selectedId;
};

} // namespace cc
