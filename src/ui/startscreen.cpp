#include "startscreen.h"
#include "localization/i18nservice.h"
#include "services/presetstore.h"
#include "services/recentfiles.h"
#include "core/serialization/ProjectFile.h"

#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QPixmap>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

namespace cc {

StartScreen::StartScreen(I18nService* i18n, PresetStore* presets,
                         RecentFiles* recent, QWidget* parent)
    : QWidget(parent)
    , m_i18n(i18n)
    , m_presets(presets)
    , m_recent(recent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(60, 40, 60, 40);
    layout->setSpacing(12);

    m_title = new QLabel(this);
    m_title->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_title->font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    m_title->setFont(titleFont);

    m_tagline = new QLabel(this);
    m_tagline->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_title);
    layout->addWidget(m_tagline);
    layout->addSpacing(16);

    m_createLabel = new QLabel(this);
    layout->addWidget(m_createLabel);

    auto* gridHost = new QWidget(this);
    auto* grid = new QGridLayout(gridHost);
    grid->setSpacing(8);
    layout->addWidget(gridHost);

    int row = 0, col = 0;
    if (m_presets) {
        for (const DocumentPreset& preset : m_presets->all()) {
            auto* card = new QPushButton(
                QStringLiteral("%1\n%2 x %3")
                    .arg(preset.name,
                         QString::number(preset.spec.width),
                         QString::number(preset.spec.height)),
                this);
            card->setMinimumSize(190, 64);
            const NewDocumentSpec spec = preset.spec;
            connect(card, &QPushButton::clicked, this,
                    [this, spec] { emit createRequested(spec); });
            grid->addWidget(card, row, col);
            if (++col == 3) { col = 0; ++row; }
        }
    }

    m_customButton = new QPushButton(this);
    m_customButton->setMinimumHeight(44);
    connect(m_customButton, &QPushButton::clicked,
            this, &StartScreen::customCreateRequested);
    layout->addWidget(m_customButton);

    m_openButton = new QPushButton(this);
    m_openButton->setMinimumHeight(36);
    connect(m_openButton, &QPushButton::clicked,
            this, &StartScreen::openRequested);
    layout->addWidget(m_openButton);

    layout->addSpacing(12);
    m_recentLabel = new QLabel(this);
    layout->addWidget(m_recentLabel);

    m_recentList = new QListWidget(this);
    m_recentList->setMaximumHeight(180);
    m_recentList->setIconSize(QSize(48, 48));
    connect(m_recentList, &QListWidget::itemActivated, this,
            [this](QListWidgetItem* item) {
                if (item)
                    emit recentActivated(item->data(Qt::UserRole).toString());
            });
    layout->addWidget(m_recentList);
    layout->addStretch();

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &StartScreen::retranslateUi);
    retranslateUi();
    populateRecents();
}

void StartScreen::populateRecents()
{
    m_recentList->clear();
    if (!m_recent)
        return;
    for (const QString& path : m_recent->paths()) {
        auto* item = new QListWidgetItem(QFileInfo(path).fileName(), m_recentList);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);

        const QImage thumb = loadProjectThumbnail(path);
        if (!thumb.isNull()) {
            item->setIcon(QIcon(QPixmap::fromImage(thumb)));
        }
    }
}

void StartScreen::refreshRecents()
{
    populateRecents();
}

void StartScreen::retranslateUi()
{
    if (!m_i18n)
        return;
    m_title->setText(m_i18n->t("common", "app.title"));
    m_tagline->setText(m_i18n->t("common", "app.tagline"));
    m_createLabel->setText(m_i18n->t("common", "start.createNew"));
    m_customButton->setText(m_i18n->t("common", "start.custom"));
    m_openButton->setText(m_i18n->t("common", "start.open"));
    m_recentLabel->setText(m_i18n->t("common", "start.recent"));
}

} // namespace cc
