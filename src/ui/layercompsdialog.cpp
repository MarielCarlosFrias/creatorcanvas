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

void LayerCompsDialog::saveCurrentState()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("saveTitle", QStringLiteral("Nova composição")),
        tr("savePrompt", QStringLiteral("Nome da composição:")),
        QLineEdit::Normal, QStringLiteral("Comp 1"), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    LayerComp comp = compFromDocument(m_doc, name.trimmed(),
                                      m_visibilityCheck->isChecked(),
                                      m_positionCheck->isChecked(),
                                      m_appearanceCheck->isChecked());
    if (comp.states.empty())
        return;
    executeAddLayerComp(m_doc, m_history, std::move(comp));
    refreshList();
}

void LayerCompsDialog::applySelected()
{
    const QString id = selectedCompId();
    if (id.isEmpty())
        return;
    if (executeApplyLayerComp(m_doc, m_history, id)) {
        refreshList();
        accept();
    }
}

void LayerCompsDialog::removeSelected()
{
    const QString id = selectedCompId();
    if (id.isEmpty())
        return;
    executeRemoveLayerComp(m_doc, m_history, id);
    refreshList();
}

} // namespace cc
