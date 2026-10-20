#include "startscreen.h"
#include "themeicons.h"
#include "designtokens.h"
#include "localization/i18nservice.h"
#include "services/presetstore.h"
#include "services/recentfiles.h"
#include "core/serialization/ProjectFile.h"
#include "core/templates/TemplateFactory.h"
#include "core/Document.h"
#include "rendering/CanvasRenderer.h"

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
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QPainter>
#include <QHash>

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
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto* scrollArea = new QScrollArea(this);
    m_scrollArea = scrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setObjectName(QStringLiteral("StartScrollArea"));

    auto* content = new QWidget;
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto* mainLayout = new QVBoxLayout(content);
    m_contentLayout = mainLayout;
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
    m_title->setObjectName(QStringLiteral("HeroTitle"));

    m_tagline = new QLabel(this);
    m_tagline->setAlignment(Qt::AlignCenter);
    QFont tagFont = m_tagline->font();
    tagFont.setPointSize(12);
    m_tagline->setFont(tagFont);
    m_tagline->setObjectName(QStringLiteral("HeroTagline"));

    headerLayout->addWidget(m_title);
    headerLayout->addWidget(m_tagline);
    mainLayout->addWidget(headerWidget);

    // --- 2. Action Buttons (Hero Row) ---
    m_actionRow = new QHBoxLayout;
    m_actionRow->setSpacing(12);
    m_actionRow->setAlignment(Qt::AlignCenter);

    m_createDesignBtn = new QPushButton(this);
    m_createDesignBtn->setIcon(ThemeIcons::actionNewDocument());
    m_createDesignBtn->setIconSize(QSize(20, 20));
    m_createDesignBtn->setMinimumHeight(44);
    m_createDesignBtn->setMinimumWidth(170);
    m_createDesignBtn->setObjectName(QStringLiteral("PrimaryButton"));
    connect(m_createDesignBtn, &QPushButton::clicked,
            this, &StartScreen::customCreateRequested);

    m_openProjectBtn = new QPushButton(this);
    m_openProjectBtn->setIcon(ThemeIcons::actionOpenFolder());
    m_openProjectBtn->setIconSize(QSize(20, 20));
    m_openProjectBtn->setMinimumHeight(44);
    m_openProjectBtn->setMinimumWidth(160);
    m_openProjectBtn->setObjectName(QStringLiteral("SecondaryButton"));
    connect(m_openProjectBtn, &QPushButton::clicked,
            this, &StartScreen::openRequested);

    m_customSizeBtn = new QPushButton(this);
    m_customSizeBtn->setMinimumHeight(44);
    m_customSizeBtn->setObjectName(QStringLiteral("GhostButton"));
    connect(m_customSizeBtn, &QPushButton::clicked,
            this, &StartScreen::customCreateRequested);

    m_actionRow->addWidget(m_createDesignBtn);
    m_actionRow->addWidget(m_openProjectBtn);
    m_actionRow->addWidget(m_customSizeBtn);
    mainLayout->addLayout(m_actionRow);

    mainLayout->addSpacing(8);

    // --- 3. Popular Visual Preset Cards ---
    m_popularLabel = new QLabel(this);
    QFont sectionFont = m_popularLabel->font();
    sectionFont.setPointSize(13);
    sectionFont.setBold(true);
    m_popularLabel->setFont(sectionFont);
    m_popularLabel->setObjectName(QStringLiteral("SectionLabel"));
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
    m_templatesLabel->setObjectName(QStringLiteral("SectionLabel"));
    mainLayout->addWidget(m_templatesLabel);

    m_templatesHost = new QWidget(this);
    m_templatesGrid = new QGridLayout(m_templatesHost);
    m_templatesGrid->setContentsMargins(0, 0, 0, 0);
    m_templatesGrid->setSpacing(12);

    const auto templates = TemplateFactory::availableTemplates();
    for (int i = 0; i < templates.size(); ++i) {
        const auto& tmpl = templates[i];
        auto* card = new QPushButton(m_templatesHost);
        card->setMinimumHeight(178);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        card->setCursor(Qt::PointingHandCursor);

        QString accentColor = QStringLiteral("#88c0d0");
        QPixmap brandIcon;
        if (tmpl.platform == QStringLiteral("YouTube")) {
            accentColor = QStringLiteral("#ff4444");
            brandIcon = ThemeIcons::brandYoutube(26);
        } else if (tmpl.platform == QStringLiteral("Instagram")) {
            accentColor = QStringLiteral("#e1306c");
            brandIcon = ThemeIcons::brandInstagram(26);
        } else if (tmpl.platform == QStringLiteral("TikTok")) {
            accentColor = QStringLiteral("#69c9d0");
            brandIcon = ThemeIcons::brandTiktok(26);
        } else {
            brandIcon = ThemeIcons::brandBanner(26);
        }

        card->setObjectName(QStringLiteral("TemplateCard"));

        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(8, 8, 8, 8);
        cardLayout->setSpacing(5);

        const auto preview = templatePreview(tmpl.kind);
        auto* previewLabel = new QLabel(card);
        previewLabel->setPixmap(preview);
        previewLabel->setMinimumHeight(104);
        previewLabel->setAlignment(Qt::AlignCenter);
        previewLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        previewLabel->setObjectName(QStringLiteral("TemplatePreview"));
        cardLayout->addWidget(previewLabel, 1);

        auto* textContainer = new QVBoxLayout;
        textContainer->setContentsMargins(0, 0, 0, 0);
        textContainer->setSpacing(2);

        auto* platformLabel = new QLabel(tmpl.platform, card);
        m_templatePlatformLabels.append(platformLabel);
        platformLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        platformLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; font-weight: bold; border: none; background: transparent;"
        ).arg(accentColor));

        auto* titleLabel = new QLabel(tmpl.defaultTitle, card);
        if (m_i18n)
            titleLabel->setText(m_i18n->t("templates", tmpl.titleKey));
        m_templateTitleLabels.append(titleLabel);
        titleLabel->setWordWrap(true);
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        titleLabel->setObjectName(QStringLiteral("TemplateTitle"));

        auto* dimLabel = new QLabel(QStringLiteral("%1 × %2 px").arg(tmpl.width).arg(tmpl.height), card);
        dimLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        dimLabel->setObjectName(QStringLiteral("TemplateDims"));

        textContainer->addWidget(platformLabel);
        textContainer->addWidget(titleLabel);
        textContainer->addStretch();
        textContainer->addWidget(dimLabel);

        auto* metaRow = new QHBoxLayout;
        if (!brandIcon.isNull()) {
            auto* iconLabel = new QLabel(card);
            iconLabel->setPixmap(brandIcon);
            iconLabel->setFixedSize(brandIcon.size());
            iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
            metaRow->addWidget(iconLabel);
        }
        metaRow->addLayout(textContainer, 1);
        cardLayout->addLayout(metaRow);

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
    m_recentLabel->setObjectName(QStringLiteral("SectionLabel"));
    mainLayout->addWidget(m_recentLabel);

    m_recentStack = new QStackedWidget(this);
    m_recentStack->setMinimumHeight(140);
    m_recentStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    // Page 0: List of recent files
    m_recentList = new QListWidget(this);
    m_recentList->setIconSize(QSize(48, 36));
    // Estilo via QSS global (QListWidget) — tema reage em runtime
    connect(m_recentList, &QListWidget::itemActivated, this,
            [this](QListWidgetItem* item) {
                if (item)
                    emit recentActivated(item->data(Qt::UserRole).toString());
            });
    m_recentStack->addWidget(m_recentList);

    // Page 1: Empty state widget
    m_emptyStateWidget = new QWidget(this);
    m_emptyStateWidget->setMinimumHeight(140);
    m_emptyStateWidget->setObjectName(QStringLiteral("EmptyState"));
    auto* emptyLayout = new QVBoxLayout(m_emptyStateWidget);
    emptyLayout->setContentsMargins(20, 24, 20, 24);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(6);

    m_emptyStateIcon = new QLabel(m_emptyStateWidget);
    m_emptyStateIcon->setPixmap(ThemeIcons::emptyProjectsPlaceholder(64, 64));
    m_emptyStateIcon->setAlignment(Qt::AlignCenter);
    // QLabel e transparente por padrao — sem estilo inline

    m_emptyStateTitle = new QLabel(m_emptyStateWidget);
    m_emptyStateTitle->setAlignment(Qt::AlignCenter);
    QFont emptyTitleFont = m_emptyStateTitle->font();
    emptyTitleFont.setPointSize(12);
    emptyTitleFont.setBold(true);
    m_emptyStateTitle->setFont(emptyTitleFont);
    // cor herdada da palette do tema

    m_emptyStateSubtitle = new QLabel(m_emptyStateWidget);
    m_emptyStateSubtitle->setAlignment(Qt::AlignCenter);
    m_emptyStateSubtitle->setObjectName(QStringLiteral("HintLabel"));

    emptyLayout->addWidget(m_emptyStateIcon);
    emptyLayout->addWidget(m_emptyStateTitle);
    emptyLayout->addWidget(m_emptyStateSubtitle);

    m_recentStack->addWidget(m_emptyStateWidget);
    mainLayout->addWidget(m_recentStack);

    mainLayout->addStretch();
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
}

