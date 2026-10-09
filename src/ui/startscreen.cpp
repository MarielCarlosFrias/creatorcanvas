#include "startscreen.h"
#include "themeicons.h"
#include "localization/i18nservice.h"
#include "services/presetstore.h"
#include "services/recentfiles.h"
#include "core/serialization/ProjectFile.h"
#include "core/templates/TemplateFactory.h"

#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QPixmap>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QEvent>
#include <QFrame>
#include <QResizeEvent>

namespace cc {

StartScreen::StartScreen(I18nService* i18n, PresetStore* presets,
                         RecentFiles* recent, QWidget* parent)
    : QWidget(parent)
    , m_i18n(i18n)
    , m_presets(presets)
    , m_recent(recent)
{
    buildUi();

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &StartScreen::retranslateUi);
    retranslateUi();
    populateRecents();
    relayoutCards();
}

void StartScreen::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(60, 32, 60, 32);
    mainLayout->setSpacing(18);

    // --- 1. Top Header ---
    auto* headerWidget = new QWidget(this);
    auto* headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    m_title = new QLabel(this);
    m_title->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_title->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_title->setStyleSheet(QStringLiteral("color: #1f2937;"));

    m_tagline = new QLabel(this);
    m_tagline->setAlignment(Qt::AlignCenter);
    QFont tagFont = m_tagline->font();
    tagFont.setPointSize(12);
    m_tagline->setFont(tagFont);
    m_tagline->setStyleSheet(QStringLiteral("color: #6b7280;"));

    headerLayout->addWidget(m_title);
    headerLayout->addWidget(m_tagline);
    mainLayout->addWidget(headerWidget);

    // --- 2. Action Buttons (Hero Row) ---
    auto* actionRow = new QHBoxLayout;
    actionRow->setSpacing(12);
    actionRow->setAlignment(Qt::AlignCenter);

    m_createDesignBtn = new QPushButton(this);
    m_createDesignBtn->setIcon(ThemeIcons::actionNewDocument());
    m_createDesignBtn->setIconSize(QSize(20, 20));
    m_createDesignBtn->setMinimumHeight(44);
    m_createDesignBtn->setMinimumWidth(170);
    m_createDesignBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #2b78e4;"
        "  color: #ffffff;"
        "  font-weight: bold;"
        "  font-size: 13px;"
        "  border-radius: 6px;"
        "  padding: 8px 18px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #3b88f4;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #1a64ca;"
        "}"
    ));
    connect(m_createDesignBtn, &QPushButton::clicked,
            this, &StartScreen::customCreateRequested);

    const bool darkUi = palette().color(QPalette::Window).value() < 128;

    m_openProjectBtn = new QPushButton(this);
    m_openProjectBtn->setIcon(ThemeIcons::actionOpenFolder());
    m_openProjectBtn->setIconSize(QSize(20, 20));
    m_openProjectBtn->setMinimumHeight(44);
    m_openProjectBtn->setMinimumWidth(160);
    m_openProjectBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  font-weight: 600;"
        "  font-size: 13px;"
        "  border: 1px solid %3;"
        "  border-radius: 6px;"
        "  padding: 8px 18px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %4;"
        "  border-color: #2b78e4;"
        "}"
        "QPushButton:pressed {"
        "  background-color: %5;"
        "}"
    ).arg(darkUi ? "#262b35" : "#ffffff",
          darkUi ? "#e5e9f0" : "#1f2937",
          darkUi ? "#3b4252" : "#cbd5e1",
          darkUi ? "#313744" : "#eef2f7",
          darkUi ? "#1e222a" : "#e2e8f0"));
    connect(m_openProjectBtn, &QPushButton::clicked,
            this, &StartScreen::openRequested);

    m_customSizeBtn = new QPushButton(this);
    m_customSizeBtn->setMinimumHeight(44);
    m_customSizeBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: transparent;"
        "  color: %1;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  border: 1px dashed #4c566a;"
        "  border-radius: 6px;"
        "  padding: 8px 16px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %2;"
        "  border-color: %3;"
        "}"
    ).arg(darkUi ? "#5c9bff" : "#2563a8",
          darkUi ? "#262b35" : "#eef2f7",
          darkUi ? "#5c9bff" : "#94a3b8"));
    connect(m_customSizeBtn, &QPushButton::clicked,
            this, &StartScreen::customCreateRequested);

    actionRow->addWidget(m_createDesignBtn);
    actionRow->addWidget(m_openProjectBtn);
    actionRow->addWidget(m_customSizeBtn);
    mainLayout->addLayout(actionRow);

    mainLayout->addSpacing(8);

    // --- 3. Popular Visual Preset Cards ---
    m_popularLabel = new QLabel(this);
    QFont sectionFont = m_popularLabel->font();
    sectionFont.setPointSize(13);
    sectionFont.setBold(true);
    m_popularLabel->setFont(sectionFont);
    m_popularLabel->setStyleSheet(QStringLiteral("color: #e5e9f0;"));
    mainLayout->addWidget(m_popularLabel);

    m_cardsHost = new QWidget(this);
    m_cardsGrid = new QGridLayout(m_cardsHost);
    m_cardsGrid->setContentsMargins(0, 0, 0, 0);
    m_cardsGrid->setSpacing(12);

    // Platform Presets definition
    NewDocumentSpec ytSpec;
    ytSpec.width = 1280; ytSpec.height = 720; ytSpec.dpi = 96;
    ytSpec.backgroundColor = QColor(Qt::white);

    NewDocumentSpec instaSpec;
    instaSpec.width = 1080; instaSpec.height = 1080; instaSpec.dpi = 96;
    instaSpec.backgroundColor = QColor(Qt::white);

    NewDocumentSpec tiktokSpec;
    tiktokSpec.width = 1080; tiktokSpec.height = 1920; tiktokSpec.dpi = 96;
    tiktokSpec.backgroundColor = QColor(Qt::white);

    NewDocumentSpec bannerSpec;
    bannerSpec.width = 2560; bannerSpec.height = 1440; bannerSpec.dpi = 96;
    bannerSpec.backgroundColor = QColor(Qt::white);

    m_cardsGrid->addWidget(createVisualCard(QStringLiteral("YouTube Thumbnail"),
                                          ThemeIcons::brandYoutube(30),
                                          QStringLiteral("1280 × 720 px"),
                                          QStringLiteral("16:9"),
                                          ytSpec), 0, 0);

    m_cardsGrid->addWidget(createVisualCard(QStringLiteral("Instagram Post"),
                                          ThemeIcons::brandInstagram(30),
                                          QStringLiteral("1080 × 1080 px"),
                                          QStringLiteral("1:1"),
                                          instaSpec), 0, 1);

    m_cardsGrid->addWidget(createVisualCard(QStringLiteral("TikTok / Reels / Story"),
                                          ThemeIcons::brandTiktok(30),
                                          QStringLiteral("1080 × 1920 px"),
                                          QStringLiteral("9:16"),
                                          tiktokSpec), 0, 2);

    m_cardsGrid->addWidget(createVisualCard(QStringLiteral("YouTube Banner"),
                                          ThemeIcons::brandBanner(30),
                                          QStringLiteral("2560 × 1440 px"),
                                          QStringLiteral("16:9"),
                                          bannerSpec), 0, 3);

    mainLayout->addWidget(m_cardsHost);

    mainLayout->addSpacing(8);

    // --- 3b. Ready-Made Templates ---
    m_templatesLabel = new QLabel(this);
    m_templatesLabel->setFont(sectionFont);
    m_templatesLabel->setStyleSheet(QStringLiteral("color: #e5e9f0;"));
    mainLayout->addWidget(m_templatesLabel);

    m_templatesHost = new QWidget(this);
    m_templatesGrid = new QGridLayout(m_templatesHost);
    m_templatesGrid->setContentsMargins(0, 0, 0, 0);
    m_templatesGrid->setSpacing(12);

    const auto templates = TemplateFactory::availableTemplates();
    for (int i = 0; i < templates.size(); ++i) {
        const auto& tmpl = templates[i];
        auto* card = new QPushButton(m_templatesHost);
        card->setMinimumHeight(72);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        card->setCursor(Qt::PointingHandCursor);

        QString borderColor = QStringLiteral("#4c566a");
        QString accentColor = QStringLiteral("#88c0d0");
        QPixmap brandIcon;
        if (tmpl.platform == QStringLiteral("YouTube")) {
            borderColor = QStringLiteral("#cc0000");
            accentColor = QStringLiteral("#ff4444");
            brandIcon = ThemeIcons::brandYoutube(26);
        } else if (tmpl.platform == QStringLiteral("Instagram")) {
            borderColor = QStringLiteral("#c13584");
            accentColor = QStringLiteral("#e1306c");
            brandIcon = ThemeIcons::brandInstagram(26);
        } else if (tmpl.platform == QStringLiteral("TikTok")) {
            borderColor = QStringLiteral("#00f2ea");
            accentColor = QStringLiteral("#69c9d0");
            brandIcon = ThemeIcons::brandTiktok(26);
        } else {
            brandIcon = ThemeIcons::brandBanner(26);
        }

        card->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 8px;"
            "  padding: 8px;"
            "  text-align: left;"
            "}"
            "QPushButton:hover {"
            "  background-color: %3;"
            "  border-color: %4;"
            "}"
            "QPushButton:pressed {"
            "  background-color: %5;"
            "}"
        ).arg(darkUi ? "#232730" : "#ffffff",
              borderColor,
              darkUi ? "#2b303c" : "#eef2f7",
              accentColor,
              darkUi ? "#1e222a" : "#e2e8f0"));

        auto* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(8, 8, 8, 8);
        cardLayout->setSpacing(10);

        if (!brandIcon.isNull()) {
            auto* iconLabel = new QLabel(card);
            iconLabel->setPixmap(brandIcon);
            iconLabel->setFixedSize(brandIcon.size());
            iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
            iconLabel->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
            cardLayout->addWidget(iconLabel, 0, Qt::AlignTop);
        }

        auto* textContainer = new QVBoxLayout;
        textContainer->setContentsMargins(0, 0, 0, 0);
        textContainer->setSpacing(2);

        auto* platformLabel = new QLabel(tmpl.platform, card);
        platformLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        platformLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; font-weight: bold; border: none; background: transparent;"
        ).arg(accentColor));

        auto* titleLabel = new QLabel(tmpl.defaultTitle, card);
        titleLabel->setWordWrap(true);
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        titleLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; font-weight: 600; border: none; background: transparent;"
        ).arg(darkUi ? "#e5e9f0" : "#1f2937"));

        auto* dimLabel = new QLabel(QStringLiteral("%1 × %2 px").arg(tmpl.width).arg(tmpl.height), card);
        dimLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        dimLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 9px; border: none; background: transparent;"
        ).arg(darkUi ? "#8c97a8" : "#6b7280"));

        textContainer->addWidget(platformLabel);
        textContainer->addWidget(titleLabel);
        textContainer->addStretch();
        textContainer->addWidget(dimLabel);

        cardLayout->addLayout(textContainer, 1);

        const int kindInt = static_cast<int>(tmpl.kind);
        connect(card, &QPushButton::clicked, this, [this, kindInt] {
            emit templateRequested(kindInt);
        });

        m_templatesGrid->addWidget(card, i / 4, i % 4);
    }

    mainLayout->addWidget(m_templatesHost);

    mainLayout->addSpacing(8);

    // --- 4. Recent Projects Section ---
    m_recentLabel = new QLabel(this);
    m_recentLabel->setFont(sectionFont);
    m_recentLabel->setStyleSheet(QStringLiteral("color: #e5e9f0;"));
    mainLayout->addWidget(m_recentLabel);

    m_recentStack = new QStackedWidget(this);
    m_recentStack->setMinimumHeight(110);
    m_recentStack->setMaximumHeight(180);

    // Page 0: List of recent files
    m_recentList = new QListWidget(this);
    m_recentList->setIconSize(QSize(54, 40));
    m_recentList->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "  padding: 6px;"
        "}"
        "QListWidget::item {"
        "  padding: 8px 12px;"
        "  border-radius: 6px;"
        "  color: %3;"
        "  margin-bottom: 2px;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: %4;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: %5;"
        "  color: %6;"
        "}"
    ).arg(darkUi ? "#22262e" : "#ffffff",
          darkUi ? "#333a46" : "#cbd5e1",
          darkUi ? "#e5e9f0" : "#1f2937",
          darkUi ? "#2e3440" : "#eef2f7",
          darkUi ? "#3b4252" : "#dbeafe",
          darkUi ? "#ffffff" : "#1f2937"));
    connect(m_recentList, &QListWidget::itemActivated, this,
            [this](QListWidgetItem* item) {
                if (item)
                    emit recentActivated(item->data(Qt::UserRole).toString());
            });
    m_recentStack->addWidget(m_recentList);

    // Page 1: Empty state widget
    m_emptyStateWidget = new QWidget(this);
    m_emptyStateWidget->setStyleSheet(QStringLiteral(
        "QWidget {"
        "  background-color: %1;"
        "  border: 1px dashed %2;"
        "  border-radius: 8px;"
        "}"
    ).arg(darkUi ? "#22262e" : "#ffffff",
          darkUi ? "#353c48" : "#cbd5e1"));
    auto* emptyLayout = new QVBoxLayout(m_emptyStateWidget);
    emptyLayout->setContentsMargins(20, 24, 20, 24);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(6);

    m_emptyStateIcon = new QLabel(m_emptyStateWidget);
    m_emptyStateIcon->setPixmap(ThemeIcons::emptyProjectsPlaceholder(64, 64));
    m_emptyStateIcon->setAlignment(Qt::AlignCenter);
    m_emptyStateIcon->setStyleSheet(QStringLiteral("border: none; background: transparent;"));

    m_emptyStateTitle = new QLabel(m_emptyStateWidget);
    m_emptyStateTitle->setAlignment(Qt::AlignCenter);
    QFont emptyTitleFont = m_emptyStateTitle->font();
    emptyTitleFont.setPointSize(12);
    emptyTitleFont.setBold(true);
    m_emptyStateTitle->setFont(emptyTitleFont);
    m_emptyStateTitle->setStyleSheet(QStringLiteral("color: %1; border: none; background: transparent;")
        .arg(darkUi ? "#abb2bf" : "#374151"));

    m_emptyStateSubtitle = new QLabel(m_emptyStateWidget);
    m_emptyStateSubtitle->setAlignment(Qt::AlignCenter);
    m_emptyStateSubtitle->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; border: none; background: transparent;")
        .arg(darkUi ? "#636d83" : "#6b7280"));

    emptyLayout->addWidget(m_emptyStateIcon);
    emptyLayout->addWidget(m_emptyStateTitle);
    emptyLayout->addWidget(m_emptyStateSubtitle);

    m_recentStack->addWidget(m_emptyStateWidget);
    mainLayout->addWidget(m_recentStack);

    mainLayout->addStretch();
}

