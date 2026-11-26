#include "ui/commandpalette.h"

#include "actionmanager.h"
#include "localization/i18nservice.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QScrollBar>
#include <QShowEvent>
#include <QVBoxLayout>

#include <algorithm>

namespace cc {

CommandPalette::CommandPalette(ActionManager* actions, I18nService* i18n,
                               QWidget* parent)
    : QDialog(parent)
    , m_actions(actions)
    , m_i18n(i18n)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setObjectName(QStringLiteral("CommandPalette"));
    setMinimumSize(620, 400);
    resize(700, 440);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 8);
    layout->setSpacing(6);

    m_query = new QLineEdit(this);
    m_query->setClearButtonEnabled(true);
    layout->addWidget(m_query);

    m_results = new QListWidget(this);
    m_results->setTextElideMode(Qt::ElideRight);
    m_results->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_results->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_results->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_results->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_results, 1);

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("PaletteHint"));
    layout->addWidget(m_hint);

    buildEntries();
    rebuildResults();

    connect(m_query, &QLineEdit::textChanged,
            this, &CommandPalette::onQueryChanged);
    connect(m_query, &QLineEdit::returnPressed, this, [this] {
        activate(m_results->currentItem());
    });
    connect(m_results, &QListWidget::itemActivated,
            this, &CommandPalette::onItemActivated);

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &CommandPalette::retranslateUi);
    retranslateUi();
}

void CommandPalette::refresh()
{
    buildEntries();
    if (m_query)
        m_query->clear();
    rebuildResults();
}

void CommandPalette::buildEntries()
{
    m_entries.clear();
    if (!m_actions)
        return;

    const QVector<QAction*> actions = m_actions->allActions();
    m_entries.reserve(actions.size());
    for (QAction* action : actions) {
        if (!action || action->text().isEmpty())
            continue;
        Entry entry;
        entry.action = action;
        entry.label = cleanLabel(action->text());
        QString haystack = entry.label.toLower();
        const QString id = m_actions->actionId(action);
        if (!id.isEmpty())
            haystack += QLatin1Char(' ') + id;
        const QString tip = cleanLabel(action->toolTip());
        if (!tip.isEmpty())
            haystack += QLatin1Char(' ') + tip.toLower();
        entry.haystack = haystack;
        m_entries.push_back(entry);
    }
}

void CommandPalette::rebuildResults()
{
    m_results->clear();
    const QString pattern = m_query->text().trimmed().toLower();

    struct Scored {
        int score = 0;
        int base = 0;
    };
    QVector<Scored> scored;
    scored.reserve(m_entries.size());
    for (int base = 0; base < m_entries.size(); ++base) {
        const Entry& entry = m_entries.at(base);
        int score = pattern.isEmpty() ? 1 : matchScore(pattern, entry.haystack);
        if (score <= 0)
            continue;
        // Ações indisponíveis agora (sem documento/camada) caem para o fim.
        if (entry.action && !entry.action->isEnabled())
            score -= 100000;
        scored.push_back({score, base});
    }
    std::sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) {
        if (a.score != b.score)
            return a.score > b.score;
        return a.base < b.base;
    });

    const QString disabledTag = m_i18n
        ? m_i18n->t("editor", "commandPalette.disabled")
        : QStringLiteral("unavailable");

    for (const Scored& s : scored) {
        const Entry& entry = m_entries.at(s.base);
        const bool enabled = entry.action->isEnabled();
        QListWidgetItem* item = new QListWidgetItem(entry.label);
        item->setIcon(entry.action->icon());
        // Índice em m_entries (estável durante a vida do popup).
        item->setData(Qt::UserRole, s.base);
        const QString shortcut = entry.action->shortcut().isEmpty()
            ? QString()
            : entry.action->shortcut().toString(QKeySequence::NativeText);
        if (!shortcut.isEmpty())
            item->setText(entry.label + QStringLiteral("   ") + shortcut);
        QString tip = groupLabel(m_actions->actionId(entry.action));
        if (!shortcut.isEmpty())
            tip += QStringLiteral("  ·  ") + shortcut;
        if (!enabled)
            tip += QStringLiteral("  ·  ") + disabledTag;
        item->setToolTip(tip);
        if (!enabled)
            item->setForeground(QBrush(QColor(0x88, 0x88, 0x88)));
        m_results->addItem(item);
    }
    if (m_results->count() > 0)
        m_results->setCurrentRow(0);
}

void CommandPalette::onQueryChanged(const QString&)
{
    rebuildResults();
}

void CommandPalette::onItemActivated(QListWidgetItem* item)
{
    activate(item);
}

void CommandPalette::activate(QListWidgetItem* item)
{
    // Enter pode chegar pelos dois caminhos (returnPressed do campo e
    // keyPressEvent do popup): a segunda chamada já encontra fechado.
    if (!isVisible())
        return;

    QAction* action = nullptr;
    if (item) {
        const int base = item->data(Qt::UserRole).toInt();
        if (base >= 0 && base < m_entries.size())
            action = m_entries.at(base).action;
    }
    close();
    if (action)
        action->trigger();
}