QWidget* StartScreen::createVisualCard(const QString& brandName, const QPixmap& icon,
                                      const QString& dimensions, const QString& ratio,
                                      const NewDocumentSpec& spec)
{
    auto* card = new QPushButton(this);
    card->setMinimumSize(180, 84);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    card->setCursor(Qt::PointingHandCursor);
    card->setObjectName(QStringLiteral("PresetCard"));

    auto* cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(10, 8, 10, 8);
    cardLayout->setSpacing(12);

    auto* iconLabel = new QLabel(card);
    iconLabel->setPixmap(icon);
    iconLabel->setFixedSize(icon.size());
    auto* textContainer = new QVBoxLayout;
    textContainer->setContentsMargins(0, 0, 0, 0);
    textContainer->setSpacing(2);

    auto* nameLabel = new QLabel(brandName, card);
    QFont nf = nameLabel->font();
    nf.setPointSize(11);
    nf.setBold(true);
    nameLabel->setFont(nf);
    // cor herdada da palette do tema

    auto* dimLabel = new QLabel(QStringLiteral("%1  (%2)").arg(dimensions, ratio), card);
    dimLabel->setObjectName(QStringLiteral("TemplateDims"));

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

    // Margens no layout do CONTEÚDO (não no layout externo): antes o
    // ajuste somava 16..60px externos aos 60px fixos do conteúdo e
    // desperdiçava metade da largura em telas pequenas.
    const int horizontalMargin = qBound(16, width() / 20, 60);
    const int verticalMargin = width() < Design::BreakSM ? 16 : 28;
    if (m_contentLayout)
        m_contentLayout->setContentsMargins(horizontalMargin, verticalMargin,
                                            horizontalMargin, verticalMargin);
    relayoutCards();
}

