#include "layerspanel.h"
#include "themeicons.h"
#include "localization/i18nservice.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "core/layers/Layer.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace cc {

namespace {

QString fallbackName(const Layer& layer, I18nService* i18n)
{
    if (!layer.name.isEmpty())
        return layer.name;
    if (!i18n) {
        switch (layer.type()) {
        case LayerType::Group:      return QStringLiteral("Group");
        case LayerType::Image:      return QStringLiteral("Image");
        case LayerType::Text:       return QStringLiteral("Text");
        case LayerType::Shape:      return QStringLiteral("Shape");
        case LayerType::Background: return QStringLiteral("Background");
        }
        return QStringLiteral("Layer");
    }

    switch (layer.type()) {
    case LayerType::Group:      return i18n->t("common", "layer.typeGroup");
    case LayerType::Image:      return i18n->t("common", "layer.typeImage");
    case LayerType::Text:       return i18n->t("common", "layer.typeText");
    case LayerType::Shape:      return i18n->t("common", "layer.typeShape");
    case LayerType::Background: return i18n->t("common", "layer.typeBackground");
    }
    return i18n->t("common", "layer.typeImage");
}

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

} // namespace

LayersPanel::LayersPanel(I18nService* i18n, CommandStack* history, QWidget* parent)
    : QWidget(parent)
    , m_history(history)
    , m_i18n(i18n)
{
    buildUi();

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

    // --- 1. Controles superiores: Modo de Mesclagem e Opacidade ---
    auto* controlsFrame = new QFrame(this);
    controlsFrame->setFrameShape(QFrame::NoFrame);
    auto* controlsLayout = new QVBoxLayout(controlsFrame);
    controlsLayout->setContentsMargins(2, 2, 2, 2);
    controlsLayout->setSpacing(4);

    // Linha 1: Modo de Mesclagem
    auto* blendRow = new QHBoxLayout;
    m_blendLabel = new QLabel(controlsFrame);
    m_blendCombo = new QComboBox(controlsFrame);
    m_blendCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    blendRow->addWidget(m_blendLabel);
    blendRow->addWidget(m_blendCombo);
    controlsLayout->addLayout(blendRow);

    // Linha 2: Opacidade
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

    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    // --- 2. Lista interativa de camadas com miniaturas ---
    m_list = new LayerListWidget(this);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  background-color: #1e222a;"
        "  border: 1px solid #2d333f;"
        "  border-radius: 6px;"
        "  padding: 2px;"
        "}"
        "QListWidget::item {"
        "  border-radius: 4px;"
        "  border: 1px solid transparent;"
        "  padding: 2px;"
        "  margin: 1px 0px;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: #282d38;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: #2b3a52;"
        "  border: 1px solid #2b78e4;"
        "}"
    ));

    connect(m_list, &QListWidget::currentRowChanged,
            this, &LayersPanel::onCurrentRowChanged);
    connect(m_list, &QListWidget::itemDoubleClicked,
            this, &LayersPanel::onItemDoubleClicked);
    connect(static_cast<LayerListWidget*>(m_list), &LayerListWidget::moved,
            this, &LayersPanel::onMoved);
    connect(m_list, &QListWidget::customContextMenuRequested,
            this, &LayersPanel::showContextMenu);

    layout->addWidget(m_list);

    // --- 3. Barra de Ações Inferior das Camadas ---
    auto* actionsBar = new QWidget(this);
    auto* actionsLayout = new QHBoxLayout(actionsBar);
    actionsLayout->setContentsMargins(2, 2, 2, 2);
    actionsLayout->setSpacing(4);

    auto makeActionBtn = [this](const QIcon& icon, const QString& tip) {
        auto* btn = new QToolButton(this);
        btn->setIcon(icon);
        btn->setIconSize(QSize(18, 18));
        btn->setToolTip(tip);
        btn->setFixedSize(28, 28);
        btn->setStyleSheet(QStringLiteral(
            "QToolButton {"
            "  border: 1px solid transparent;"
            "  border-radius: 4px;"
            "  background-color: transparent;"
            "}"
            "QToolButton:hover {"
            "  background-color: #353b49;"
            "  border-color: #4a5366;"
            "}"
            "QToolButton:pressed {"
            "  background-color: #22262f;"
            "}"
        ));
        return btn;
    };

    m_addLayerBtn = makeActionBtn(ThemeIcons::actionAdd(), QStringLiteral("Add Layer"));
    m_addLayerBtn->setPopupMode(QToolButton::InstantPopup);
    auto* addMenu = new QMenu(m_addLayerBtn);
    auto* addTextAct = addMenu->addAction(ThemeIcons::layerTypeText(), QStringLiteral("Text Layer"));
    auto* addShapeAct = addMenu->addAction(ThemeIcons::layerTypeShape(), QStringLiteral("Shape Layer"));
    connect(addTextAct, &QAction::triggered, this, &LayersPanel::addTextRequested);
    connect(addShapeAct, &QAction::triggered, this, &LayersPanel::addShapeRequested);
    m_addLayerBtn->setMenu(addMenu);

    m_duplicateBtn = makeActionBtn(ThemeIcons::actionDuplicate(), QStringLiteral("Duplicate Layer"));
    connect(m_duplicateBtn, &QToolButton::clicked, this, [this] {
        if (!m_selectedId.isNull())
            emit duplicateRequested(m_selectedId);
    });

    m_moveUpBtn = makeActionBtn(ThemeIcons::actionMoveUp(), QStringLiteral("Move Up"));
    connect(m_moveUpBtn, &QToolButton::clicked, this, [this] {
        if (!m_document || m_selectedId.isNull()) return;
        int row = m_list->currentRow();
        if (row > 0) {
            int newDocIdx = docIndexFromRow(row - 1);
            m_document->reorderLayer(m_selectedId, m_document->rootGroup(), newDocIdx);
            rebuild();
        }
    });

    m_moveDownBtn = makeActionBtn(ThemeIcons::actionMoveDown(), QStringLiteral("Move Down"));
    connect(m_moveDownBtn, &QToolButton::clicked, this, [this] {
        if (!m_document || m_selectedId.isNull()) return;
        int row = m_list->currentRow();
        if (row >= 0 && row < m_list->count() - 1) {
            int newDocIdx = docIndexFromRow(row + 1);
            m_document->reorderLayer(m_selectedId, m_document->rootGroup(), newDocIdx);
            rebuild();
        }
    });

    m_deleteBtn = makeActionBtn(ThemeIcons::actionDelete(), QStringLiteral("Delete Layer"));
    connect(m_deleteBtn, &QToolButton::clicked, this, [this] {
        if (!m_selectedId.isNull())
            emit deleteRequested(m_selectedId);
    });

    actionsLayout->addWidget(m_addLayerBtn);
    actionsLayout->addWidget(m_duplicateBtn);
    actionsLayout->addWidget(m_moveUpBtn);
    actionsLayout->addWidget(m_moveDownBtn);
    actionsLayout->addStretch();
    actionsLayout->addWidget(m_deleteBtn);

    layout->addWidget(actionsBar);

    // Eventos do Modo de Mesclagem
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

    connect(m_opacitySlider, &QSlider::sliderPressed, this, [this] {
        if (!m_document || m_selectedId.isNull())
            return;
        if (const Layer* l = m_document->findLayer(m_selectedId))
            m_opacityBeforeSlide = l->opacity();
    });

    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_updating || !m_document || m_selectedId.isNull())
            return;
        const bool prev = m_opacitySpin->blockSignals(true);
        m_opacitySpin->setValue(value);
        m_opacitySpin->blockSignals(prev);

        m_document->setLayerOpacity(m_selectedId, static_cast<float>(value) / 100.0f);
    });

    connect(m_opacitySlider, &QSlider::sliderReleased, this, [this] {
        if (!m_document || m_selectedId.isNull() || !m_history)
            return;
        Layer* l = m_document->findLayer(m_selectedId);
        if (!l)
            return;
        const float newOp = l->opacity();
        if (!qFuzzyCompare(m_opacityBeforeSlide, newOp)) {
            l->setOpacity(m_opacityBeforeSlide);
            m_history->execute(std::make_unique<LayerOpacityCommand>(
                *m_document, m_selectedId, QStringLiteral("layer.opacity"),
                &Document::setLayerOpacity, m_opacityBeforeSlide, newOp));
        }
    });

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

