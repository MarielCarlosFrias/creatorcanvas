#include "layercompsdialog.h"

#include "core/Document.h"
#include "core/commands/LayerCompCommands.h"
#include "core/history/CommandStack.h"
#include "core/layers/Layer.h"
#include "localization/i18nservice.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace cc {

LayerCompsDialog::LayerCompsDialog(Document& doc, CommandStack* history,
                                   I18nService* i18n, QWidget* parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_history(history)
    , m_i18n(i18n)
{
    setWindowTitle(tr("title", QStringLiteral("Composições de Camadas - CreatorCanvas")));
    setMinimumSize(460, 380);

    auto* layout = new QVBoxLayout(this);

    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

    auto* flagsRow = new QHBoxLayout;
    m_visibilityCheck = new QCheckBox(tr("captureVisibility", QStringLiteral("Visibilidade")), this);
    m_positionCheck = new QCheckBox(tr("capturePosition", QStringLiteral("Posição")), this);
    m_appearanceCheck = new QCheckBox(tr("captureAppearance", QStringLiteral("Aparência")), this);
    m_visibilityCheck->setChecked(true);
    m_positionCheck->setChecked(true);
    m_appearanceCheck->setChecked(true);
    flagsRow->addWidget(m_visibilityCheck);
    flagsRow->addWidget(m_positionCheck);
    flagsRow->addWidget(m_appearanceCheck);
    layout->addLayout(flagsRow);

    auto* buttons = new QHBoxLayout;
    auto* saveBtn = new QPushButton(tr("save", QStringLiteral("Guardar estado actual")), this);
    m_applyButton = new QPushButton(tr("apply", QStringLiteral("Aplicar")), this);
    m_removeButton = new QPushButton(tr("remove", QStringLiteral("Remover")), this);
    m_applyButton->setEnabled(false);
    m_removeButton->setEnabled(false);
    auto* closeBtn = new QPushButton(QStringLiteral("Fechar"), this);
    buttons->addWidget(saveBtn);
    buttons->addWidget(m_applyButton);
    buttons->addWidget(m_removeButton);
    buttons->addStretch(1);
    buttons->addWidget(closeBtn);
    layout->addLayout(buttons);

    connect(saveBtn, &QPushButton::clicked, this, &LayerCompsDialog::saveCurrentState);
    connect(m_applyButton, &QPushButton::clicked, this, &LayerCompsDialog::applySelected);
    connect(m_removeButton, &QPushButton::clicked, this, &LayerCompsDialog::removeSelected);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() {
        const bool has = !selectedCompId().isEmpty();
        m_applyButton->setEnabled(has);
        m_removeButton->setEnabled(has);
    });

    refreshList();
}

QString LayerCompsDialog::tr(const char* key, const QString& fallback) const
{
    return m_i18n ? m_i18n->t("editor", QStringLiteral("comps.") + QLatin1String(key))
                  : fallback;
}

void LayerCompsDialog::refreshList()
{
    m_list->clear();
    for (const auto& comp : m_doc.layerComps()) {
        QString label = comp.name.isEmpty() ? comp.id : comp.name;
        QStringList flags;
        if (comp.applyVisibility) flags << QStringLiteral("V");
        if (comp.applyPosition) flags << QStringLiteral("P");
        if (comp.applyAppearance) flags << QStringLiteral("A");
        auto* item = new QListWidgetItem(
            QStringLiteral("%1  [%2]").arg(label, flags.join(QStringLiteral(" "))));
        item->setData(Qt::UserRole, comp.id);
        if (!comp.comment.isEmpty())
            item->setToolTip(comp.comment);
        m_list->addItem(item);
    }
    m_applyButton->setEnabled(false);
    m_removeButton->setEnabled(false);
}

QString LayerCompsDialog::selectedCompId() const
{
    QListWidgetItem* item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QString LayerCompsDialog::saveCompWithName(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return {};
    LayerComp comp = compFromDocument(m_doc, trimmed,
                                      m_visibilityCheck->isChecked(),
                                      m_positionCheck->isChecked(),
                                      m_appearanceCheck->isChecked());
    if (comp.states.empty())
        return {};

    // Guardar o id ANTES do move: std::move esvazia o QString e devolver
    // comp.id depois daria uma string vazia (era o bug — o comp era
    // adicionado mas o id devolvido não servia para nada).
    const QString newId = comp.id;
    executeAddLayerComp(m_doc, m_history, std::move(comp));
    refreshList();
    return newId;
}

void LayerCompsDialog::saveCurrentState()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("saveTitle", QStringLiteral("Nova composição")),
        tr("savePrompt", QStringLiteral("Nome da composição:")),
        QLineEdit::Normal, QStringLiteral("Comp 1"), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    saveCompWithName(name);
}

bool LayerCompsDialog::applyCompById(const QString& id)
{
    if (id.isEmpty() || !executeApplyLayerComp(m_doc, m_history, id))
        return false;
    refreshList();
    return true;
}

bool LayerCompsDialog::removeCompById(const QString& id)
{
    if (id.isEmpty())
        return false;
    bool found = false;
    for (const auto& comp : m_doc.layerComps())
        if (comp.id == id) {
            found = true;
            break;
        }
    if (!found)
        return false;
    executeRemoveLayerComp(m_doc, m_history, id);
    refreshList();
    return true;
}

void LayerCompsDialog::applySelected()
{
    if (applyCompById(selectedCompId()))
        accept();
}

void LayerCompsDialog::removeSelected()
{
    removeCompById(selectedCompId());
}

} // namespace cc
