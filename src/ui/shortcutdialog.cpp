#include "shortcutdialog.h"

#include <QAction>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "actionmanager.h"

namespace cc {

ShortcutDialog::ShortcutDialog(ActionManager* actionManager, QWidget* parent)
    : QDialog(parent)
    , m_actionManager(actionManager)
{
    setWindowTitle(tr("Atalhos de Teclado"));
    setMinimumSize(700, 500);
    buildUi();
    populateActions();
}

void ShortcutDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);

    auto* infoLabel = new QLabel(tr("Clique duas vezes em um atalho para altera-lo.\nPressione a nova combinacao de teclas desejada."));
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Acao"),
        QStringLiteral("Atalho Atual"),
        QStringLiteral("Novo Atalho")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    connect(m_table, &QTableWidget::cellDoubleClicked, this, &ShortcutDialog::editShortcut);

    auto* btnLayout = new QHBoxLayout();

    auto* resetBtn = new QPushButton(tr("Redefinir Selecionado"));
    auto* resetAllBtn = new QPushButton(tr("Redefinir Todos"));
    auto* applyBtn = new QPushButton(tr("Aplicar"));
    auto* closeBtn = new QPushButton(tr("Fechar"));

    btnLayout->addStretch();
    btnLayout->addWidget(resetBtn);
    btnLayout->addWidget(resetAllBtn);
    btnLayout->addWidget(applyBtn);
    btnLayout->addWidget(closeBtn);

    connect(resetBtn, &QPushButton::clicked, this, &ShortcutDialog::resetSelectedShortcut);
    connect(resetAllBtn, &QPushButton::clicked, this, &ShortcutDialog::resetAllShortcuts);
    connect(applyBtn, &QPushButton::clicked, this, &ShortcutDialog::applyChanges);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    layout->addLayout(btnLayout);
}

void ShortcutDialog::populateActions()
{
    m_table->setRowCount(0);
    if (!m_actionManager)
        return;

    m_originalShortcuts = m_actionManager->getAllShortcuts();
    for (auto it = m_originalShortcuts.cbegin(); it != m_originalShortcuts.cend(); ++it) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto* actionItem = new QTableWidgetItem(it.key());
        actionItem->setData(Qt::UserRole, it.key());
        actionItem->setFlags(actionItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 0, actionItem);

        const QKeySequence current = m_changedShortcuts.value(it.key(), it.value());
        const QString shortcutText = current.isEmpty()
            ? tr("Sem atalho")
            : current.toString(QKeySequence::NativeText);
        auto* currentItem = new QTableWidgetItem(shortcutText);
        currentItem->setFlags(currentItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 1, currentItem);

        auto* pendingItem = new QTableWidgetItem(
            m_changedShortcuts.contains(it.key()) ? shortcutText : QString());
        pendingItem->setFlags(pendingItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 2, pendingItem);
    }
    m_table->resizeColumnsToContents();
}

void ShortcutDialog::editShortcut(int row)
{
    if (row < 0 || row >= m_table->rowCount())
        return;
    const QString actionName = m_table->item(row, 0)->data(Qt::UserRole).toString();
    const QKeySequence current = m_changedShortcuts.value(
        actionName, m_originalShortcuts.value(actionName));

    QDialog editor(this);
    editor.setWindowTitle(tr("Definir atalho"));
    auto* layout = new QVBoxLayout(&editor);
    layout->addWidget(new QLabel(tr("Pressione a combinação para %1.").arg(actionName), &editor));
    auto* keyEdit = new QKeySequenceEdit(current, &editor);
    layout->addWidget(keyEdit);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         Qt::Horizontal, &editor);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &editor, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &editor, &QDialog::reject);
    if (editor.exec() != QDialog::Accepted)
        return;

    const QKeySequence replacement = keyEdit->keySequence();
    if (!replacement.isEmpty()) {
        const auto effectiveShortcut = [this](const QString& name) {
            return m_changedShortcuts.value(name, m_originalShortcuts.value(name));
        };
        for (auto it = m_originalShortcuts.cbegin(); it != m_originalShortcuts.cend(); ++it) {
            if (it.key() != actionName && effectiveShortcut(it.key()) == replacement) {
                QMessageBox::warning(this, tr("Atalho em uso"),
                                     tr("%1 já usa %2.")
                                         .arg(it.key(), replacement.toString(QKeySequence::NativeText)));
                return;
            }
        }
    }
    m_changedShortcuts[actionName] = replacement;
    populateActions();
}

void ShortcutDialog::resetSelectedShortcut()
{
    const int row = m_table->currentRow();
    if (row < 0)
        return;
    const QString actionName = m_table->item(row, 0)->data(Qt::UserRole).toString();
    m_changedShortcuts[actionName] = m_actionManager->defaultShortcut(actionName);
    populateActions();
}

void ShortcutDialog::resetAllShortcuts()
{
    if (!m_actionManager)
        return;
    for (auto it = m_originalShortcuts.cbegin(); it != m_originalShortcuts.cend(); ++it)
        m_changedShortcuts[it.key()] = m_actionManager->defaultShortcut(it.key());
    populateActions();
}

void ShortcutDialog::applyChanges()
{
    if (!m_actionManager)
        return;
    for (auto it = m_changedShortcuts.cbegin(); it != m_changedShortcuts.cend(); ++it) {
        if (it.value() == m_actionManager->defaultShortcut(it.key()))
            m_actionManager->resetShortcut(it.key());
        else
            m_actionManager->setShortcut(it.key(), it.value());
    }
    accept();
}

QMap<QString, QKeySequence> ShortcutDialog::changedShortcuts() const
{
    return m_changedShortcuts;
}

} // namespace cc
