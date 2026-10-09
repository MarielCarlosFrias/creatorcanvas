#include "historypanel.h"
#include "core/history/CommandStack.h"
#include "localization/i18nservice.h"
#include "themeicons.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QToolButton>
#include <QLabel>
#include <QIcon>
#include <QFont>
#include <QPainter>
#include <QEvent>

namespace cc {

HistoryPanel::HistoryPanel(I18nService* i18n, CommandStack* history, QWidget* parent)
    : QWidget(parent)
    , m_i18n(i18n)
    , m_history(nullptr)
{
    buildUi();
    if (m_i18n) {
        connect(m_i18n, &I18nService::languageChanged, this, &HistoryPanel::retranslateUi);
    }
    setHistory(history);
}

void HistoryPanel::setHistory(CommandStack* history)
{
    if (m_history == history)
        return;

    if (m_history) {
        disconnect(m_history, &CommandStack::stateChanged, this, &HistoryPanel::refresh);
    }

    m_history = history;

    if (m_history) {
        connect(m_history, &CommandStack::stateChanged, this, &HistoryPanel::refresh);
    }

    refresh();
}

void HistoryPanel::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // List of history states
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("HistoryListWidget"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setFocusPolicy(Qt::NoFocus);

    connect(m_list, &QListWidget::itemClicked, this, &HistoryPanel::onItemClicked);
    mainLayout->addWidget(m_list, 1);

    // Bottom action bar
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(2, 0, 2, 0);
    bottomLayout->setSpacing(6);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("HistoryStatusLabel"));
    m_statusLabel->setStyleSheet(QStringLiteral("font-size: 11px;"));
    bottomLayout->addWidget(m_statusLabel, 1);

    m_undoBtn = new QToolButton(this);
    m_undoBtn->setObjectName(QStringLiteral("HistoryUndoButton"));
    m_undoBtn->setIcon(ThemeIcons::actionUndo());
    m_undoBtn->setIconSize(QSize(18, 18));
    m_undoBtn->setAutoRaise(true);
    connect(m_undoBtn, &QToolButton::clicked, this, [this] {
        if (m_history && m_history->canUndo())
            m_history->undo();
    });
    bottomLayout->addWidget(m_undoBtn);

    m_redoBtn = new QToolButton(this);
    m_redoBtn->setObjectName(QStringLiteral("HistoryRedoButton"));
    m_redoBtn->setIcon(ThemeIcons::actionRedo());
    m_redoBtn->setIconSize(QSize(18, 18));
    m_redoBtn->setAutoRaise(true);
    connect(m_redoBtn, &QToolButton::clicked, this, [this] {
        if (m_history && m_history->canRedo())
            m_history->redo();
    });
    bottomLayout->addWidget(m_redoBtn);

    mainLayout->addLayout(bottomLayout);

    retranslateUi();
}

void HistoryPanel::retranslateUi()
{
    if (m_undoBtn)
        m_undoBtn->setToolTip(m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("action.undo")) : QStringLiteral("Undo"));
    if (m_redoBtn)
        m_redoBtn->setToolTip(m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("menu.edit.redo")) : QStringLiteral("Redo"));

    refresh();
}

QString HistoryPanel::friendlyCommandName(const QString& rawName) const
{
    if (rawName == QLatin1String("layer.add")) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.addLayer")) : QStringLiteral("Add Layer");
    } else if (rawName == QLatin1String("layer.remove")) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.delete")) : QStringLiteral("Delete Layer");
    } else if (rawName == QLatin1String("layer.duplicate")) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.duplicate")) : QStringLiteral("Duplicate Layer");
    } else if (rawName == QLatin1String("layer.reorder")) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.moveUp")) : QStringLiteral("Reorder Layer");
    } else if (rawName == QLatin1String("layer.transform")) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.history.transform")) : QStringLiteral("Transform Layer");
    } else if (rawName == QLatin1String("layer.text")) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.history.text")) : QStringLiteral("Edit Text");
    } else if (rawName == QLatin1String("layer.textBox")) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.history.textBox")) : QStringLiteral("Resize Text Box");
    } else if (rawName == QLatin1String("layer.textEffects")) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.history.textEffects")) : QStringLiteral("Text Effects");
    } else if (rawName == QLatin1String("layer.imageEffects")) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.history.imageEffects")) : QStringLiteral("Image Effects");
    } else if (rawName == QLatin1String("layer.opacity")) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.opacity")) : QStringLiteral("Opacity");
    } else if (rawName == QLatin1String("layer.blendMode")) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.blendMode")) : QStringLiteral("Blend Mode");
    } else if (rawName == QLatin1String("layer.visibility")) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.visibility")) : QStringLiteral("Visibility");
    } else if (rawName == QLatin1String("layer.lock")) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.lock")) : QStringLiteral("Lock");
    } else if (rawName == QLatin1String("layer.rename")) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("action.rename")) : QStringLiteral("Rename");
    } else if (rawName == QLatin1String("layer.paint")) {
        return m_i18n ? m_i18n->t(QStringLiteral("editor"), QStringLiteral("canvas.layer.paint")) : QStringLiteral("Paint");
    } else if (rawName == QLatin1String("canvas.command.cropImage")) {
        return m_i18n ? m_i18n->t(QStringLiteral("editor"), QStringLiteral("canvas.command.cropImage")) : QStringLiteral("Crop Image");
    } else if (rawName == QLatin1String("canvas.command.scissorsCut")) {
        return m_i18n ? m_i18n->t(QStringLiteral("editor"), QStringLiteral("canvas.command.scissorsCut")) : QStringLiteral("Scissors Cut");
    } else if (rawName == QLatin1String("canvas.command.magicWandRemoval")) {
        return m_i18n ? m_i18n->t(QStringLiteral("editor"), QStringLiteral("canvas.command.magicWandRemoval")) : QStringLiteral("Magic Wand Cut");
    } else if (rawName == QLatin1String("canvas.command.cloneStamp")) {
        return m_i18n ? m_i18n->t(QStringLiteral("editor"), QStringLiteral("canvas.command.cloneStamp")) : QStringLiteral("Clone Stamp");
    } else if (rawName == QLatin1String("canvas.command.paintBucket")) {
        return m_i18n ? m_i18n->t(QStringLiteral("editor"), QStringLiteral("canvas.command.paintBucket")) : QStringLiteral("Paint Bucket");
    } else if (rawName == QLatin1String("canvas.command.aiBackground")) {
        return m_i18n ? m_i18n->t(QStringLiteral("editor"), QStringLiteral("canvas.command.aiBackground")) : QStringLiteral("AI Background Removal");
    } else if (rawName == QLatin1String("canvas.command.aiBackgroundQuick")) {
        return m_i18n ? m_i18n->t(QStringLiteral("editor"), QStringLiteral("canvas.command.aiBackgroundQuick")) : QStringLiteral("Quick AI Cutout");
    } else if (rawName == QLatin1String("layer.group")) {
        return m_i18n ? m_i18n->t(QStringLiteral("editor"), QStringLiteral("canvas.layer.group")) : QStringLiteral("Group Layers");
    } else if (rawName.startsWith(QLatin1String("layer.color."))) {
        return m_i18n ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.history.layerColor")) : QStringLiteral("Layer Color");
    } else if (rawName.startsWith(QLatin1String("layer."))) {
        return rawName.mid(6);
    }
    return rawName;
}

