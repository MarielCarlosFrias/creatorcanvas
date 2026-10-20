#include "newdocumentdialog.h"

#include "localization/i18nservice.h"
#include "services/presetstore.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabBar>
#include <QToolButton>
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
    setMinimumSize(480, 360);
    setMaximumSize(800, 600);
    resize(640, 480);
    setSizeGripEnabled(true);

    auto* layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetMinimumSize);

    m_categoryTabs = new QTabBar(this);
    m_categoryTabs->setExpanding(false);
    m_categoryTabs->setUsesScrollButtons(true);
    m_categoryTabs->setElideMode(Qt::ElideRight);
    m_categories = {m_i18n ? m_i18n->t("editor", "dialog.newDocument.category.all")
                           : QStringLiteral("All"),
                    m_i18n ? m_i18n->t("editor", "dialog.newDocument.category.youtube")
                           : QStringLiteral("YouTube"),
                    m_i18n ? m_i18n->t("editor", "dialog.newDocument.category.instagram")
                           : QStringLiteral("Instagram"),
                    m_i18n ? m_i18n->t("editor", "dialog.newDocument.category.tiktok")
                           : QStringLiteral("TikTok"),
                    m_i18n ? m_i18n->t("editor", "dialog.newDocument.category.custom")
                           : QStringLiteral("Custom")};
    for (const QString& category : m_categories)
        m_categoryTabs->addTab(category);
    connect(m_categoryTabs, &QTabBar::currentChanged,
            this, &NewDocumentDialog::onCategoryChanged);
    layout->addWidget(m_categoryTabs);

    m_presetsTitle = new QLabel(this);
    m_presetList = new QListWidget(this);
    m_presetList->setMaximumHeight(120);
    m_presetList->setSizePolicy(QSizePolicy::Expanding,
                                QSizePolicy::Preferred);
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
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->addRow(m_widthLabel, m_width);
    form->addRow(m_heightLabel, m_height);

    auto* orientationRow = new QHBoxLayout;
    m_orientPortrait = new QToolButton(this);
    m_orientLandscape = new QToolButton(this);
    m_orientPortrait->setText(m_i18n ? m_i18n->t("editor", "dialog.newDocument.orientation.portrait")
                                     : QStringLiteral("Portrait"));
    m_orientLandscape->setText(m_i18n ? m_i18n->t("editor", "dialog.newDocument.orientation.landscape")
                                      : QStringLiteral("Landscape"));
    m_orientPortrait->setCheckable(true);
    m_orientLandscape->setCheckable(true);
    m_orientPortrait->setAutoExclusive(true);
    m_orientLandscape->setAutoExclusive(true);
    m_orientLandscape->setChecked(true);
    connect(m_orientPortrait, &QToolButton::clicked,
            this, &NewDocumentDialog::swapOrientation);
    connect(m_orientLandscape, &QToolButton::clicked,
            this, &NewDocumentDialog::swapOrientation);
    orientationRow->addWidget(m_orientPortrait);
    orientationRow->addWidget(m_orientLandscape);
    orientationRow->addStretch();
    form->addRow(m_i18n ? m_i18n->t("editor", "dialog.newDocument.orientation")
                        : QStringLiteral("Orientation"), orientationRow);
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
    const int categoryIndex = m_categoryTabs ? m_categoryTabs->currentIndex() : 0;
    for (const DocumentPreset& preset : m_presets->all()) {
        const QString name = preset.name;
        bool visible = (categoryIndex == 0); // "All"
        if (categoryIndex == 1)              // "YouTube"
            visible = name.contains(QStringLiteral("youtube"), Qt::CaseInsensitive);
        else if (categoryIndex == 2)         // "Instagram"
            visible = name.contains(QStringLiteral("instagram"), Qt::CaseInsensitive);
        else if (categoryIndex == 3)         // "TikTok"
            visible = name.contains(QStringLiteral("tiktok"), Qt::CaseInsensitive);
        else if (categoryIndex == 4)         // "Custom"
            visible = preset.category.compare(QStringLiteral("Custom"), Qt::CaseInsensitive) == 0;
        if (!visible)
            continue;
        auto* item = new QListWidgetItem(
            QStringLiteral("%1  (%2 × %3)")
                .arg(preset.name,
                     QString::number(preset.spec.width),
                     QString::number(preset.spec.height)),
            m_presetList);
        item->setData(Qt::UserRole, QVariant::fromValue(preset));
    }
}

void NewDocumentDialog::onCategoryChanged(int)
{
    populatePresetList();
}

void NewDocumentDialog::swapOrientation()
{
    const bool portrait = m_orientPortrait && m_orientPortrait->isChecked();
    if (portrait == (m_height->value() > m_width->value()))
        return;
    const int width = m_width->value();
    m_width->setValue(m_height->value());
    m_height->setValue(width);
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

    // Categorias das abas
    const QStringList cats = {
        m_i18n->t("editor", "dialog.newDocument.category.all"),
        m_i18n->t("editor", "dialog.newDocument.category.youtube"),
        m_i18n->t("editor", "dialog.newDocument.category.instagram"),
        m_i18n->t("editor", "dialog.newDocument.category.tiktok"),
        m_i18n->t("editor", "dialog.newDocument.category.custom"),
    };
    for (int i = 0; i < cats.size() && i < m_categoryTabs->count(); ++i)
        m_categoryTabs->setTabText(i, cats.at(i));

    // Orientação
    m_orientPortrait->setText(m_i18n->t("editor", "dialog.newDocument.orientation.portrait"));
    m_orientLandscape->setText(m_i18n->t("editor", "dialog.newDocument.orientation.landscape"));
}

} // namespace cc
