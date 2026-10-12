#include "layerspanel.h"
#include "themeicons.h"
#include "localization/i18nservice.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "core/layers/Layer.h"
#include "core/selection/SelectionOperations.h"

#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QInputDialog>
#include <functional>

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

QColor colorTagColor(LayerColorTag tag)
{
    switch (tag) {
    case LayerColorTag::Red: return QColor("#ef4444");
    case LayerColorTag::Orange: return QColor("#f97316");
    case LayerColorTag::Yellow: return QColor("#eab308");
    case LayerColorTag::Green: return QColor("#22c55e");
    case LayerColorTag::Blue: return QColor("#3b82f6");
    case LayerColorTag::Purple: return QColor("#a855f7");
    case LayerColorTag::Gray: return QColor("#94a3b8");
    case LayerColorTag::None: break;
    }
    return {};
}

class LayerRowWidget final : public QWidget
{
    Q_OBJECT
public:
    LayerRowWidget(const LayerId& id, QListWidget* listWidget, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_id(id)
        , m_list(listWidget)
    {
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragStartPos = event->pos();
            for (int i = 0; i < m_list->count(); ++i) {
                if (m_list->itemWidget(m_list->item(i)) == this) {
                    m_list->setCurrentRow(i);
                    break;
                }
            }
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if ((event->buttons() & Qt::LeftButton) &&
            (event->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
            startDrag();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            for (int i = 0; i < m_list->count(); ++i) {
                if (m_list->itemWidget(m_list->item(i)) == this) {
                    emit m_list->itemDoubleClicked(m_list->item(i));
                    break;
                }
            }
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent* event) override
    {
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->itemWidget(m_list->item(i)) == this) {
                m_list->setCurrentRow(i);
                break;
            }
        }
        const QPoint listPos = m_list->mapFromGlobal(event->globalPos());
        emit m_list->customContextMenuRequested(listPos);
    }

private:
    void startDrag()
    {
        auto* drag = new QDrag(this);
        auto* mimeData = new QMimeData;
        mimeData->setData(QStringLiteral("application/x-creatorcanvas-layer-id"),
                          m_id.toString(QUuid::WithoutBraces).toUtf8());
        drag->setMimeData(mimeData);

        QPixmap pixmap(size());
        pixmap.fill(Qt::transparent);
        render(&pixmap);
        drag->setPixmap(pixmap);
        drag->setHotSpot(m_dragStartPos);

        drag->exec(Qt::MoveAction);
    }

    LayerId m_id;
    QListWidget* m_list = nullptr;
    QPoint m_dragStartPos;
};

class LayerListWidget final : public QListWidget
{
    Q_OBJECT
public:
    using QListWidget::QListWidget;

signals:
    void moved();
    void layerReordered(const LayerId& id, int targetDocIndex);
    void layerDroppedOn(const LayerId& id, const LayerId& targetId, int targetRow);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (event->mimeData()->hasFormat(QStringLiteral("application/x-creatorcanvas-layer-id"))) {
            event->acceptProposedAction();
        } else {
            QListWidget::dragEnterEvent(event);
        }
    }

    void dragMoveEvent(QDragMoveEvent* event) override
    {
        if (event->mimeData()->hasFormat(QStringLiteral("application/x-creatorcanvas-layer-id"))) {
            event->acceptProposedAction();
        } else {
            QListWidget::dragMoveEvent(event);
        }
    }