void HistoryPanel::refresh()
{
    if (m_updating)
        return;

    m_updating = true;

    if (m_undoBtn)
        m_undoBtn->setEnabled(m_history && m_history->canUndo());
    if (m_redoBtn)
        m_redoBtn->setEnabled(m_history && m_history->canRedo());

    int total = m_history ? m_history->totalCount() : 0;
    int current = m_history ? m_history->currentIndex() : 0;

    if (m_statusLabel) {
        m_statusLabel->setText(QStringLiteral("%1 / %2").arg(current).arg(total));
    }

    m_list->clear();

    // Item 0: Initial state (document creation/open)
    const QString initialTitle = m_i18n
        ? m_i18n->t(QStringLiteral("common"), QStringLiteral("panel.history.initial"))
        : QStringLiteral("New Document");

    auto* initItem = new QListWidgetItem(m_list);
    initItem->setText(QStringLiteral("0. %1").arg(initialTitle));
    initItem->setIcon(ThemeIcons::actionNewDocument());
    initItem->setData(Qt::UserRole, 0);

    const bool darkUi = palette().color(QPalette::Window).value() < 128;
    const QColor initialPastColor = darkUi ? QColor(180, 185, 195) : QColor(71, 85, 105);
    const QColor futureColor = darkUi ? QColor(95, 102, 115) : QColor(148, 163, 184);
    const QColor pastColor = darkUi ? QColor(210, 215, 225) : QColor(30, 41, 59);

    if (current == 0) {
        initItem->setSelected(true);
    } else {
        // Already executed state
        initItem->setForeground(initialPastColor);
    }

    // Following states: recorded commands
    for (int i = 0; i < total; ++i) {
        QString raw = m_history->commandNameAt(i);
        QString friendly = friendlyCommandName(raw);

        auto* item = new QListWidgetItem(m_list);
        item->setText(QStringLiteral("%1. %2").arg(i + 1).arg(friendly));
        item->setData(Qt::UserRole, i + 1);

        // Appropriate icon by command type
        if (raw.contains(QLatin1String("paint"))) {
            item->setIcon(ThemeIcons::toolPaint());
        } else if (raw.contains(QLatin1String("crop"))) {
            item->setIcon(ThemeIcons::toolCrop());
        } else if (raw.contains(QLatin1String("scissors"))) {
            item->setIcon(ThemeIcons::toolScissors());
        } else if (raw.contains(QLatin1String("text"))) {
            item->setIcon(ThemeIcons::toolText());
        } else if (raw.contains(QLatin1String("shape"))) {
            item->setIcon(ThemeIcons::toolShape());
        } else if (raw.contains(QLatin1String("duplicate"))) {
            item->setIcon(ThemeIcons::actionDuplicate());
        } else if (raw.contains(QLatin1String("remove"))) {
            item->setIcon(ThemeIcons::actionDelete());
        } else {
            item->setIcon(ThemeIcons::actionHistory());
        }

        if (i + 1 == current) {
            item->setSelected(true);
        } else if (i + 1 > current) {
            // Future state (redoable): dimmed text style
            item->setForeground(futureColor);
        } else {
            // Past state (already executed): crisp normal text
            item->setForeground(pastColor);
        }
    }

    if (current >= 0 && current < m_list->count()) {
        m_list->setCurrentRow(current);
        m_list->scrollToItem(m_list->item(current));
    }

    m_updating = false;
}

void HistoryPanel::onItemClicked(QListWidgetItem* item)
{
    if (m_updating || !m_history || !item)
        return;

    int targetIndex = item->data(Qt::UserRole).toInt();
    if (targetIndex != m_history->currentIndex()) {
        m_history->jumpToState(targetIndex);
    }
}

void HistoryPanel::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        refresh();
    }
}

} // namespace cc