QPixmap LayersPanel::renderLayerThumbnail(const Layer& layer, int size) const
{
    QPixmap pixmap(size, size);
    pixmap.fill(QColor(32, 36, 44));

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Fundo quadriculado sutil de transparência
    const int checkSize = 4;
    for (int y = 0; y < size; y += checkSize) {
        for (int x = 0; x < size; x += checkSize) {
            if (((x / checkSize) + (y / checkSize)) % 2 == 0) {
                p.fillRect(x, y, checkSize, checkSize, QColor(42, 46, 56));
            }
        }
    }

    if (layer.type() == LayerType::Image) {
        const auto* imgLayer = static_cast<const ImageLayer*>(&layer);
        if (m_document) {
            const QImage img = m_document->assets().decodedImage(imgLayer->assetId);
            if (!img.isNull()) {
                const QImage thumb = img.scaled(size - 4, size - 4, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                const int ox = (size - thumb.width()) / 2;
                const int oy = (size - thumb.height()) / 2;
                p.drawImage(ox, oy, thumb);
            }
        }
    } else if (layer.type() == LayerType::Text) {
        const auto* txtLayer = static_cast<const TextLayer*>(&layer);
        p.setPen(txtLayer->color.isValid() ? txtLayer->color : QColor(240, 240, 240));
        QFont f = p.font();
        f.setPixelSize(size * 0.65);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, QStringLiteral("T"));
    } else if (layer.type() == LayerType::Shape) {
        const auto* shapeLayer = static_cast<const ShapeLayer*>(&layer);
        p.setPen(QPen(shapeLayer->stroke.isValid() ? shapeLayer->stroke : QColor(100, 100, 100), 1.5));
        p.setBrush(shapeLayer->fill.isValid() ? shapeLayer->fill : QColor(200, 200, 200));
        p.drawRoundedRect(QRectF(4, 4, size - 8, size - 8), 2.5, 2.5);
    } else if (layer.type() == LayerType::Background) {
        const auto* bgLayer = static_cast<const BackgroundLayer*>(&layer);
        p.fillRect(QRect(0, 0, size, size), bgLayer->fill);
    }

    p.end();
    return pixmap;
}

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

    if (m_duplicateBtn) m_duplicateBtn->setEnabled(hasSelection);
    if (m_deleteBtn) m_deleteBtn->setEnabled(hasSelection);
    if (m_moveUpBtn) m_moveUpBtn->setEnabled(hasSelection);
    if (m_moveDownBtn) m_moveDownBtn->setEnabled(hasSelection);

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
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            Layer* layer = it->get();
            const LayerId layerId = layer->id();

            auto* item = new QListWidgetItem(m_list);
            item->setData(Qt::UserRole, QVariant::fromValue(layerId));
            item->setSizeHint(QSize(200, 42));

            // Custom Widget for each layer row
            auto* rowWidget = new QWidget(m_list);
            auto* rowLayout = new QHBoxLayout(rowWidget);
            rowLayout->setContentsMargins(4, 2, 6, 2);
            rowLayout->setSpacing(6);

            // Visibility Toggle Button
            auto* visBtn = new QToolButton(rowWidget);
            visBtn->setIcon(ThemeIcons::layerVisibility(layer->visible));
            visBtn->setIconSize(QSize(18, 18));
            visBtn->setFixedSize(22, 22);
            visBtn->setStyleSheet(QStringLiteral("border: none; background: transparent;"));
            visBtn->setToolTip(m_i18n ? m_i18n->t("common", "panel.visibility") : QStringLiteral("Toggle Visibility"));
            connect(visBtn, &QToolButton::clicked, this, [this, layerId, layer] {
                if (m_document) {
                    m_document->setLayerVisible(layerId, !layer->visible);
                    rebuild();
                }
            });

            // Thumbnail Preview
            auto* thumbLabel = new QLabel(rowWidget);
            thumbLabel->setPixmap(renderLayerThumbnail(*layer, 32));
            thumbLabel->setFixedSize(32, 32);
            thumbLabel->setStyleSheet(QStringLiteral("border: 1px solid #333a46; border-radius: 4px;"));

            // Type Badge Icon
            auto* typeIcon = new QLabel(rowWidget);
            QIcon badgeIcon;
            switch (layer->type()) {
            case LayerType::Text:       badgeIcon = ThemeIcons::layerTypeText(); break;
            case LayerType::Shape:      badgeIcon = ThemeIcons::layerTypeShape(); break;
            case LayerType::Image:      badgeIcon = ThemeIcons::layerTypeImage(); break;
            case LayerType::Group:      badgeIcon = ThemeIcons::layerTypeGroup(); break;
            case LayerType::Background: badgeIcon = ThemeIcons::layerTypeShape(); break;
            }
            typeIcon->setPixmap(badgeIcon.pixmap(14, 14));
            typeIcon->setFixedSize(14, 14);

            // Layer Name Label
            auto* nameLabel = new QLabel(fallbackName(*layer, m_i18n), rowWidget);
            nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            QFont nf = nameLabel->font();
            if (layer->locked) {
                nf.setItalic(true);
                nameLabel->setStyleSheet(QStringLiteral("color: #7b889b;"));
            } else {
                nameLabel->setStyleSheet(QStringLiteral("color: #e5e9f0;"));
            }
            nameLabel->setFont(nf);

            // Lock Toggle Button
            auto* lockBtn = new QToolButton(rowWidget);
            lockBtn->setIcon(ThemeIcons::layerLock(layer->locked));
            lockBtn->setIconSize(QSize(16, 16));
            lockBtn->setFixedSize(20, 20);
            lockBtn->setStyleSheet(QStringLiteral("border: none; background: transparent;"));
            lockBtn->setToolTip(m_i18n ? m_i18n->t("common", "panel.lockToggle") : QStringLiteral("Toggle Lock"));
            connect(lockBtn, &QToolButton::clicked, this, [this, layerId, layer] {
                if (m_document) {
                    m_document->setLayerLocked(layerId, !layer->locked);
                    rebuild();
                }
            });

            rowLayout->addWidget(visBtn);
            rowLayout->addWidget(thumbLabel);
            rowLayout->addWidget(typeIcon);
            rowLayout->addWidget(nameLabel);
            rowLayout->addWidget(lockBtn);

            m_list->setItemWidget(item, rowWidget);

            if (layerId == m_selectedId)
                m_list->setCurrentRow(m_list->count() - 1);
        }
    }
    m_updating = false;

    updateControlsForSelection();
}