void StartScreen::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_scrollArea && m_scrollArea->verticalScrollBar())
        m_scrollArea->verticalScrollBar()->setValue(0);
}

void StartScreen::relayoutCards()
{
    if (!m_cardsGrid || !m_templatesGrid)
        return;

    // Four columns on wide screens, two on medium screens, and one on narrow
    // windows.  Widgets are reused, so resizing does not rebuild the screen
    // or lose signal connections.
    const int columns = width() < 620 ? 1 : (width() < 980 ? 2 : 4);
    const bool compact = width() < Design::BreakXS;

    if (m_actionRow)
        m_actionRow->setDirection(compact ? QBoxLayout::TopToBottom
                                          : QBoxLayout::LeftToRight);
    if (m_createDesignBtn && m_openProjectBtn && m_customSizeBtn) {
        const int buttonHeight = compact ? 40 : 44;
        m_createDesignBtn->setMinimumHeight(buttonHeight);
        m_openProjectBtn->setMinimumHeight(buttonHeight);
        m_customSizeBtn->setMinimumHeight(buttonHeight);
        m_createDesignBtn->setMinimumWidth(compact ? 140 : 170);
        m_openProjectBtn->setMinimumWidth(compact ? 140 : 160);
    }
    if (m_recentList)
        m_recentList->setIconSize(compact ? QSize(40, 30) : QSize(48, 36));
    if (m_cardsHost) {
        for (QPushButton* card : m_cardsHost->findChildren<QPushButton*>())
            card->setMinimumSize(compact ? QSize(140, 72) : QSize(180, 84));
    }
    if (m_templatesHost) {
        for (QPushButton* card : m_templatesHost->findChildren<QPushButton*>())
            card->setMinimumHeight(compact ? 140 : 178);
    }

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
    m_tagline->setText(m_i18n->t("common", "app.tagline"));

    m_createDesignBtn->setText(m_i18n->t("common", "start.createDesign"));
    m_openProjectBtn->setText(m_i18n->t("common", "start.openProject"));
    m_customSizeBtn->setText(m_i18n->t("common", "start.custom"));

    m_popularLabel->setText(m_i18n->t("common", "start.popular"));
    if (m_templatesLabel)
        m_templatesLabel->setText(m_i18n->t("common", "start.templates"));
    const auto templates = TemplateFactory::availableTemplates();
    for (int i = 0; i < templates.size() && i < m_templateTitleLabels.size(); ++i) {
        if (m_templateTitleLabels[i])
            m_templateTitleLabels[i]->setText(m_i18n->t("templates", templates[i].titleKey));
        if (m_templatePlatformLabels[i])
            m_templatePlatformLabels[i]->setText(templates[i].platform);
    }
    m_recentLabel->setText(m_i18n->t("common", "start.recent"));

    m_emptyStateTitle->setText(m_i18n->t("common", "start.emptyRecents"));
    m_emptyStateSubtitle->setText(m_i18n->t("common", "start.emptyRecentsHint"));
}

QPixmap StartScreen::templatePreview(TemplateKind kind)
{
    const int key = static_cast<int>(kind);
    if (m_templatePreviewCache.contains(key))
        return m_templatePreviewCache.value(key);

    auto doc = TemplateFactory::createTemplate(kind, m_i18n);
    if (!doc)
        return {};

    constexpr int maxW = 300;
    constexpr int maxH = 112;
    const double scale = qMin(static_cast<double>(maxW) / doc->width(),
                              static_cast<double>(maxH) / doc->height());
    const QSize size(qMax(1, qRound(doc->width() * scale)),
                     qMax(1, qRound(doc->height() * scale)));
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    QTransform transform;
    transform.scale(scale, scale);
    RenderOptions options;
    options.drawCheckerboard = false;
    options.canvasBorder = Qt::transparent;
    renderDocument(*doc, &painter, transform, options);
    painter.end();

    const QPixmap result = QPixmap::fromImage(image);
    m_templatePreviewCache.insert(key, result);
    return result;
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