    void dropEvent(QDropEvent* event) override
    {
        if (event->mimeData()->hasFormat(QStringLiteral("application/x-creatorcanvas-layer-id"))) {
            const QByteArray data = event->mimeData()->data(QStringLiteral("application/x-creatorcanvas-layer-id"));
            const LayerId draggedId = QUuid::fromString(QString::fromUtf8(data));

            const QPoint pos = event->position().toPoint();
            QListWidgetItem* targetItem = itemAt(pos);
            const LayerId targetId = targetItem
                ? targetItem->data(Qt::UserRole).value<LayerId>() : LayerId();
            int targetRow = count() - 1;
            if (targetItem) {
                targetRow = row(targetItem);
                const QRect itemR = visualItemRect(targetItem);
                if (pos.y() > itemR.center().y()) {
                    targetRow += 1;
                }
            }
            targetRow = qBound(0, targetRow, count() - 1);
            if (targetItem && !targetId.isNull()) {
                emit layerDroppedOn(draggedId, targetId, targetRow);
                event->acceptProposedAction();
                return;
            }

            const int total = count();
            if (total > 0 && !draggedId.isNull()) {
                const int targetDocIndex = qBound(0, total - 1 - targetRow, total - 1);
                emit layerReordered(draggedId, targetDocIndex);
            }
            event->acceptProposedAction();
            return;
        }
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

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(QStringLiteral("Search layers..."));
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this] { rebuild(); });
    layout->addWidget(m_searchEdit);

    m_typeFilter = new QComboBox(this);
    m_typeFilter->addItem(QStringLiteral("All types"), -1);
    m_typeFilter->addItem(QStringLiteral("Text"), static_cast<int>(LayerType::Text));
    m_typeFilter->addItem(QStringLiteral("Image"), static_cast<int>(LayerType::Image));
    m_typeFilter->addItem(QStringLiteral("Shape"), static_cast<int>(LayerType::Shape));
    m_typeFilter->addItem(QStringLiteral("Group"), static_cast<int>(LayerType::Group));
    m_typeFilter->addItem(QStringLiteral("Background"), static_cast<int>(LayerType::Background));
    connect(m_typeFilter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this] { rebuild(); });
    layout->addWidget(m_typeFilter);

    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    // --- 2. Lista interativa de camadas com miniaturas ---
    m_list = new LayerListWidget(this);
    m_list->setObjectName(QStringLiteral("LayerListWidget"));
    m_list->setDragDropMode(QAbstractItemView::DropOnly);
    m_list->setAcceptDrops(true);
    m_list->setDropIndicatorShown(true);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_list, &QListWidget::currentRowChanged,
            this, &LayersPanel::onCurrentRowChanged);
    connect(m_list, &QListWidget::itemDoubleClicked,
            this, &LayersPanel::onItemDoubleClicked);
    connect(static_cast<LayerListWidget*>(m_list), &LayerListWidget::moved,
            this, &LayersPanel::onMoved);
    connect(static_cast<LayerListWidget*>(m_list), &LayerListWidget::layerReordered,
            this, &LayersPanel::onLayerReordered);
    connect(static_cast<LayerListWidget*>(m_list), &LayerListWidget::layerDroppedOn,
            this, [this](const LayerId& id, const LayerId& targetId, int targetRow) {
                if (!m_document || id == targetId) return;
                Layer* target = m_document->findLayer(targetId);
                if (!target || !m_history) return;
                if (target->type() == LayerType::Group) {
                    m_history->execute(std::make_unique<ReorderLayerCommand>(
                        *m_document, id, static_cast<GroupLayer*>(target), -1));
                } else {
                    onLayerReordered(id, docIndexFromRow(targetRow));
                    return;
                }
                m_selectedId = id;
                rebuild();
                emit selectionRequested(id);
            });
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
        if (m_document->parentOf(m_selectedId) != m_document->rootGroup()) return;
        int row = m_list->currentRow();
        if (row > 0) {
            int newDocIdx = docIndexFromRow(row - 1);
            onLayerReordered(m_selectedId, newDocIdx);
        }
    });

    m_moveDownBtn = makeActionBtn(ThemeIcons::actionMoveDown(), QStringLiteral("Move Down"));
    connect(m_moveDownBtn, &QToolButton::clicked, this, [this] {
        if (!m_document || m_selectedId.isNull()) return;
        if (m_document->parentOf(m_selectedId) != m_document->rootGroup()) return;
        int row = m_list->currentRow();
        if (row >= 0 && row < m_list->count() - 1) {
            int newDocIdx = docIndexFromRow(row + 1);
            onLayerReordered(m_selectedId, newDocIdx);
        }
    });

    m_deleteBtn = makeActionBtn(ThemeIcons::actionDelete(), QStringLiteral("Delete Layer"));
    connect(m_deleteBtn, &QToolButton::clicked, this, [this] {
        if (!m_selectedId.isNull())
            emit deleteRequested(m_selectedId);
    });

    m_applyMaskBtn = new QToolButton(this);
    m_applyMaskBtn->setText(QStringLiteral("Mask ▾"));
    m_applyMaskBtn->setMinimumWidth(86);
    m_applyMaskBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_applyMaskBtn->setToolTip(QStringLiteral("Apply the current selection as a layer mask"));
    m_applyMaskBtn->setPopupMode(QToolButton::MenuButtonPopup);
    auto* maskMenu = new QMenu(m_applyMaskBtn);
    auto* keepInside = maskMenu->addAction(QStringLiteral("Keep inside selection"));
    auto* keepOutside = maskMenu->addAction(QStringLiteral("Keep outside selection"));
    maskMenu->addSeparator();
    auto* removeInside = maskMenu->addAction(QStringLiteral("Remove inside selection"));
    auto* removeOutside = maskMenu->addAction(QStringLiteral("Remove outside selection"));
    m_applyMaskBtn->setMenu(maskMenu);
    // Some styles only open MenuButtonPopup from the small arrow area. Make
    // the main button area open the same menu as well.
    connect(m_applyMaskBtn, &QToolButton::clicked, this, [this] {
        if (m_applyMaskBtn && m_applyMaskBtn->menu())
            m_applyMaskBtn->showMenu();
    });
    auto applySelectionMask = [this](bool invert) {
        if (m_document && SelectionOperations::applyMask(
                *m_document, m_history, m_selectedId, !invert))
            rebuild();
    };
    connect(keepInside, &QAction::triggered, this, [applySelectionMask] { applySelectionMask(false); });
    connect(keepOutside, &QAction::triggered, this, [applySelectionMask] { applySelectionMask(true); });
    connect(removeInside, &QAction::triggered, this, [applySelectionMask] { applySelectionMask(true); });
    connect(removeOutside, &QAction::triggered, this, [applySelectionMask] { applySelectionMask(false); });
    // Primary selection/mask action stays beside the layer controls so it is
    // easy to discover; it is no longer hidden in the tiny footer.
    controlsLayout->addWidget(m_applyMaskBtn);

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
        f.setPixelSize(qMax(8, size / 4));
        f.setBold(txtLayer->bold);
        f.setItalic(txtLayer->italic);
        p.setFont(f);
        const QString previewText = txtLayer->content.simplified();
        p.drawText(QRect(2, 2, size - 4, size - 4),
                   Qt::AlignCenter | Qt::TextWordWrap,
                   previewText.isEmpty() ? QStringLiteral("T") : previewText);
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
    m_blendCombo->setToolTip(hasSelection
        ? QString()
        : (m_i18n ? m_i18n->t("common", "panel.selectLayerToEdit")
                  : QStringLiteral("Select a layer to edit blending")));
    m_opacityLabel->setEnabled(hasSelection);
    m_opacitySlider->setEnabled(hasSelection);
    m_opacitySpin->setEnabled(hasSelection);

    if (m_duplicateBtn) m_duplicateBtn->setEnabled(hasSelection);
    if (m_deleteBtn) m_deleteBtn->setEnabled(hasSelection);
    if (m_moveUpBtn) m_moveUpBtn->setEnabled(hasSelection);
    if (m_moveDownBtn) m_moveDownBtn->setEnabled(hasSelection);
    if (m_applyMaskBtn) {
        const bool canMask = hasSelection && layer->type() == LayerType::Image
            && m_document && !m_document->pixelSelection().isNull();
        m_applyMaskBtn->setEnabled(canMask);
    }

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
    if (m_searchEdit)
        m_searchEdit->clear();
    if (m_typeFilter)
        m_typeFilter->setCurrentIndex(0);
    refresh();
}

