#include "ui/welcomedialog.h"

#include "localization/i18nservice.h"
#include "services/settingsservice.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDialogButtonBox>

namespace cc {

WelcomeDialog::WelcomeDialog(I18nService* i18n, SettingsService* settings,
                             QWidget* parent)
    : QDialog(parent)
    , m_i18n(i18n)
    , m_settings(settings)
{
    setObjectName(QStringLiteral("WelcomeDialog"));
    setWindowTitle(QStringLiteral("CreatorCanvas"));
    setMinimumSize(460, 320);
    resize(520, 380);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("WelcomeTitle"));
    layout->addWidget(m_titleLabel);

    m_introLabel = new QLabel(this);
    m_introLabel->setWordWrap(true);
    layout->addWidget(m_introLabel);

    m_bulletsLabel = new QLabel(this);
    m_bulletsLabel->setWordWrap(true);
    m_bulletsLabel->setObjectName(QStringLiteral("HintLabel"));
    layout->addWidget(m_bulletsLabel);

    layout->addStretch();

    auto* form = new QHBoxLayout();
    auto* langCaption = new QLabel(this);
    langCaption->setObjectName(QStringLiteral("WelcomeLangCaption"));
    form->addWidget(langCaption);
    m_language = new QComboBox(this);
    if (m_i18n) {
        const QStringList codes = m_i18n->availableLanguages();
        for (const QString& code : codes)
            m_language->addItem(m_i18n->displayName(code), code);
        const int index = codes.indexOf(m_i18n->currentLanguage());
        if (index >= 0)
            m_language->setCurrentIndex(index);
    }
    form->addWidget(m_language, 1);
    layout->addLayout(form);
    // A legenda do idioma precisa de existir para o retranslate; guardada no
    // layout através do objeto irmão do combo.
    langCaption->setObjectName(QStringLiteral("WelcomeLangCaption"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    buttons->button(QDialogButtonBox::Ok)->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &WelcomeDialog::retranslateUi);
    retranslateUi();
}

void WelcomeDialog::retranslateUi()
{
    if (!m_i18n)
        return;
    setWindowTitle(m_i18n->t("editor", "welcome.title"));
    if (m_titleLabel)
        m_titleLabel->setText(m_i18n->t("editor", "welcome.heading"));
    if (m_introLabel)
        m_introLabel->setText(m_i18n->t("editor", "welcome.intro"));
    if (m_bulletsLabel)
        m_bulletsLabel->setText(m_i18n->t("editor", "welcome.bullets"));
    if (auto* caption = findChild<QLabel*>(QStringLiteral("WelcomeLangCaption")))
        caption->setText(m_i18n->t("editor", "welcome.language"));
}

void WelcomeDialog::accept()
{
    // O idioma escolhido aplica-se a TODO o programa (MainWindow ainda não
    // construiu os seus rótulos quando este diálogo corre).
    if (m_i18n && m_language) {
        const QString code = m_language->currentData().toString();
        if (!code.isEmpty())
            m_i18n->setLanguage(code);
    }
    if (m_settings)
        m_settings->setWelcomeShown(true);
    QDialog::accept();
}

} // namespace cc
