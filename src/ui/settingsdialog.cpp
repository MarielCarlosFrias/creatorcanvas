#include "settingsdialog.h"

#include "localization/i18nservice.h"
#include "services/settingsservice.h"
#include "ui/darktheme.h"

#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace cc {

SettingsDialog::SettingsDialog(I18nService* i18n, SettingsService* settings,
                               QWidget* parent)
    : QDialog(parent)
    , m_i18n(i18n)
    , m_settings(settings)
{
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    m_language = new QComboBox(this);
    if (m_i18n) {
        const QStringList codes = m_i18n->availableLanguages();
        for (const QString& code : codes)
            m_language->addItem(m_i18n->displayName(code), code);
        const int index = codes.indexOf(m_i18n->currentLanguage());
        if (index >= 0)
            m_language->setCurrentIndex(index);
    }
    m_languageLabel = new QLabel(this);
    form->addRow(m_languageLabel, m_language);

    m_autosave = new QComboBox(this);
    m_autosave->addItem(QStringLiteral("0"), 0);
    m_autosave->addItem(QStringLiteral("1"), 1);
    m_autosave->addItem(QStringLiteral("5"), 5);
    m_autosave->addItem(QStringLiteral("10"), 10);
    const int interval = m_settings ? m_settings->autosaveIntervalMinutes() : 5;
    const int index = m_autosave->findData(interval);
    if (index >= 0)
        m_autosave->setCurrentIndex(index);
    m_autosaveLabel = new QLabel(this);
    form->addRow(m_autosaveLabel, m_autosave);

    m_gpuAcceleration = new QCheckBox(this);
    m_gpuAcceleration->setChecked(m_settings && m_settings->gpuAccelerationEnabled());
    m_gpuAccelerationLabel = new QLabel(this);
    form->addRow(m_gpuAccelerationLabel, m_gpuAcceleration);

    m_restartHintLabel = new QLabel(this);
    m_restartHintLabel->setWordWrap(true);
    m_restartHintLabel->setStyleSheet(QStringLiteral("color: #6b7280; font-size: 11px;"));
    form->addRow(QString(), m_restartHintLabel);

    m_darkMode = new QCheckBox(this);
    m_darkMode->setChecked(m_settings && m_settings->darkModeEnabled());
    m_appearanceLabel = new QLabel(this);
    form->addRow(m_appearanceLabel, m_darkMode);

    m_confirmExit = new QCheckBox(this);
    m_confirmExit->setChecked(!m_settings || m_settings->confirmBeforeExit());
    form->addRow(QString(), m_confirmExit);

    m_showGrid = new QCheckBox(this);
    m_showGrid->setChecked(m_settings && m_settings->showGridByDefault());
    m_canvasLabel = new QLabel(this);
    form->addRow(m_canvasLabel, m_showGrid);

    m_snapToGrid = new QCheckBox(this);
    m_snapToGrid->setChecked(m_settings && m_settings->snapToGridByDefault());
    form->addRow(QString(), m_snapToGrid);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &SettingsDialog::retranslateUi);
    retranslateUi();
}

void SettingsDialog::retranslateUi()
{
    if (!m_i18n)
        return;
    setWindowTitle(m_i18n->t("settings", "dialog.title"));
    m_languageLabel->setText(m_i18n->t("settings", "dialog.language"));
    m_autosaveLabel->setText(m_i18n->t("settings", "dialog.autosave"));
    if (m_gpuAccelerationLabel)
        m_gpuAccelerationLabel->setText(m_i18n->t("settings", "dialog.gpuAcceleration"));
    if (m_restartHintLabel)
        m_restartHintLabel->setText(m_i18n->t("settings", "dialog.gpuAccelerationHint"));
    if (m_appearanceLabel)
        m_appearanceLabel->setText(m_i18n->t("settings", "dialog.appearance"));
    if (m_darkMode)
        m_darkMode->setText(m_i18n->t("settings", "dialog.darkMode"));
    if (m_confirmExit)
        m_confirmExit->setText(m_i18n->t("settings", "dialog.confirmExit"));
    if (m_canvasLabel)
        m_canvasLabel->setText(m_i18n->t("settings", "dialog.canvas"));
    if (m_showGrid)
        m_showGrid->setText(m_i18n->t("settings", "dialog.showGrid"));
    if (m_snapToGrid)
        m_snapToGrid->setText(m_i18n->t("settings", "dialog.snapToGrid"));
}

void SettingsDialog::accept()
{
    if (m_i18n) {
        const QString code = m_language->currentData().toString();
        if (!code.isEmpty())
            m_i18n->setLanguage(code);
    }
    if (m_settings) {
        bool ok = false;
        const int minutes = m_autosave->currentData().toInt(&ok);
        if (ok)
            m_settings->setAutosaveIntervalMinutes(minutes);
        m_settings->setGpuAccelerationEnabled(m_gpuAcceleration->isChecked());
        m_settings->setDarkModeEnabled(m_darkMode->isChecked());
        m_settings->setConfirmBeforeExit(m_confirmExit->isChecked());
        m_settings->setShowGridByDefault(m_showGrid->isChecked());
        m_settings->setSnapToGridByDefault(m_snapToGrid->isChecked());
    }
    if (qApp)
        applyTheme(*qApp, m_darkMode->isChecked());
    QDialog::accept();
}

} // namespace cc