QWidget* StartScreen::createVisualCard(const QString& brandName, const QPixmap& icon,
                                      const QString& dimensions, const QString& ratio,
                                      const NewDocumentSpec& spec)
{
    const bool darkUi = palette().color(QPalette::Window).value() < 128;
    auto* card = new QPushButton(this);
    card->setMinimumSize(180, 84);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    card->setCursor(Qt::PointingHandCursor);
    card->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "  padding: 10px;"
        "  text-align: left;"
        "}"
        "QPushButton:hover {"
        "  background-color: %3;"
        "  border: 1px solid #2b78e4;"
        "}"
        "QPushButton:pressed {"
        "  background-color: %4;"
        "}"
    ).arg(darkUi ? "#232730" : "#ffffff",
          darkUi ? "#333945" : "#cbd5e1",
          darkUi ? "#2b303c" : "#eef2f7",
          darkUi ? "#1e222a" : "#e2e8f0"));

    auto* cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(10, 8, 10, 8);
    cardLayout->setSpacing(12);

    auto* iconLabel = new QLabel(card);
    iconLabel->setPixmap(icon);
    iconLabel->setFixedSize(icon.size());
    iconLabel->setStyleSheet(QStringLiteral("background: transparent; border: none;"));

    auto* textContainer = new QVBoxLayout;
    textContainer->setContentsMargins(0, 0, 0, 0);
    textContainer->setSpacing(2);

    auto* nameLabel = new QLabel(brandName, card);
    QFont nf = nameLabel->font();
    nf.setPointSize(11);
    nf.setBold(true);
    nameLabel->setFont(nf);
    nameLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent; border: none;")
        .arg(darkUi ? "#ffffff" : "#1f2937"));

    auto* dimLabel = new QLabel(QStringLiteral("%1  (%2)").arg(dimensions, ratio), card);
    dimLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; background: transparent; border: none;")
        .arg(darkUi ? "#8c97a8" : "#6b7280"));

    textContainer->addWidget(nameLabel);
    textContainer->addWidget(dimLabel);

    cardLayout->addWidget(iconLabel);
    cardLayout->addLayout(textContainer);
    cardLayout->addStretch();

    connect(card, &QPushButton::clicked, this, [this, spec] {
        emit createRequested(spec);
    });

    return card;
}

