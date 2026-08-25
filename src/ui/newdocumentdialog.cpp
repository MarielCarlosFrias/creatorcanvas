#include "newdocumentdialog.h"

#include "localization/i18nservice.h"
#include "services/presetstore.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace cc {

NewDocumentDialog::NewDocumentDialog(I18nService* i18n, PresetStore* presets,
                                     QWidget* parent)
    : QDialog(parent)
    , m_i18n(i18n)
    , m_presets(presets)
{
    buildUi();
    populatePresetList();
    updateColorButtonText();

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &NewDocumentDialog::retranslateUi);
    retranslateUi();
}

void NewDocumentDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);

    m_presetsTitle = new QLabel(this);
    m_presetList = new QListWidget(this);
    connect(m_presetList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem* current) { applyPreset(current); });
    layout->addWidget(m_presetsTitle);
    layout->addWidget(m_presetList);

    m_width = new QSpinBox(this);
    m_width->setRange(1, 20000);
    m_width->setValue(1280);
    m_height = new QSpinBox(this);
    m_height->setRange(1, 20000);
    m_height->setValue(720);
    m_dpi = new QSpinBox(this);
    m_dpi->setRange(1, 2400);
    m_dpi->setValue(96);

    m_widthLabel = new QLabel(this);
    m_heightLabel = new QLabel(this);
    m_dpiLabel = new QLabel(this);
    auto* form = new QFormLayout;
    form->addRow(m_widthLabel, m_width);
    form->addRow(m_heightLabel, m_height);
    form->addRow(m_dpiLabel, m_dpi);
    layout->addLayout(form);

    m_backgroundLabel = new QLabel(this);
    m_colorButton = new QPushButton(this);
    connect(m_colorButton, &QPushButton::clicked, this, [this] {
        const QColor chosen = QColorDialog::getColor(m_backgroundColor, this);
        if (chosen.isValid()) {
            m_backgroundColor = chosen;
            updateColorButtonText();
        }
    });

    m_transparent = new QCheckBox(this);
    connect(m_transparent, &QCheckBox::toggled,
            m_colorButton, &QPushButton::setDisabled);

    auto* backgroundRow = new QHBoxLayout;
    backgroundRow->addWidget(m_colorButton);
    backgroundRow->addWidget(m_transparent);
    backgroundRow->addStretch();
    form->addRow(m_backgroundLabel, backgroundRow);

    m_savePresetButton = new QPushButton(this);
    connect(m_savePresetButton, &QPushButton::clicked,
            this, &NewDocumentDialog::savePreset);
    layout->addWidget(m_savePresetButton);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void NewDocumentDialog::populatePresetList()
{
    m_presetList->clear();
    if (!m_presets)
        return;
    for (const DocumentPreset& preset : m_presets->all()) {
        auto* item = new QListWidgetItem(
            QStringLiteral("%1  (%2 × %3)")
                .arg(preset.name,
                     QString::number(preset.spec.width),
                     QString::number(preset.spec.height)),
            m_presetList);
        item->setData(Qt::UserRole, QVariant::fromValue(preset));
    }
}

void NewDocumentDialog::applyPreset(QListWidgetItem* current)
{
    if (!current)
        return;
    const DocumentPreset preset =
        current->data(Qt::UserRole).value<DocumentPreset>();
    m_width->setValue(preset.spec.width);
    m_height->setValue(preset.spec.height);
    m_dpi->setValue(preset.spec.dpi);
    m_transparent->setChecked(preset.spec.transparentBackground);
    m_backgroundColor = preset.spec.backgroundColor;
    updateColorButtonText();
}

void NewDocumentDialog::updateColorButtonText()
{
    m_colorButton->setText(m_backgroundColor.name(QColor::HexRgb));
}

void NewDocumentDialog::savePreset()
{
    if (!m_i18n || !m_presets)
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(
        this,
        m_i18n->t("editor", "dialog.newDocument.presetName"),
        m_i18n->t("editor", "dialog.newDocument.presetName"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || name.isEmpty())
        return;

    DocumentPreset preset;
    preset.name = name;
    preset.spec.width = m_width->value();
    preset.spec.height = m_height->value();
    preset.spec.dpi = m_dpi->value();
    preset.spec.transparentBackground = m_transparent->isChecked();
    preset.spec.backgroundColor = m_backgroundColor;

    if (!m_presets->addUserPreset(preset)) {
        QMessageBox::warning(this,
                             m_i18n->t("editor", "dialog.newDocument.title"),
                             m_i18n->t("editor", "dialog.newDocument.exists"));
        return;
    }
    populatePresetList();
}

NewDocumentSpec NewDocumentDialog::spec() const
{
    NewDocumentSpec spec;
    spec.width = m_width->value();
    spec.height = m_height->value();
    spec.dpi = m_dpi->value();
    spec.transparentBackground = m_transparent->isChecked();
    spec.backgroundColor = m_backgroundColor;
    return spec;
}

void NewDocumentDialog::retranslateUi()
{
    if (!m_i18n)
        return;

    setWindowTitle(m_i18n->t("editor", "dialog.newDocument.title"));
    m_presetsTitle->setText(m_i18n->t("editor", "dialog.newDocument.presets"));
    m_widthLabel->setText(m_i18n->t("editor", "dialog.newDocument.width"));
    m_heightLabel->setText(m_i18n->t("editor", "dialog.newDocument.height"));
    m_dpiLabel->setText(m_i18n->t("editor", "dialog.newDocument.dpi"));
    m_backgroundLabel->setText(m_i18n->t("editor", "dialog.newDocument.background"));
    m_transparent->setText(m_i18n->t("editor", "dialog.newDocument.transparent"));
    m_savePresetButton->setText(m_i18n->t("editor", "dialog.newDocument.savePreset"));
}

} // namespace cc