void LayersPanel::refresh()
{
    rebuild();
}

void LayersPanel::refreshProperties()
{
    // Atualiza somente os controles de opacidade/blend sem reconstruir a lista.
    // Isso evita reconstrução cara do painel durante arraste de camadas no canvas.
    updateControlsForSelection();
}

void LayersPanel::rebuild()
{
    m_updating = true;
    m_list->clear();

    if (m_document) {
        QVector<QPair<Layer*, int>> visibleLayers;
        std::function<void(const GroupLayer*, int)> collect = [&](const GroupLayer* group, int depth) {
            for (auto it = group->children.rbegin(); it != group->children.rend(); ++it) {
                Layer* child = it->get();
                visibleLayers.append({child, depth});
                if (child->type() == LayerType::Group) {
                    const auto* childGroup = static_cast<const GroupLayer*>(child);
                    if (childGroup->isExpanded) {
                        collect(childGroup, depth + 1);
                    }
                }
            }
        };
        collect(m_document->rootGroup(), 0);

        for (const auto& entry : visibleLayers) {
            Layer* layer = entry.first;
            const int depth = entry.second;
            if (m_searchEdit && !m_searchEdit->text().trimmed().isEmpty()
                && !layer->name.contains(m_searchEdit->text().trimmed(), Qt::CaseInsensitive))
                continue;
            const int typeFilter = m_typeFilter ? m_typeFilter->currentData().toInt() : -1;
            if (typeFilter >= 0 && static_cast<int>(layer->type()) != typeFilter)
                continue;
            const LayerId layerId = layer->id();

            auto* item = new QListWidgetItem(m_list);
            item->setData(Qt::UserRole, QVariant::fromValue(layerId));
            item->setSizeHint(QSize(200, 42));

            // Custom Widget for each layer row
            auto* rowWidget = new LayerRowWidget(layerId, m_list, m_list);
            rowWidget->setAttribute(Qt::WA_StyledBackground, true);
            rowWidget->setStyleSheet(QStringLiteral("background: transparent;"));
            auto* rowLayout = new QHBoxLayout(rowWidget);
            rowLayout->setContentsMargins(4 + depth * 16, 2, 6, 2);
            rowLayout->setSpacing(4);

            // Expand/collapse arrow for Group layers
            if (layer->type() == LayerType::Group) {
                auto* groupLayer = static_cast<GroupLayer*>(layer);
                auto* expandBtn = new QToolButton(rowWidget);
                expandBtn->setText(groupLayer->isExpanded ? QStringLiteral("▼") : QStringLiteral("▶"));
                expandBtn->setFixedSize(16, 24);
                expandBtn->setStyleSheet(QStringLiteral("border: none; background: transparent; color: #94a3b8; font-size: 10px;"));
                expandBtn->setToolTip(groupLayer->isExpanded ? QStringLiteral("Collapse Group") : QStringLiteral("Expand Group"));
                connect(expandBtn, &QToolButton::clicked, this, [this, layerId, groupLayer] {
                    if (!m_document) return;
                    m_document->setGroupExpanded(layerId, !groupLayer->isExpanded);
                    rebuild();
                });
                rowLayout->addWidget(expandBtn);
            } else if (depth > 0) {
                // Spacer matching the expand button width for alignment
                auto* indentSpacer = new QLabel(rowWidget);
                indentSpacer->setFixedWidth(16);
                indentSpacer->setAttribute(Qt::WA_TransparentForMouseEvents);
                rowLayout->addWidget(indentSpacer);
            }

            // Visibility Toggle Button
            auto* visBtn = new QToolButton(rowWidget);
            visBtn->setIcon(ThemeIcons::layerVisibility(layer->visible));
            visBtn->setIconSize(QSize(22, 22));
            visBtn->setFixedSize(30, 30);
            visBtn->setStyleSheet(QStringLiteral("border: none; background: transparent;"));
            visBtn->setToolTip(m_i18n ? m_i18n->t("common", "panel.visibility") : QStringLiteral("Toggle Visibility"));
            connect(visBtn, &QToolButton::clicked, this, [this, layerId, layer] {
                if (!m_document) return;
                const bool newVis = !layer->visible;
                if (m_history) {
                    m_history->execute(std::make_unique<LayerVisibilityCommand>(
                        *m_document, layerId, QStringLiteral("layer.visible"),
                        &Document::setLayerVisible, layer->visible, newVis));
                } else {
                    m_document->setLayerVisible(layerId, newVis);
                }
                rebuild();
            });

            // Thumbnail Preview
            auto* thumbLabel = new QLabel(rowWidget);
            thumbLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
            thumbLabel->setPixmap(renderLayerThumbnail(*layer, 32));
            thumbLabel->setFixedSize(32, 32);
            const bool darkUi = palette().color(QPalette::Window).value() < 128;
            thumbLabel->setStyleSheet(darkUi
                ? QStringLiteral("border: 1px solid #333a46; border-radius: 4px;")
                : QStringLiteral("border: 1px solid #cbd5e1; border-radius: 4px;"));

            // Type Badge Icon
            auto* typeIcon = new QLabel(rowWidget);
            typeIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
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

            // An applied mask is persistent document data, unlike an active
            // selection. Make that distinction visible in the layer list.
            QLabel* maskBadge = nullptr;
            if (layer->type() == LayerType::Image
                && !static_cast<const ImageLayer*>(layer)->alphaMask.isNull()) {
                maskBadge = new QLabel(QStringLiteral("MASK"), rowWidget);
                maskBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
                maskBadge->setToolTip(QStringLiteral("This layer has an applied alpha mask"));
                maskBadge->setStyleSheet(darkUi
                    ? QStringLiteral("color: #7dd3fc; font-size: 9px; font-weight: 700; background: transparent;")
                    : QStringLiteral("color: #0369a1; font-size: 9px; font-weight: 700; background: transparent;"));
            }

            // Layer Name Label
            auto* nameLabel = new QLabel(fallbackName(*layer, m_i18n), rowWidget);
            nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
            nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            QFont nf = nameLabel->font();
            if (layer->locked) {
                nf.setItalic(true);
                nameLabel->setStyleSheet(darkUi
                    ? QStringLiteral("color: #7b889b; background: transparent;")
                    : QStringLiteral("color: #94a3b8; background: transparent;"));
            } else {
                nameLabel->setStyleSheet(darkUi
                    ? QStringLiteral("color: #e5e9f0; background: transparent;")
                    : QStringLiteral("color: #1e293b; background: transparent;"));
            }
            nameLabel->setFont(nf);

            auto* colorTag = new QLabel(rowWidget);
            colorTag->setFixedSize(8, 22);
            const QColor tagColor = colorTagColor(layer->colorTag);
            colorTag->setStyleSheet(tagColor.isValid()
                ? QStringLiteral("background: %1; border-radius: 3px;").arg(tagColor.name())
                : QStringLiteral("background: transparent; border: none;"));
            colorTag->setAttribute(Qt::WA_TransparentForMouseEvents);

            // Lock Toggle Button
            auto* lockBtn = new QToolButton(rowWidget);
            lockBtn->setIcon(ThemeIcons::layerLock(layer->locked));
            lockBtn->setIconSize(QSize(22, 22));
            lockBtn->setFixedSize(30, 30);
            lockBtn->setStyleSheet(QStringLiteral("border: none; background: transparent;"));
            lockBtn->setToolTip(m_i18n ? m_i18n->t("common", "panel.lockToggle") : QStringLiteral("Toggle Lock"));
            connect(lockBtn, &QToolButton::clicked, this, [this, layerId, layer] {
                if (!m_document) return;
                const bool newLock = !layer->locked;
                if (m_history) {
                    m_history->execute(std::make_unique<LayerLockCommand>(
                        *m_document, layerId, QStringLiteral("layer.lock"),
                        &Document::setLayerLocked, layer->locked, newLock));
                } else {
                    m_document->setLayerLocked(layerId, newLock);
                }
                rebuild();
            });

            rowLayout->addWidget(visBtn);
            rowLayout->addWidget(thumbLabel);
            rowLayout->addWidget(typeIcon);
            if (maskBadge)
                rowLayout->addWidget(maskBadge);
            rowLayout->addWidget(colorTag);
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
    // A seleção normal no canvas deve ser exclusiva. Sem limpar os itens
    // anteriores, o QListWidget mantinha várias camadas visualmente ativas e
    // o movimento parecia arrastar objetos que já não deviam estar no grupo.
    m_list->clearSelection();
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

    const auto& children = m_document->rootGroup()->children;
    const int total = static_cast<int>(children.size());
    if (total <= 1 || m_list->count() != total) {
        rebuild();
        return;
    }

    // A lista UI exibe as camadas do topo para a base (inverso da ordem do Document).
    // O item na linha 0 da lista corresponde ao topo da pilha (índice total - 1 no Document).
    // Vamos sincronizar a nova ordem da lista diretamente com o Document.
    // Preserve the order emitted by the widget before applying changes.  The
    // custom drag path emits layerReordered directly; this fallback path is
    // used by native QListWidget moves and must use the same history command.
    QList<QPair<LayerId, int>> desiredOrder;
    desiredOrder.reserve(m_list->count());
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* item = m_list->item(row);
        if (!item)
            continue;
        const LayerId id = item->data(Qt::UserRole).value<LayerId>();
        if (!id.isNull())
            desiredOrder.append({id, total - 1 - row});
    }

    m_updating = true;
    if (m_history) {
        auto compound = std::make_unique<CompoundCommand>(QStringLiteral("layer.reorder"));
        for (const auto& entry : desiredOrder) {
            const LayerId& id = entry.first;
            const int targetDocIndex = entry.second;
            const int currentDocIndex = m_document->indexOf(id);
            if (currentDocIndex == targetDocIndex || currentDocIndex < 0)
                continue;
            compound->addCommand(std::make_unique<ReorderLayerCommand>(
                *m_document, id, m_document->rootGroup(), targetDocIndex));
        }
        if (!compound->isEmpty())
            m_history->execute(std::move(compound));
    } else {
        for (const auto& entry : desiredOrder) {
            const LayerId& id = entry.first;
            const int targetDocIndex = entry.second;
            const int currentDocIndex = m_document->indexOf(id);
            if (currentDocIndex == targetDocIndex || currentDocIndex < 0)
                continue;
            m_document->reorderLayer(id, m_document->rootGroup(), targetDocIndex);
        }
    }
    m_updating = false;

    rebuild();
}

void LayersPanel::onLayerReordered(const LayerId& id, int targetDocIndex)
{
    if (!m_document)
        return;

    const int currentIndex = m_document->indexOf(id);
    if (currentIndex == targetDocIndex || currentIndex < 0)
        return;

    if (m_history) {
        m_history->execute(std::make_unique<ReorderLayerCommand>(
            *m_document, id, m_document->rootGroup(), targetDocIndex));
    } else {
        m_document->reorderLayer(id, m_document->rootGroup(), targetDocIndex);
    }

    m_selectedId = id;
    rebuild();
    emit selectionRequested(id);
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
    menu.addSeparator();
    QAction* quickExportAction = menu.addAction(m_i18n ? m_i18n->t("common", "panel.quickExportPng") : QStringLiteral("Quick Export as PNG"));
    menu.addSeparator();
    const bool portuguese = m_i18n && m_i18n->currentLanguage() == QStringLiteral("pt-BR");
    QAction* mergeAction = menu.addAction(portuguese ? QStringLiteral("Mesclar camadas (Ctrl+E)") : QStringLiteral("Merge layers (Ctrl+E)"));
    QAction* flattenAction = menu.addAction(portuguese ? QStringLiteral("Achatar imagem") : QStringLiteral("Flatten image"));
    menu.addSeparator();
    QAction* groupAction = menu.addAction(m_i18n->t("common", "menu.layer.group"));
    QAction* ungroupAction = menu.addAction(m_i18n->t("common", "menu.layer.ungroup"));
    QAction* maskAction = menu.addAction(QStringLiteral("Apply mask from selection"));
    groupAction->setEnabled(m_list->selectedItems().size() >= 2);
    ungroupAction->setEnabled(layer->type() == LayerType::Group);
    maskAction->setEnabled(layer->type() == LayerType::Image && !m_document->pixelSelection().isNull());
    QAction* renameAction = menu.addAction(m_i18n->t("common", "action.rename"));
    QMenu* colorMenu = menu.addMenu(QStringLiteral("Color label"));
    const QList<QPair<QString, LayerColorTag>> colors = {
        {QStringLiteral("None"), LayerColorTag::None},
        {QStringLiteral("Red"), LayerColorTag::Red},
        {QStringLiteral("Orange"), LayerColorTag::Orange},
        {QStringLiteral("Yellow"), LayerColorTag::Yellow},
        {QStringLiteral("Green"), LayerColorTag::Green},
        {QStringLiteral("Blue"), LayerColorTag::Blue},
        {QStringLiteral("Purple"), LayerColorTag::Purple},
        {QStringLiteral("Gray"), LayerColorTag::Gray}
    };
    QHash<QAction*, LayerColorTag> colorActions;
    for (const auto& color : colors) {
        QAction* action = colorMenu->addAction(color.first);
        colorActions.insert(action, color.second);
    }

    QAction* chosen = menu.exec(m_list->mapToGlobal(pos));
    if (chosen == lockAction) {
        const bool newLock = !layer->locked;
        if (m_history) {
            m_history->execute(std::make_unique<LayerLockCommand>(
                *m_document, id, QStringLiteral("layer.lock"),
                &Document::setLayerLocked, layer->locked, newLock));
        } else {
            m_document->setLayerLocked(id, newLock);
        }
        rebuild();
    } else if (chosen == frontAction) {
        onLayerReordered(id, -1);
    } else if (chosen == backAction) {
        onLayerReordered(id, 0);
    } else if (chosen == duplicateAction) {
        emit duplicateRequested(id);
    } else if (chosen == deleteAction) {
        emit deleteRequested(id);
    } else if (chosen == quickExportAction) {
        emit quickExportRequested(id);
    } else if (chosen == mergeAction) {
        emit mergeRequested();
    } else if (chosen == flattenAction) {
        emit flattenRequested();
    } else if (chosen == groupAction) {
        if (!m_history)
            return;
        QList<LayerId> ids;
        for (QListWidgetItem* selected : m_list->selectedItems())
            ids.append(selected->data(Qt::UserRole).value<LayerId>());
        if (!ids.contains(id))
            ids.append(id);
        if (ids.size() < 2)
            return;
        auto command = std::make_unique<GroupLayersCommand>(*m_document, ids);
        const LayerId groupId = command->groupId();
        m_history->execute(std::move(command));
        m_selectedId = groupId;
        rebuild();
        emit selectionRequested(groupId);
    } else if (chosen == ungroupAction) {
        if (!m_history || layer->type() != LayerType::Group)
            return;
        m_history->execute(std::make_unique<UngroupLayersCommand>(*m_document, id));
        rebuild();
    } else if (chosen == maskAction) {
        if (layer->type() != LayerType::Image || m_document->pixelSelection().isNull())
            return;
        if (SelectionOperations::applyMask(*m_document, m_history, id, true))
            rebuild();
    } else if (chosen == renameAction) {
        bool accepted = false;
        const QString name = QInputDialog::getText(this, m_i18n->t("common", "action.rename"),
                                                    m_i18n->t("common", "panel.layerName"),
                                                    QLineEdit::Normal, layer->name, &accepted);
        if (accepted && !name.trimmed().isEmpty()) {
            if (m_history)
                m_history->execute(std::make_unique<RenameLayerCommand>(
                    *m_document, id, QStringLiteral("layer.rename"),
                    &Document::setLayerName, layer->name, name.trimmed()));
            else
                m_document->setLayerName(id, name.trimmed());
            rebuild();
        }
    } else if (colorActions.contains(chosen)) {
        const LayerColorTag next = colorActions.value(chosen);
        if (m_history)
            m_history->execute(std::make_unique<LayerColorTagCommand>(
                *m_document, id, QStringLiteral("layer.colorTag"),
                &Document::setLayerColorTag, layer->colorTag, next));
        else
            m_document->setLayerColorTag(id, next);
        rebuild();
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
    if (m_searchEdit)
        m_searchEdit->setPlaceholderText(m_i18n->t("common", "panel.searchLayers"));
    if (m_typeFilter) {
        const int current = m_typeFilter->currentIndex();
        m_typeFilter->setItemText(0, m_i18n->t("common", "panel.allTypes"));
        m_typeFilter->setItemText(1, m_i18n->t("common", "layer.typeText"));
        m_typeFilter->setItemText(2, m_i18n->t("common", "layer.typeImage"));
        m_typeFilter->setItemText(3, m_i18n->t("common", "layer.typeShape"));
        m_typeFilter->setItemText(4, m_i18n->t("common", "layer.typeGroup"));
        m_typeFilter->setItemText(5, m_i18n->t("common", "layer.typeBackground"));
        m_typeFilter->setCurrentIndex(current);
    }

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
    m_blendCombo->addItem(QStringLiteral("Color Dodge"));
    m_blendCombo->addItem(QStringLiteral("Color Burn"));
    m_blendCombo->addItem(QStringLiteral("Hard Light"));
    m_blendCombo->addItem(QStringLiteral("Soft Light"));
    m_blendCombo->addItem(QStringLiteral("Difference"));
    m_blendCombo->addItem(QStringLiteral("Exclusion"));
    if (currentIdx >= 0 && currentIdx < m_blendCombo->count())
        m_blendCombo->setCurrentIndex(currentIdx);
    m_updating = prevUpdating;

    rebuild();
}

void LayersPanel::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        rebuild();
    }
}

} // namespace cc

#include "layerspanel.moc"
