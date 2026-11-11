#include "workspacelayout.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QComboBox>

#include <QSettings>

namespace cc {
namespace {

constexpr auto kLayoutsGroup = "WorkspaceLayouts";
constexpr auto kGeometryKey = "geometry";
constexpr auto kStateKey = "state";
constexpr auto kSavedAtKey = "savedAt";

bool fitsOnAvailableScreen(const QRect& rect)
{
    if (rect.isNull() || rect.isEmpty())
        return false;
    for (QScreen* screen : QGuiApplication::screens()) {
        if (!screen->availableGeometry().intersected(rect).isEmpty())
            return true;
    }
    return false;
}

} // namespace

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
    settings.beginGroup(kLayoutsGroup);
    const QStringList layouts = settings.childGroups();
    for (const QString& layout : layouts) {
        m_layoutCombo->addItem(layout);
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
    if (name.contains(u'/') || name.contains(u'\\')) {
        QMessageBox::warning(this, tr("Erro"),
                             tr("O nome do layout não pode conter / ou \\."));
        return;
    }

    QMainWindow* window = mainWindow();
    if (!window) {
        QMessageBox::warning(this, tr("Erro"),
                             tr("Não foi possível encontrar a janela principal."));
        return;
    }

    const QByteArray geometry = window->saveGeometry();
    const QByteArray state = window->saveState();
    if (geometry.isEmpty() || state.isEmpty()) {
        QMessageBox::warning(this, tr("Erro"),
                             tr("Não foi possível guardar o estado do workspace."));
        return;
    }

    QSettings settings;
    settings.beginGroup(kLayoutsGroup);
    settings.beginGroup(name);
    settings.setValue(kGeometryKey, geometry);
    settings.setValue(kStateKey, state);
    settings.setValue(kSavedAtKey, QDateTime::currentDateTime().toString(Qt::ISODate));
    settings.endGroup();
    settings.endGroup();
    settings.sync();

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
    settings.beginGroup(kLayoutsGroup);
    settings.beginGroup(name);
    const QByteArray geometry = settings.value(kGeometryKey).toByteArray();
    const QByteArray state = settings.value(kStateKey).toByteArray();
    settings.endGroup();
    settings.endGroup();

    if (geometry.isEmpty() || state.isEmpty()) {
        QMessageBox::warning(this, tr("Erro"), tr("Layout nao encontrado."));
        return;
    }

    if (!restoreLayout(geometry, state)) {
        QMessageBox::warning(this, tr("Erro"),
                             tr("Não foi possível aplicar este layout. Ele pode pertencer a outro ecrã."));
        return;
    }
    accept();
}

void WorkspaceLayoutDialog::deleteLayout()
{
    QString name = m_layoutCombo->currentText();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Erro"), tr("Selecione um layout."));
        return;
    }

    QSettings settings;
    settings.beginGroup(kLayoutsGroup);
    settings.remove(name);
    settings.endGroup();
    settings.sync();

    updateLayoutList();
}

QMainWindow* WorkspaceLayoutDialog::mainWindow() const
{
    return qobject_cast<QMainWindow*>(parentWidget());
}

bool WorkspaceLayoutDialog::restoreLayout(const QByteArray& geometry,
                                          const QByteArray& state)
{
    QMainWindow* window = mainWindow();
    if (!window)
        return false;

    // A geometria pode vir de um monitor que já não existe. Nesse caso não
    // aplicamos nem o estado dos docks para não deixar a janela inacessível.
    if (!window->restoreGeometry(geometry) || !fitsOnAvailableScreen(window->geometry()))
        return false;
    return window->restoreState(state);
}

} // namespace cc
