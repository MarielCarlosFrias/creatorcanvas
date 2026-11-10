#include "shortcutdialog.h"

#include <QAction>
#include <QHeaderView>
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

    connect(resetBtn, &QPushButton::clicked, this, &ShortcutDialog::resetShortcut);
    connect(resetAllBtn, &QPushButton::clicked, this, &ShortcutDialog::resetAllShortcuts);
    connect(applyBtn, &QPushButton::clicked, this, &ShortcutDialog::applyChanges);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    layout->addLayout(btnLayout);
}

void ShortcutDialog::populateActions()
{
    m_table->setRowCount(0);
}

void ShortcutDialog::editShortcut(int row)
{
    Q_UNUSED(row);
}

void ShortcutDialog::resetShortcut(int row)
{
    Q_UNUSED(row);
}

void ShortcutDialog::resetAllShortcuts()
{
    m_changedShortcuts.clear();
    populateActions();
}

void ShortcutDialog::applyChanges()
{
    accept();
}

} // namespace cc
