#pragma once

#include <memory>
#include <QString>
#include <QVector>

namespace cc {

class Document;
class I18nService;

enum class TemplateKind {
    YouTubeTechReview,
    YouTubeGamingEpic,
    InstagramPromoSale,
    TikTokReelsViral,
    YouTubePodcast,
    YouTubeFinance,
    SocialBanner
};

struct TemplateMeta {
    TemplateKind kind;
    QString id;
    QString titleKey;
    QString defaultTitle;
    QString descKey;
    QString defaultDesc;
    int width = 0;
    int height = 0;
    QString platform; // "YouTube", "Instagram", "TikTok"
};

class TemplateFactory {
public:
    static QVector<TemplateMeta> availableTemplates();
    static std::unique_ptr<Document> createTemplate(TemplateKind kind, I18nService* i18n = nullptr);
};

} // namespace cc
