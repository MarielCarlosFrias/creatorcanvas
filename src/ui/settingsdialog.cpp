#include "settingsdialog.h"

#include "localization/i18nservice.h"
#include "services/settingsservice.h"

#include <QComboBox>
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
    }
    QDialog::accept();
}

} // namespace cc