void StartScreen::populateRecents()
{
    m_recentList->clear();
    const QStringList paths = m_recent ? m_recent->paths() : QStringList();

    if (paths.isEmpty()) {
        m_recentStack->setCurrentWidget(m_emptyStateWidget);
        return;
    }

    m_recentStack->setCurrentWidget(m_recentList);

    for (const QString& path : paths) {
        auto* item = new QListWidgetItem(m_recentList);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);

        const QFileInfo info(path);
        item->setText(QStringLiteral("%1\n%2")
                          .arg(info.fileName(), info.absolutePath()));

        const QImage thumb = loadProjectThumbnail(path);
        if (!thumb.isNull()) {
            item->setIcon(QIcon(QPixmap::fromImage(thumb)));
        } else {
            item->setIcon(ThemeIcons::actionNewDocument());
        }
    }
}

void StartScreen::refreshRecents()
{
    populateRecents();
    relayoutCards();
}

void StartScreen::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // Keep useful margins on large windows without wasting most of a small
    // laptop display on empty space.
    const int horizontalMargin = qBound(16, width() / 20, 60);
    if (auto* mainLayout = qobject_cast<QVBoxLayout*>(layout()))
        mainLayout->setContentsMargins(horizontalMargin, 20,
                                       horizontalMargin, 20);
    relayoutCards();
}

