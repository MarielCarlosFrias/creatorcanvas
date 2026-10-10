#include "recoverydialog.h"

#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "localization/i18nservice.h"
#include "ui/themeicons.h"

namespace cc {

RecoveryDialog::RecoveryDialog(I18nService* i18n, const QString& recoveryFilePath,
                               QWidget* parent)
    : QDialog(parent)
    , m_i18n(i18n)
{
    setWindowTitle(m_i18n ? m_i18n->t("common", "dialog.recovery.title")
                          : QStringLiteral("Recuperação Automática de Projeto"));
    setWindowIcon(ThemeIcons::appIcon());
    setMinimumWidth(440);
    buildUi(recoveryFilePath);
}

void RecoveryDialog::buildUi(const QString& recoveryFilePath)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    auto* headerLayout = new QHBoxLayout;
    auto* iconLabel = new QLabel(this);
    iconLabel->setPixmap(ThemeIcons::actionOpenFolder().pixmap(48, 48));
    headerLayout->addWidget(iconLabel);

    auto* titleLayout = new QVBoxLayout;
    auto* titleLabel = new QLabel(this);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    titleLabel->setText(m_i18n ? m_i18n->t("common", "dialog.recovery.heading")
                               : QStringLiteral("Sessão anterior não salva foi encontrada"));

    auto* descLabel = new QLabel(this);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(QStringLiteral("color: #888;"));
    descLabel->setText(m_i18n ? m_i18n->t("common", "dialog.recovery.desc")
                              : QStringLiteral("O CreatorCanvas recuperou automaticamente as alterações que você estava fazendo antes do encerramento inesperado."));
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(descLabel);
    headerLayout->addLayout(titleLayout);
    mainLayout->addLayout(headerLayout);

    // Detalhes do arquivo recuperado
    QFileInfo fi(recoveryFilePath);
    auto* detailsBox = new QWidget(this);
    detailsBox->setStyleSheet(QStringLiteral("background: rgba(255, 255, 255, 0.04); border-radius: 6px; padding: 10px;"));
    auto* detailsLayout = new QVBoxLayout(detailsBox);
    detailsLayout->setSpacing(6);

    const QString timeStr = fi.lastModified().toString(QStringLiteral("dd/MM/yyyy HH:mm:ss"));
    const QString sizeStr = QString::number(fi.size() / 1024.0, 'f', 1) + QStringLiteral(" KB");

    auto* timeLabel = new QLabel(QStringLiteral("<b>Data/Hora:</b> %1").arg(timeStr), detailsBox);
    auto* sizeLabel = new QLabel(QStringLiteral("<b>Tamanho:</b> %1").arg(sizeStr), detailsBox);
    detailsLayout->addWidget(timeLabel);
    detailsLayout->addWidget(sizeLabel);
    mainLayout->addWidget(detailsBox);

    // Botões de ação
    auto* buttonsLayout = new QHBoxLayout;

    auto* discardBtn = new QPushButton(m_i18n ? m_i18n->t("common", "dialog.recovery.discard")
                                              : QStringLiteral("Descartar"), this);
    discardBtn->setStyleSheet(QStringLiteral("color: #e55;"));
    connect(discardBtn, &QPushButton::clicked, this, [this] {
        m_choice = Choice::Discard;
        reject();
    });

    auto* copyBtn = new QPushButton(m_i18n ? m_i18n->t("common", "dialog.recovery.openAsCopy")
                                           : QStringLiteral("Abrir como Cópia"), this);
    connect(copyBtn, &QPushButton::clicked, this, [this] {
        m_choice = Choice::OpenAsCopy;
        accept();
    });

    auto* recoverBtn = new QPushButton(m_i18n ? m_i18n->t("common", "dialog.recovery.openRecovery")
                                              : QStringLiteral("Abrir Recuperação"), this);
    recoverBtn->setDefault(true);
    recoverBtn->setStyleSheet(QStringLiteral("font-weight: bold;"));
    connect(recoverBtn, &QPushButton::clicked, this, [this] {
        m_choice = Choice::OpenRecovery;
        accept();
    });

    buttonsLayout->addWidget(discardBtn);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(copyBtn);
    buttonsLayout->addWidget(recoverBtn);

    mainLayout->addLayout(buttonsLayout);
}

} // namespace cc