int LayersPanel::docIndexFromRow(int row) const
{
    if (!m_document) return 0;
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

void LayersPanel::setSelectedLayers(const QList<LayerId>& ids)
{
    if (ids.isEmpty()) {
        setSelectedLayer(LayerId());
        return;
    }
    m_selectedId = ids.last();
    updateControlsForSelection();

    if (m_updating || !m_document)
        return;

    m_updating = true;
    m_list->clearSelection();
    for (int row = 0; row < m_list->count(); ++row) {
        const LayerId lid = m_list->item(row)->data(Qt::UserRole).value<LayerId>();
        if (ids.contains(lid)) {
            m_list->item(row)->setSelected(true);
        }
    }
    m_updating = false;
}

void LayersPanel::onCurrentRowChanged(int row)
{
    if (m_updating)
        return;

    const auto selectedItems = m_list->selectedItems();
    if (selectedItems.size() > 1) {
        QList<LayerId> ids;
        for (auto* item : selectedItems) {
            ids.append(item->data(Qt::UserRole).value<LayerId>());
        }
        m_selectedId = layerIdFromRow(row);
        updateControlsForSelection();
        emit multiSelectionRequested(ids);
        return;
    }

    const LayerId id = layerIdFromRow(row);
    m_selectedId = id;
    updateControlsForSelection();
    if (!id.isNull())
        emit selectionRequested(id);
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

    if (m_addLayerBtn) m_addLayerBtn->setToolTip(m_i18n->t("common", "panel.addLayer"));
    if (m_duplicateBtn) m_duplicateBtn->setToolTip(m_i18n->t("common", "panel.duplicate"));
    if (m_moveUpBtn) m_moveUpBtn->setToolTip(m_i18n->t("common", "panel.moveUp"));
    if (m_moveDownBtn) m_moveDownBtn->setToolTip(m_i18n->t("common", "panel.moveDown"));
    if (m_deleteBtn) m_deleteBtn->setToolTip(m_i18n->t("common", "panel.delete"));

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

    rebuild();
}

} // namespace cc

#include "layerspanel.moc"
