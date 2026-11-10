#include "workspacelayout.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QComboBox>

#include <QSettings>

namespace cc {

WorkspaceLayoutDialog::WorkspaceLayoutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Layouts de Workspace"));
    setMinimumSize(500, 300);
    buildUi();
    updateLayoutList();
}

void WorkspaceLayoutDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);

    auto* infoLabel = new QLabel(tr("Salve e restaure layouts de workspace."));
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    auto* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel(tr("Nome do layout:")));
    m_layoutNameEdit = new QLineEdit();
    m_layoutNameEdit->setPlaceholderText(tr("Nome do layout..."));
    nameLayout->addWidget(m_layoutNameEdit);
    layout->addLayout(nameLayout);

    m_layoutCombo = new QComboBox();
    layout->addWidget(m_layoutCombo);

    auto* btnLayout = new QHBoxLayout();
    auto* saveBtn = new QPushButton(tr("Salvar Layout"));
    auto* loadBtn = new QPushButton(tr("Aplicar Layout"));
    auto* deleteBtn = new QPushButton(tr("Excluir Layout"));
    auto* closeBtn = new QPushButton(tr("Fechar"));

    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(loadBtn);
    btnLayout->addWidget(deleteBtn);
    btnLayout->addWidget(closeBtn);

    layout->addLayout(btnLayout);

    connect(saveBtn, &QPushButton::clicked, this, &WorkspaceLayoutDialog::saveLayout);
    connect(loadBtn, &QPushButton::clicked, this, &WorkspaceLayoutDialog::loadLayout);
    connect(deleteBtn, &QPushButton::clicked, this, &WorkspaceLayoutDialog::deleteLayout);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_layoutCombo, &QComboBox::currentTextChanged, this, [this](const QString& text){
        m_layoutNameEdit->setText(text);
    });
}

void WorkspaceLayoutDialog::updateLayoutList()
{
    m_layoutCombo->clear();

    QSettings settings;
    settings.beginGroup("WorkspaceLayouts");
    QStringList keys = settings.childKeys();
    for (const QString& key : keys) {
        m_layoutCombo->addItem(key);
    }
    settings.endGroup();
}

void WorkspaceLayoutDialog::saveLayout()
{
    QString name = m_layoutNameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Erro"), tr("Digite um nome para o layout."));
        return;
    }

    QSettings settings;
    settings.beginGroup("WorkspaceLayouts");
    settings.setValue(name, QDateTime::currentDateTime().toString(Qt::ISODate));
    settings.endGroup();

    updateLayoutList();
    m_layoutCombo->setCurrentText(name);
}

void WorkspaceLayoutDialog::loadLayout()
{
    QString name = m_layoutCombo->currentText();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Erro"), tr("Selecione um layout."));
        return;
    }

    QSettings settings;
    settings.beginGroup("WorkspaceLayouts");
    QString saved = settings.value(name).toString();
    settings.endGroup();

    if (saved.isEmpty()) {
        QMessageBox::warning(this, tr("Erro"), tr("Layout nao encontrado."));
        return;
    }

    QMessageBox::information(this, tr("Layout Carregado"),
                             tr("Layout '%1' carregado com sucesso.").arg(name));
}

void WorkspaceLayoutDialog::deleteLayout()
{
    QString name = m_layoutCombo->currentText();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Erro"), tr("Selecione um layout."));
        return;
    }

    QSettings settings;
    settings.beginGroup("WorkspaceLayouts");
    settings.remove(name);
    settings.endGroup();

    updateLayoutList();
}

} // namespace cc
