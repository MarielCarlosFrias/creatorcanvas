#include "workspacelayout.h"
#include "localization/i18nservice.h"

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

WorkspaceLayoutDialog::WorkspaceLayoutDialog(I18nService* i18n, QWidget* parent)
    : QDialog(parent)
    , m_i18n(i18n)
{
    setWindowTitle(tr_("title", QStringLiteral("Workspace Layouts")));
    setMinimumSize(500, 300);
    buildUi();
    updateLayoutList();
}

QString WorkspaceLayoutDialog::tr_(const QString& key, const QString& fallback) const
{
    return m_i18n ? m_i18n->t("common", QStringLiteral("workspaceDialog.") + key)
                  : fallback;
}

void WorkspaceLayoutDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);

    auto* infoLabel = new QLabel(tr_("info", QStringLiteral("Save and restore workspace layouts.")));
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    auto* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel(tr_("name", QStringLiteral("Layout name:"))));
    m_layoutNameEdit = new QLineEdit();
    m_layoutNameEdit->setPlaceholderText(tr_("namePlaceholder", QStringLiteral("Layout name...")));
    nameLayout->addWidget(m_layoutNameEdit);
    layout->addLayout(nameLayout);

    m_layoutCombo = new QComboBox();
    layout->addWidget(m_layoutCombo);

    auto* btnLayout = new QHBoxLayout();
    auto* saveBtn = new QPushButton(tr_("save", QStringLiteral("Save Layout")));
    auto* loadBtn = new QPushButton(tr_("apply", QStringLiteral("Apply Layout")));
    auto* deleteBtn = new QPushButton(tr_("delete", QStringLiteral("Delete Layout")));
    auto* closeBtn = new QPushButton(tr_("close", QStringLiteral("Close")));

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
        QMessageBox::warning(this, tr_("error", QStringLiteral("Error")), tr_("enterName", QStringLiteral("Enter a layout name.")));
        return;
    }
    if (name.contains(u'/') || name.contains(u'\\')) {
        QMessageBox::warning(this, tr_("error", QStringLiteral("Error")),
                             tr_("invalidName", QStringLiteral("The layout name cannot contain / or \\.")));
        return;
    }

    QMainWindow* window = mainWindow();
    if (!window) {
        QMessageBox::warning(this, tr_("error", QStringLiteral("Error")),
                             tr_("noMainWindow", QStringLiteral("Could not find the main window.")));
        return;
    }

    const QByteArray geometry = window->saveGeometry();
    const QByteArray state = window->saveState();
    if (geometry.isEmpty() || state.isEmpty()) {
        QMessageBox::warning(this, tr_("error", QStringLiteral("Error")),
                             tr_("saveFailed", QStringLiteral("Could not save the workspace state.")));
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
        QMessageBox::warning(this, tr_("error", QStringLiteral("Error")), tr_("selectLayout", QStringLiteral("Select a layout.")));
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
        QMessageBox::warning(this, tr_("error", QStringLiteral("Error")), tr_("notFound", QStringLiteral("Layout not found.")));
        return;
    }

    if (!restoreLayout(geometry, state)) {
        QMessageBox::warning(this, tr_("error", QStringLiteral("Error")),
                             tr_("applyFailed", QStringLiteral("Could not apply this layout. It may belong to another screen.")));
        return;
    }
    accept();
}

void WorkspaceLayoutDialog::deleteLayout()
{
    QString name = m_layoutCombo->currentText();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr_("error", QStringLiteral("Error")), tr_("selectLayout", QStringLiteral("Select a layout.")));
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
