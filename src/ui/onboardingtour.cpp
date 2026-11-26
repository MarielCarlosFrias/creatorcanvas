#include "ui/onboardingtour.h"

#include "localization/i18nservice.h"
#include "services/settingsservice.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace cc {

namespace {
constexpr int kTourSteps = 4;
}

OnboardingTour::OnboardingTour(I18nService* i18n, SettingsService* settings,
                               QWidget* host, QObject* parent)
    : QObject(parent)
    , m_i18n(i18n)
    , m_settings(settings)
    , m_host(host)
{
}

OnboardingTour::~OnboardingTour()
{
    delete m_popup;
    m_popup = nullptr;
}

void OnboardingTour::setTargets(const QVector<QWidget*>& targets)
{
    m_targets = targets;
}

void OnboardingTour::start()
{
    if (!m_host)
        return;
    buildPopup();
    m_running = true;
    showStep(0);
}

void OnboardingTour::buildPopup()
{
    if (m_popup)
        return;

    m_popup = new QWidget(m_host, Qt::Tool | Qt::FramelessWindowHint);
    m_popup->setObjectName(QStringLiteral("TourPopup"));
    auto* layout = new QVBoxLayout(m_popup);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);

    m_stepLabel = new QLabel(m_popup);
    m_stepLabel->setObjectName(QStringLiteral("TourStep"));
    layout->addWidget(m_stepLabel);

    m_textLabel = new QLabel(m_popup);
    m_textLabel->setObjectName(QStringLiteral("TourText"));
    m_textLabel->setWordWrap(true);
    layout->addWidget(m_textLabel);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    m_nextButton = new QPushButton(m_popup);
    m_nextButton->setDefault(true);
    buttons->addWidget(m_nextButton);
    m_skipButton = new QPushButton(m_popup);
    m_skipButton->setAutoDefault(false);
    buttons->addWidget(m_skipButton);
    layout->addLayout(buttons);

    connect(m_nextButton, &QPushButton::clicked, this, [this] {
        if (m_index + 1 >= kTourSteps) {
            finish(true);
            return;
        }
        showStep(m_index + 1);
    });
    connect(m_skipButton, &QPushButton::clicked, this, [this] { finish(true); });
}

void OnboardingTour::showStep(int index)
{
    if (!m_popup)
        return;
    m_index = qBound(0, index, kTourSteps - 1);

    static const char* const kKeys[kTourSteps] = {
        "tour.tools", "tour.options", "tour.layers", "tour.export",
    };

    if (m_i18n) {
        m_stepLabel->setText(m_i18n->t("editor", "tour.step")
                                 .arg(m_index + 1).arg(kTourSteps));
        m_textLabel->setText(m_i18n->t("editor", kKeys[m_index]));
        m_nextButton->setText(m_i18n->t(
            "editor", m_index + 1 >= kTourSteps ? "tour.finish" : "tour.next"));
        m_skipButton->setText(m_i18n->t("editor", "tour.skip"));
    }

    QWidget* target = (m_index < m_targets.size()) ? m_targets.at(m_index) : nullptr;
    positionPopup(target);
    m_popup->show();
    m_popup->raise();
}

void OnboardingTour::positionPopup(QWidget* target)
{
    if (!m_popup || !m_host)
        return;

    const QSize popupSize = m_popup->sizeHint();
    m_popup->resize(qMax(280, popupSize.width()), popupSize.height());

    const QRect hostRect = m_host->rect();
    QPoint topLeft;
    if (target && target->isVisible()) {
        const QPoint targetTopLeft = target->mapTo(m_host, QPoint(0, 0));
        const int x = qBound(8, targetTopLeft.x(), hostRect.width() - m_popup->width() - 8);
        const int below = targetTopLeft.y() + target->height() + 8;
        const int fitsBelow = below + m_popup->height() + 8 <= hostRect.height();
        const int y = fitsBelow ? below
                                : qMax(8, targetTopLeft.y() - m_popup->height() - 8);
        topLeft = QPoint(x, y);
    } else {
        // Sem alvo visível: centra na janela.
        topLeft = QPoint((hostRect.width() - m_popup->width()) / 2,
                         (hostRect.height() - m_popup->height()) / 2);
    }
    m_popup->move(m_host->mapToGlobal(topLeft));
}

void OnboardingTour::finish(bool completed)
{
    if (m_popup)
        m_popup->hide();
    m_running = false;
    if (completed && m_settings)
        m_settings->setTourCompleted(true);
}

} // namespace cc
