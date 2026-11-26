#include "ui/onboardingpanel.h"

#include "localization/i18nservice.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace cc {

OnboardingPanel::OnboardingPanel(I18nService* i18n, QWidget* parent)
    : QWidget(parent)
    , m_i18n(i18n)
{
    setObjectName(QStringLiteral("OnboardingPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("SectionLabel"));
    layout->addWidget(m_titleLabel);

    m_progressLabel = new QLabel(this);
    m_progressLabel->setObjectName(QStringLiteral("HintLabel"));
    layout->addWidget(m_progressLabel);

    for (int i = 0; i < kStepCount; ++i) {
        auto* row = new QLabel(this);
        row->setWordWrap(true);
        m_stepLabels.push_back(row);
        layout->addWidget(row);
    }

    layout->addStretch();

    // Sem este botão o painel não tinha saída: fechar a dock não persistia e
    // ele voltava a aparecer a cada updateOnboarding().
    m_dismissButton = new QPushButton(this);
    m_dismissButton->setObjectName(QStringLiteral("LinkButton"));
    m_dismissButton->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_dismissButton);
    connect(m_dismissButton, &QPushButton::clicked,
            this, &OnboardingPanel::dismissed);

    retranslateUi();
}

void OnboardingPanel::setSteps(const QVector<bool>& done)
{
    m_done = done;
    while (m_done.size() < kStepCount)
        m_done.push_back(false);
    m_done.resize(kStepCount);

    int completed = 0;
    for (int i = 0; i < m_stepLabels.size(); ++i) {
        const bool isDone = m_done.at(i);
        if (isDone)
            ++completed;
        const QString text = m_i18n
            ? m_i18n->t("editor", QStringLiteral("onboarding.step%1").arg(i + 1))
            : QString();
        m_stepLabels[i]->setText((isDone ? QString::fromUtf8("\u2714  ")
                                         : QString::fromUtf8("\u25CB  "))
                                 + text);
    }
    m_allDone = (completed == m_stepLabels.size());

    if (m_dismissButton && m_i18n)
        m_dismissButton->setText(m_i18n->t("editor", "onboarding.dismiss"));

    if (m_i18n && m_progressLabel)
        m_progressLabel->setText(m_i18n->t("editor", "onboarding.progress")
                                     .arg(completed).arg(m_stepLabels.size()));
    if (m_allDone)
        emit dismissed();
}

void OnboardingPanel::retranslateUi()
{
    if (!m_i18n)
        return;
    if (m_titleLabel)
        m_titleLabel->setText(m_i18n->t("editor", "onboarding.title"));
    // Os rótulos dos passos são reescritos por setSteps (carregam o estado).
    setSteps(m_done);
}

} // namespace cc