void CommandPalette::moveSelection(int delta)
{
    const int count = m_results->count();
    if (count <= 0)
        return;
    int row = m_results->currentRow();
    if (row < 0)
        row = delta > 0 ? 0 : count - 1;
    else
        row = (row + delta + count) % count;
    m_results->setCurrentRow(row);
}

void CommandPalette::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (QWidget* pw = parentWidget()) {
        const int x = (pw->width() - width()) / 2;
        const int y = qMin(120, qMax(20, pw->height() / 6));
        move(pw->mapToGlobal(QPoint(qMax(0, x), y)));
    }
    m_query->setFocus();
    m_query->selectAll();
}

void CommandPalette::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        close();
        return;
    case Qt::Key_Down:
        moveSelection(1);
        return;
    case Qt::Key_Up:
        moveSelection(-1);
        return;
    case Qt::Key_PageDown:
        moveSelection(5);
        return;
    case Qt::Key_PageUp:
        moveSelection(-5);
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        activate(m_results->currentItem());
        return;
    default:
        break;
    }
    QDialog::keyPressEvent(event);
}

int CommandPalette::matchScore(const QString& pattern, const QString& text)
{
    if (pattern.isEmpty())
        return 1;
    const int direct = text.indexOf(pattern);
    if (direct >= 0)
        return 10000 - direct * 4 + pattern.size() * 10;

    // Correspondencia por subsequencia (estilo VS Code): "plp" acha "Paleta...".
    int score = 400;
    int pos = 0;
    int streak = 0;
    for (const QChar c : pattern) {
        const int found = text.indexOf(c, pos);
        if (found < 0)
            return 0;
        streak = (found == pos && pos > 0) ? streak + 1 : 0;
        score += 20 - qMin(20, (found - pos) * 3) + streak * 8;
        pos = found + 1;
    }
    return score;
}

QString CommandPalette::cleanLabel(const QString& text)
{
    QString out = text;
    const int tab = out.indexOf(QLatin1Char('\t'));
    if (tab >= 0)
        out.truncate(tab);
    out.remove(QLatin1Char('&'));
    return out.simplified();
}

QString CommandPalette::groupLabel(const QString& id) const
{
    if (!m_i18n)
        return id;
    const QString prefix = id.section(QLatin1Char('.'), 0, 0);
    if (prefix == QLatin1String("file"))
        return m_i18n->t("editor", "commandPalette.group.file");
    if (prefix == QLatin1String("edit"))
        return m_i18n->t("editor", "commandPalette.group.edit");
    if (prefix == QLatin1String("view"))
        return m_i18n->t("editor", "commandPalette.group.view");
    if (prefix == QLatin1String("image"))
        return m_i18n->t("editor", "commandPalette.group.image");
    if (prefix == QLatin1String("filter"))
        return m_i18n->t("editor", "commandPalette.group.filter");
    if (prefix == QLatin1String("select"))
        return m_i18n->t("editor", "commandPalette.group.select");
    if (prefix == QLatin1String("layer"))
        return m_i18n->t("editor", "commandPalette.group.layer");
    if (prefix == QLatin1String("shape"))
        return m_i18n->t("editor", "commandPalette.group.shape");
    if (prefix == QLatin1String("align"))
        return m_i18n->t("editor", "commandPalette.group.align");
    if (prefix == QLatin1String("transform"))
        return m_i18n->t("editor", "commandPalette.group.transform");
    if (prefix == QLatin1String("tool"))
        return m_i18n->t("editor", "commandPalette.group.tool");
    if (prefix == QLatin1String("panel"))
        return m_i18n->t("editor", "commandPalette.group.panel");
    if (prefix == QLatin1String("adjustments"))
        return m_i18n->t("editor", "commandPalette.group.adjustments");
    if (prefix == QLatin1String("ai"))
        return m_i18n->t("editor", "commandPalette.group.ai");
    if (prefix == QLatin1String("help"))
        return m_i18n->t("editor", "commandPalette.group.help");
    if (prefix == QLatin1String("settings"))
        return m_i18n->t("editor", "commandPalette.group.settings");
    if (prefix == QLatin1String("window"))
        return m_i18n->t("editor", "commandPalette.group.window");
    return m_i18n->t("editor", "commandPalette.group.other");
}

void CommandPalette::retranslateUi()
{
    setWindowTitle(m_i18n ? m_i18n->t("editor", "commandPalette.title")
                          : QStringLiteral("Command Palette"));
    if (m_query)
        m_query->setPlaceholderText(
            m_i18n ? m_i18n->t("editor", "commandPalette.placeholder")
                   : QStringLiteral("Type a command..."));
    if (m_hint)
        m_hint->setText(m_i18n ? m_i18n->t("editor", "commandPalette.hint")
                               : QString());
    rebuildResults();
}

} // namespace cc