void StartScreen::relayoutCards()
{
    if (!m_cardsGrid || !m_templatesGrid)
        return;

    // Four columns on wide screens, two on medium screens, and one on narrow
    // windows.  Widgets are reused, so resizing does not rebuild the screen
    // or lose signal connections.
    const int columns = width() < 620 ? 1 : (width() < 980 ? 2 : 4);

    auto relayout = [columns](QGridLayout* grid) {
        QVector<QWidget*> widgets;
        for (int i = 0; i < grid->count(); ++i) {
            if (auto* widget = grid->itemAt(i)->widget())
                widgets.append(widget);
        }
        for (int i = 0; i < widgets.size(); ++i)
            grid->addWidget(widgets.at(i), i / columns, i % columns);
        for (int col = 0; col < columns; ++col)
            grid->setColumnStretch(col, 1);
    };

    relayout(m_cardsGrid);
    relayout(m_templatesGrid);
}

void StartScreen::retranslateUi()
{
    if (!m_i18n)
        return;

    m_title->setText(m_i18n->t("common", "app.title"));
    const bool darkUi = palette().color(QPalette::Window).value() < 128;
    m_title->setStyleSheet(QStringLiteral("color: %1;").arg(darkUi ? "#f3f4f6" : "#1f2937"));
    m_tagline->setText(m_i18n->t("common", "app.tagline"));
    m_tagline->setStyleSheet(QStringLiteral("color: %1;").arg(darkUi ? "#b0b5bf" : "#6b7280"));

    m_createDesignBtn->setText(m_i18n->t("common", "start.createDesign"));
    m_openProjectBtn->setText(m_i18n->t("common", "start.openProject"));
    m_customSizeBtn->setText(m_i18n->t("common", "start.custom"));

    m_popularLabel->setText(m_i18n->t("common", "start.popular"));
    m_popularLabel->setStyleSheet(QStringLiteral("color: %1;").arg(darkUi ? "#f3f4f6" : "#1f2937"));
    if (m_templatesLabel)
        m_templatesLabel->setText(m_i18n->t("common", "start.templates"));
    if (m_templatesLabel)
        m_templatesLabel->setStyleSheet(QStringLiteral("color: %1;").arg(darkUi ? "#f3f4f6" : "#1f2937"));
    m_recentLabel->setText(m_i18n->t("common", "start.recent"));
    m_recentLabel->setStyleSheet(QStringLiteral("color: %1;").arg(darkUi ? "#f3f4f6" : "#1f2937"));

    m_emptyStateTitle->setText(m_i18n->t("common", "start.emptyRecents"));
    m_emptyStateTitle->setStyleSheet(QStringLiteral("color: %1; border: none; background: transparent;").arg(darkUi ? "#f3f4f6" : "#374151"));
    m_emptyStateSubtitle->setText(m_i18n->t("common", "start.emptyRecentsHint"));
    m_emptyStateSubtitle->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; border: none; background: transparent;").arg(darkUi ? "#b0b5bf" : "#6b7280"));
}

bool StartScreen::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QWidget* card = qobject_cast<QWidget*>(watched);
        if (card) {
            bool ok = false;
            int kind = card->property("_templateKind").toInt(&ok);
            if (ok) {
                emit templateRequested(kind);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace cc
