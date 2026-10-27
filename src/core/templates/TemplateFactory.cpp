#include "TemplateFactory.h"
#include "core/Document.h"
#include "core/layers/Layer.h"

namespace cc {

static QPolygonF makeRectPolygon(double w, double h)
{
    const double hw = w / 2.0;
    const double hh = h / 2.0;
    return QPolygonF{
        QPointF(-hw, -hh),
        QPointF(hw, -hh),
        QPointF(hw, hh),
        QPointF(-hw, hh)
    };
}

QVector<TemplateMeta> TemplateFactory::availableTemplates()
{
    return {
        {
            TemplateKind::YouTubeTechReview,
            QStringLiteral("yt_tech_review"),
            QStringLiteral("builtIn.yt_tech.title"),
            QStringLiteral("Review de Tecnologia (YouTube)"),
            QStringLiteral("builtIn.yt_tech.desc"),
            QStringLiteral("Thumbnail 16:9 profissional com contraste para reviews e análises."),
            1280, 720,
            QStringLiteral("YouTube")
        },
        {
            TemplateKind::YouTubeGamingEpic,
            QStringLiteral("yt_gaming_epic"),
            QStringLiteral("builtIn.yt_gaming.title"),
            QStringLiteral("Gaming / Gameplay Épica (YouTube)"),
            QStringLiteral("builtIn.yt_gaming.desc"),
            QStringLiteral("Thumbnail de alto impacto visual com texto em destaque e badge 4K."),
            1280, 720,
            QStringLiteral("YouTube")
        },
        {
            TemplateKind::InstagramPromoSale,
            QStringLiteral("insta_promo_sale"),
            QStringLiteral("builtIn.insta_promo.title"),
            QStringLiteral("Post Promocional / Oferta (Instagram)"),
            QStringLiteral("builtIn.insta_promo.desc"),
            QStringLiteral("Formato quadrado 1:1 limpo e moderno com chamada para ação."),
            1080, 1080,
            QStringLiteral("Instagram")
        },
        {
            TemplateKind::TikTokReelsViral,
            QStringLiteral("tiktok_reels_viral"),
            QStringLiteral("builtIn.tiktok_viral.title"),
            QStringLiteral("Curiosidade / Dica Viral (TikTok & Reels)"),
            QStringLiteral("builtIn.tiktok_viral.desc"),
            QStringLiteral("Formato vertical 9:16 perfeitamente alinhado na zona segura dos botões."),
            1080, 1920,
            QStringLiteral("TikTok")
        },
        {
            TemplateKind::YouTubePodcast,
            QStringLiteral("yt_podcast"),
            QStringLiteral("builtIn.yt_podcast.title"),
            QStringLiteral("Podcast / Entrevista (YouTube)"),
            QStringLiteral("builtIn.yt_podcast.desc"),
            QStringLiteral("Thumbnail 16:9 sofisticada para podcasts e mesas redondas com moldura e selo de episódio."),
            1280, 720,
            QStringLiteral("YouTube")
        },
        {
            TemplateKind::YouTubeFinance,
            QStringLiteral("yt_finance"),
            QStringLiteral("builtIn.yt_finance.title"),
            QStringLiteral("Finanças & Investimentos (YouTube)"),
            QStringLiteral("builtIn.yt_finance.desc"),
            QStringLiteral("Thumbnail 16:9 de alta conversão com setas e badges de ganhos/estatísticas."),
            1280, 720,
            QStringLiteral("YouTube")
        },
        {
            TemplateKind::SocialBanner,
            QStringLiteral("social_banner"),
            QStringLiteral("builtIn.social_banner.title"),
            QStringLiteral("Banner para Redes Sociais (LinkedIn / X)"),
            QStringLiteral("builtIn.social_banner.desc"),
            QStringLiteral("Banner panorâmico moderno para cabeçalho de perfil pessoal ou corporativo."),
            1584, 396,
            QStringLiteral("LinkedIn")
        },
        {
            TemplateKind::YouTubeShortsTrending,
            QStringLiteral("yt_shorts_trending"),
            QStringLiteral("builtIn.yt_shorts.title"),
            QStringLiteral("YouTube Shorts Viral / Trending"),
            QStringLiteral("builtIn.yt_shorts.desc"),
            QStringLiteral("Layout vertical 9:16 com gancho explosivo, balão de fala e badge em destaque."),
            1080, 1920,
            QStringLiteral("YouTube")
        },
        {
            TemplateKind::InstagramCarouselSlide,
            QStringLiteral("insta_carousel_slide"),
            QStringLiteral("builtIn.insta_carousel.title"),
            QStringLiteral("Slide de Carrossel (Instagram)"),
            QStringLiteral("builtIn.insta_carousel.desc"),
            QStringLiteral("Formato retrato 4:5 moderno para carrosséis educativos com indicador de páginas e coração."),
            1080, 1350,
            QStringLiteral("Instagram")
        },
        {
            TemplateKind::TwitchStreamOffline,
            QStringLiteral("twitch_stream_offline"),
            QStringLiteral("builtIn.twitch_offline.title"),
            QStringLiteral("Tela Offline / Intervalo (Twitch Stream)"),
            QStringLiteral("builtIn.twitch_offline.desc"),
            QStringLiteral("Tela 16:9 estilizada com gradiente escuro, redes sociais e aviso de transmissão encerrada."),
            1920, 1080,
            QStringLiteral("Twitch")
        },
        {
            TemplateKind::YouTubeTrueCrimeMystery,
            QStringLiteral("yt_true_crime_mystery"),
            QStringLiteral("builtIn.yt_true_crime.title"),
            QStringLiteral("Documentário & Mistério / True Crime (YouTube)"),
            QStringLiteral("builtIn.yt_true_crime.desc"),
            QStringLiteral("Thumbnail 16:9 de alto impacto dramático com tipografia gigante, contraste sombrio e badge de tempo."),
            1280, 720,
            QStringLiteral("YouTube")
        },
        {
            TemplateKind::ShowcaseCharacterVS,
            QStringLiteral("showcase_character_vs"),
            QStringLiteral("builtIn.showcase_vs.title"),
            QStringLiteral("Personagem em Duelo (VS) — Showcase"),
            QStringLiteral("builtIn.showcase_vs.desc"),
            QStringLiteral("Recorte IA + fundo radial com glow + faixa em gradiente — o combo clássico das thumbnails de gaming."),
            1280, 720,
            QStringLiteral("YouTube")
        },
        {
            TemplateKind::ShowcaseKineticType,
            QStringLiteral("showcase_kinetic_type"),
            QStringLiteral("builtIn.showcase_kinetic.title"),
            QStringLiteral("Tipografia Cinética — Showcase"),
            QStringLiteral("builtIn.showcase_kinetic.desc"),
            QStringLiteral("Texto com gradiente, letras vazadas e tilt 3D — demonstra os efeitos de tipografia do CreatorCanvas."),
            1280, 720,
            QStringLiteral("YouTube")
        },
        {
            TemplateKind::ShowcaseBeforeAfter,
            QStringLiteral("showcase_before_after"),
            QStringLiteral("builtIn.showcase_before_after.title"),
            QStringLiteral("Antes / Depois — Showcase"),
            QStringLiteral("builtIn.showcase_before_after.desc"),
            QStringLiteral("Comparação lado a lado com seta em gradiente e badges — ideal para transformações e filtros."),
            1280, 720,
            QStringLiteral("YouTube")
        },
        {
            TemplateKind::ShowcaseTutorialArrows,
            QStringLiteral("showcase_tutorial_arrows"),
            QStringLiteral("builtIn.showcase_tutorial.title"),
            QStringLiteral("Tutorial com Setas — Showcase"),
            QStringLiteral("builtIn.showcase_tutorial.desc"),
            QStringLiteral("Moldura de screenshot, seta curva, anel de destaque e balão de fala para vídeos de tutorial."),
            1280, 720,
            QStringLiteral("YouTube")
        },
        {
            TemplateKind::ShowcaseChromaCut,
            QStringLiteral("showcase_chroma_cut"),
            QStringLiteral("builtIn.showcase_chroma.title"),
            QStringLiteral("Chroma Cut — Showcase"),
            QStringLiteral("builtIn.showcase_chroma.desc"),
            QStringLiteral("Fundo verde para chroma key, texto atrás do sujeito e slots de recorte com a IA."),
            1280, 720,
            QStringLiteral("YouTube")
        }
    };
}

std::unique_ptr<Document> TemplateFactory::createTemplate(TemplateKind kind, I18nService* /*i18n*/)
{
    switch (kind) {
    case TemplateKind::YouTubeTechReview: {
        auto doc = std::make_unique<Document>(1280, 720, 96);

        // Fundo
        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Escuro");
        bg->fill = QColor(0x0e, 0x12, 0x17);
        doc->addLayer(std::move(bg));

        // Barra de destaque lateral ciano
        auto bar = std::make_unique<ShapeLayer>();
        bar->name = QStringLiteral("Barra Neon");
        bar->kind = ShapeKind::RoundedRect;
        bar->points = makeRectPolygon(12, 600);
        bar->cornerRadius = 6;
        bar->fill = QColor(0x00, 0xd2, 0xff);
        bar->strokeWidth = 0;
        bar->transform.position = QPointF(20, 360);
        doc->addLayer(std::move(bar));

        // Frame de produto/foto
        auto frame = std::make_unique<ShapeLayer>();
        frame->name = QStringLiteral("Moldura Produto");
        frame->kind = ShapeKind::RoundedRect;
        frame->points = makeRectPolygon(460, 560);
        frame->cornerRadius = 24;
        frame->fill = QColor(0x1a, 0x20, 0x2c);
        frame->stroke = QColor(0x2d, 0x37, 0x48);
        frame->strokeWidth = 3;
        frame->transform.position = QPointF(990, 360);
        doc->addLayer(std::move(frame));

        auto frameText = std::make_unique<TextLayer>();
        frameText->name = QStringLiteral("Dica Foto");
        frameText->content = QStringLiteral("Insira a foto do produto aqui");
        frameText->sizePt = 22;
        frameText->color = QColor(0x71, 0x80, 0x96);
        frameText->align = TextAlignment::Center;
        frameText->transform.position = QPointF(990, 360);
        doc->addLayer(std::move(frameText));

        // Badge Pill
        auto badge = std::make_unique<ShapeLayer>();
        badge->name = QStringLiteral("Badge Fundo");
        badge->kind = ShapeKind::RoundedRect;
        badge->points = makeRectPolygon(260, 48);
        badge->cornerRadius = 14;
        badge->fill = QColor(0xff, 0x00, 0x55);
        badge->strokeWidth = 0;
        badge->transform.position = QPointF(200, 140);
        doc->addLayer(std::move(badge));

        auto badgeText = std::make_unique<TextLayer>();
        badgeText->name = QStringLiteral("Badge Texto");
        badgeText->content = QStringLiteral("REVIEW COMPLETO");
        badgeText->sizePt = 20;
        badgeText->bold = true;
        badgeText->color = Qt::white;
        badgeText->align = TextAlignment::Center;
        badgeText->transform.position = QPointF(200, 140);
        doc->addLayer(std::move(badgeText));

        // Título Principal
        auto title = std::make_unique<TextLayer>();
        title->name = QStringLiteral("Título Principal");
        title->content = QStringLiteral("VALE A PENA?");
        title->sizePt = 74;
        title->bold = true;
        title->color = Qt::white;
        title->align = TextAlignment::Left;
        title->effects.shadow.enabled = true;
        title->effects.shadow.color = QColor(0, 0, 0, 200);
        title->effects.shadow.offsetX = 4.0;
        title->effects.shadow.offsetY = 4.0;
        title->effects.shadow.blur = 8.0;
        title->transform.position = QPointF(380, 270);
        doc->addLayer(std::move(title));

        // Subtítulo
        auto sub = std::make_unique<TextLayer>();
        sub->name = QStringLiteral("Subtítulo");
        sub->content = QStringLiteral("ANÁLISE SINCERA EM 2026");
        sub->sizePt = 34;
        sub->bold = true;
        sub->color = QColor(0xff, 0xea, 0x00);
        sub->align = TextAlignment::Left;
        sub->transform.position = QPointF(380, 380);
        doc->addLayer(std::move(sub));

        return doc;
    }

    case TemplateKind::YouTubeGamingEpic: {
        auto doc = std::make_unique<Document>(1280, 720, 96);

        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Roxo");
        bg->fill = QColor(0x12, 0x07, 0x2b);
        doc->addLayer(std::move(bg));

        auto frame = std::make_unique<ShapeLayer>();
        frame->name = QStringLiteral("Slot Personagem");
        frame->kind = ShapeKind::RoundedRect;
        frame->points = makeRectPolygon(480, 580);
        frame->cornerRadius = 24;
        frame->fill = QColor(0x25, 0x12, 0x4d);
        frame->stroke = QColor(0xe0, 0x40, 0xfb);
        frame->strokeWidth = 3;
        frame->transform.position = QPointF(980, 360);
        doc->addLayer(std::move(frame));

        auto frameText = std::make_unique<TextLayer>();
        frameText->name = QStringLiteral("Dica Personagem");
        frameText->content = QStringLiteral("Coloque seu personagem / face aqui");
        frameText->sizePt = 22;
        frameText->box = QSizeF(350, 100);
        frameText->color = QColor(0xb3, 0x88, 0xff);
        frameText->align = TextAlignment::Center;
        frameText->transform.position = QPointF(980, 360);
        doc->addLayer(std::move(frameText));

        auto badge = std::make_unique<ShapeLayer>();
        badge->name = QStringLiteral("Badge Fundo");
        badge->kind = ShapeKind::RoundedRect;
        badge->points = makeRectPolygon(220, 46);
        badge->cornerRadius = 12;
        badge->fill = QColor(0xff, 0x57, 0x22);
        badge->strokeWidth = 0;
        badge->transform.position = QPointF(180, 120);
        doc->addLayer(std::move(badge));

        auto badgeText = std::make_unique<TextLayer>();
        badgeText->name = QStringLiteral("Badge 4K");
        badgeText->content = QStringLiteral("GAMEPLAY 4K");
        badgeText->sizePt = 20;
        badgeText->bold = true;
        badgeText->color = Qt::white;
        badgeText->align = TextAlignment::Center;
        badgeText->transform.position = QPointF(180, 120);
        doc->addLayer(std::move(badgeText));

        auto title = std::make_unique<TextLayer>();
        title->name = QStringLiteral("Título Épico");
        title->content = QStringLiteral("FINAL\nSECRETO!");
        title->sizePt = 72;
        title->box = QSizeF(520, 190);
        title->bold = true;
        title->color = Qt::white;
        title->align = TextAlignment::Left;
        title->effects.outline.enabled = true;
        title->effects.outline.color = Qt::black;
        title->effects.outline.width = 5;
        title->effects.shadow.enabled = true;
        title->effects.shadow.color = QColor(0xff, 0x00, 0x55);
        title->effects.shadow.offsetX = 4.0;
        title->effects.shadow.offsetY = 4.0;
        title->effects.shadow.blur = 6.0;
        title->transform.position = QPointF(350, 270);
        doc->addLayer(std::move(title));

        auto sub = std::make_unique<TextLayer>();
        sub->name = QStringLiteral("Subtítulo");
        sub->content = QStringLiteral("EPISÓDIO FINAL");
        sub->sizePt = 38;
        sub->box = QSizeF(520, 56);
        sub->bold = true;
        sub->color = QColor(0x00, 0xff, 0xff);
        sub->align = TextAlignment::Left;
        sub->transform.position = QPointF(350, 515);
        doc->addLayer(std::move(sub));

        return doc;
    }

    case TemplateKind::InstagramPromoSale: {
        auto doc = std::make_unique<Document>(1080, 1080, 96);

        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo");
        bg->fill = QColor(0xf7, 0xf8, 0xfa);
        doc->addLayer(std::move(bg));

        auto card = std::make_unique<ShapeLayer>();
        card->name = QStringLiteral("Card Central");
        card->kind = ShapeKind::RoundedRect;
        card->points = makeRectPolygon(960, 960);
        card->cornerRadius = 28;
        card->fill = Qt::white;
        card->stroke = QColor(0xe2, 0xe8, 0xf0);
        card->strokeWidth = 2;
        card->transform.position = QPointF(540, 540);
        doc->addLayer(std::move(card));

        auto badge = std::make_unique<ShapeLayer>();
        badge->name = QStringLiteral("Badge Oferta");
        badge->kind = ShapeKind::RoundedRect;
        badge->points = makeRectPolygon(320, 56);
        badge->cornerRadius = 20;
        badge->fill = QColor(0x10, 0xb9, 0x81);
        badge->strokeWidth = 0;
        badge->transform.position = QPointF(540, 200);
        doc->addLayer(std::move(badge));

        auto badgeText = std::make_unique<TextLayer>();
        badgeText->name = QStringLiteral("Texto Badge");
        badgeText->content = QStringLiteral("OFERTA ESPECIAL");
        badgeText->sizePt = 24;
        badgeText->bold = true;
        badgeText->color = Qt::white;
        badgeText->align = TextAlignment::Center;
        badgeText->transform.position = QPointF(540, 200);
        doc->addLayer(std::move(badgeText));

        auto headline = std::make_unique<TextLayer>();
        headline->name = QStringLiteral("Título");
        headline->content = QStringLiteral("COLEÇÃO EXCLUSIVA");
        headline->sizePt = 50;
        headline->bold = true;
        headline->color = QColor(0x1e, 0x29, 0x3b);
        headline->align = TextAlignment::Center;
        headline->transform.position = QPointF(540, 360);
        doc->addLayer(std::move(headline));

        auto discount = std::make_unique<TextLayer>();
        discount->name = QStringLiteral("Destaque Desconto");
        discount->content = QStringLiteral("ATÉ 50% OFF");
        discount->sizePt = 84;
        discount->bold = true;
        discount->color = QColor(0x05, 0x96, 0x69);
        discount->align = TextAlignment::Center;
        discount->effects.shadow.enabled = true;
        discount->effects.shadow.color = QColor(0xa7, 0xf3, 0xd0, 180);
        discount->effects.shadow.offsetX = 2.0;
        discount->effects.shadow.offsetY = 3.0;
        discount->effects.shadow.blur = 4.0;
        discount->transform.position = QPointF(540, 530);
        doc->addLayer(std::move(discount));

        auto desc = std::make_unique<TextLayer>();
        desc->name = QStringLiteral("Descrição");
        desc->content = QStringLiteral("Aproveite descontos imperdíveis por tempo limitado.");
        desc->sizePt = 26;
        desc->color = QColor(0x64, 0x74, 0x8b);
        desc->align = TextAlignment::Center;
        desc->transform.position = QPointF(540, 680);
        doc->addLayer(std::move(desc));

        auto btn = std::make_unique<ShapeLayer>();
        btn->name = QStringLiteral("Botão CTA");
        btn->kind = ShapeKind::RoundedRect;
        btn->points = makeRectPolygon(320, 72);
        btn->cornerRadius = 18;
        btn->fill = QColor(0x0f, 0x17, 0x2a);
        btn->strokeWidth = 0;
        btn->transform.position = QPointF(540, 830);
        doc->addLayer(std::move(btn));

        auto btnText = std::make_unique<TextLayer>();
        btnText->name = QStringLiteral("Texto Botão");
        btnText->content = QStringLiteral("COMPRAR AGORA");
        btnText->sizePt = 24;
        btnText->bold = true;
        btnText->color = Qt::white;
        btnText->align = TextAlignment::Center;
        btnText->transform.position = QPointF(540, 830);
        doc->addLayer(std::move(btnText));

        return doc;
    }

    case TemplateKind::TikTokReelsViral: {
        auto doc = std::make_unique<Document>(1080, 1920, 96);

        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Noturno");
        bg->fill = QColor(0x09, 0x09, 0x0b);
        doc->addLayer(std::move(bg));

        auto card = std::make_unique<ShapeLayer>();
        card->name = QStringLiteral("Zona Segura Central");
        card->kind = ShapeKind::RoundedRect;
        card->points = makeRectPolygon(920, 1260);
        card->cornerRadius = 32;
        card->fill = QColor(0x18, 0x18, 0x1b);
        card->stroke = QColor(0x27, 0x27, 0x2a);
        card->strokeWidth = 2;
        card->transform.position = QPointF(540, 960);
        doc->addLayer(std::move(card));

        auto tag = std::make_unique<ShapeLayer>();
        tag->name = QStringLiteral("Badge Tag");
        tag->kind = ShapeKind::RoundedRect;
        tag->points = makeRectPolygon(340, 60);
        tag->cornerRadius = 18;
        tag->fill = QColor(0x8b, 0x5c, 0xf6);
        tag->strokeWidth = 0;
        tag->transform.position = QPointF(540, 540);
        doc->addLayer(std::move(tag));

        auto tagText = std::make_unique<TextLayer>();
        tagText->name = QStringLiteral("Texto Tag");
        tagText->content = QStringLiteral("DICA RÁPIDA ⚡");
        tagText->sizePt = 26;
        tagText->bold = true;
        tagText->color = Qt::white;
        tagText->align = TextAlignment::Center;
        tagText->transform.position = QPointF(540, 540);
        doc->addLayer(std::move(tagText));

        auto hook = std::make_unique<TextLayer>();
        hook->name = QStringLiteral("Texto Gancho");
        hook->content = QStringLiteral("3 TRUQUES QUE");
        hook->sizePt = 58;
        hook->bold = true;
        hook->color = QColor(0xfa, 0xcc, 0x15);
        hook->align = TextAlignment::Center;
        hook->transform.position = QPointF(540, 720);
        doc->addLayer(std::move(hook));

        auto mainText = std::make_unique<TextLayer>();
        mainText->name = QStringLiteral("Texto Principal");
        mainText->content = QStringLiteral("MUDARAM TUDO!");
        mainText->sizePt = 68;
        mainText->bold = true;
        mainText->color = Qt::white;
        mainText->align = TextAlignment::Center;
        mainText->effects.outline.enabled = true;
        mainText->effects.outline.color = Qt::black;
        mainText->effects.outline.width = 4;
        mainText->transform.position = QPointF(540, 870);
        doc->addLayer(std::move(mainText));

        auto divider = std::make_unique<ShapeLayer>();
        divider->name = QStringLiteral("Divisor Neon");
        divider->kind = ShapeKind::Rectangle;
        divider->points = makeRectPolygon(600, 6);
        divider->fill = QColor(0x06, 0xb6, 0xd4);
        divider->strokeWidth = 0;
        divider->transform.position = QPointF(540, 1020);
        doc->addLayer(std::move(divider));

        auto cta = std::make_unique<TextLayer>();
        cta->name = QStringLiteral("Texto Salvar");
        cta->content = QStringLiteral("SALVE ESTE VÍDEO");
        cta->sizePt = 32;
        cta->bold = true;
        cta->color = QColor(0xd4, 0xd4, 0xd8);
        cta->align = TextAlignment::Center;
        cta->transform.position = QPointF(540, 1160);
        doc->addLayer(std::move(cta));

        return doc;
    }
    case TemplateKind::YouTubePodcast: {
        auto doc = std::make_unique<Document>(1280, 720, 96);

        // Fundo escuro elegante de estúdio
        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Estúdio");
        bg->fill = QColor(0x12, 0x12, 0x14);
        doc->addLayer(std::move(bg));

        // Moldura do entrevistado (placeholder de foto)
        auto frame = std::make_unique<ShapeLayer>();
        frame->name = QStringLiteral("Moldura Convidado");
        frame->kind = ShapeKind::RoundedRect;
        frame->cornerRadius = 24.0;
        frame->points = makeRectPolygon(420, 560);
        frame->fill = QColor(0x27, 0x27, 0x2a);
        frame->stroke = QColor(0xa8, 0x55, 0xf7); // Roxo podcast
        frame->strokeWidth = 4.0;
        frame->transform.position = QPointF(1020, 360);
        doc->addLayer(std::move(frame));

        // Badge Episódio
        auto epBadge = std::make_unique<ShapeLayer>();
        epBadge->name = QStringLiteral("Badge Episódio");
        epBadge->kind = ShapeKind::RoundedRect;
        epBadge->cornerRadius = 8.0;
        epBadge->points = makeRectPolygon(180, 44);
        epBadge->fill = QColor(0xa8, 0x55, 0xf7);
        epBadge->strokeWidth = 0.0;
        epBadge->transform.position = QPointF(160, 140);
        doc->addLayer(std::move(epBadge));

        auto epText = std::make_unique<TextLayer>();
        epText->name = QStringLiteral("Texto Episódio");
        epText->content = QStringLiteral("EPISÓDIO #42");
        epText->sizePt = 16;
        epText->bold = true;
        epText->color = Qt::white;
        epText->align = TextAlignment::Center;
        epText->transform.position = QPointF(160, 140);
        doc->addLayer(std::move(epText));

        // Título Impactante
        auto title = std::make_unique<TextLayer>();
        title->name = QStringLiteral("Título Revelador");
        title->content = QStringLiteral("ELE CONTOU\nTUDO SOBRE\nO SEGREDO!");
        title->sizePt = 54;
        title->box = QSizeF(520, 220);
        title->bold = true;
        title->color = Qt::white;
        title->align = TextAlignment::Left;
        title->effects.shadow.enabled = true;
        title->effects.shadow.blur = 12.0;
        title->effects.shadow.offsetY = 4.0;
        title->transform.position = QPointF(340, 240);
        doc->addLayer(std::move(title));

        // Nome do Convidado
        auto guest = std::make_unique<TextLayer>();
        guest->name = QStringLiteral("Nome Convidado");
        guest->content = QStringLiteral("COM NOME DO CONVIDADO");
        guest->sizePt = 22;
        guest->box = QSizeF(500, 42);
        guest->bold = true;
        guest->color = QColor(0xc0, 0x84, 0xfc);
        guest->align = TextAlignment::Left;
        guest->transform.position = QPointF(330, 590);
        doc->addLayer(std::move(guest));

        return doc;
    }
    case TemplateKind::YouTubeFinance: {
        auto doc = std::make_unique<Document>(1280, 720, 96);

        // Fundo escuro azul petróleo
        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Finanças");
        bg->fill = QColor(0x0a, 0x0f, 0x1d);
        doc->addLayer(std::move(bg));

        // Badge Ganho / Alta
        auto badge = std::make_unique<ShapeLayer>();
        badge->name = QStringLiteral("Badge Retorno");
        badge->kind = ShapeKind::Badge;
        badge->points = makeRectPolygon(170, 170);
        badge->fill = QColor(0x10, 0xb9, 0x81); // Verde esmeralda
        badge->stroke = Qt::white;
        badge->strokeWidth = 3.0;
        badge->transform.position = QPointF(1100, 160);
        doc->addLayer(std::move(badge));

        auto badgeText = std::make_unique<TextLayer>();
        badgeText->name = QStringLiteral("Texto Retorno");
        badgeText->content = QStringLiteral("+340%");
        badgeText->sizePt = 28;
        badgeText->bold = true;
        badgeText->color = Qt::white;
        badgeText->align = TextAlignment::Center;
        badgeText->transform.position = QPointF(1100, 160);
        doc->addLayer(std::move(badgeText));

        // Seta de Alta
        auto arrow = std::make_unique<ShapeLayer>();
        arrow->name = QStringLiteral("Seta Alta");
        arrow->kind = ShapeKind::ArrowCurved;
        arrow->points = makeRectPolygon(360, 260);
        arrow->fill = QColor(0x34, 0xd3, 0x99);
        arrow->stroke = QColor(0x06, 0x5f, 0x46);
        arrow->strokeWidth = 3.0;
        arrow->transform.position = QPointF(850, 480);
        doc->addLayer(std::move(arrow));

        // Textos principais
        auto tag = std::make_unique<TextLayer>();
        tag->name = QStringLiteral("Tag Alerta");
        tag->content = QStringLiteral("URGENTE: NÃO COMPRE ANTES DE VER");
        tag->sizePt = 20;
        tag->box = QSizeF(760, 42);
        tag->bold = true;
        tag->color = QColor(0xfb, 0xbf, 0x24); // Amarelo ouro
        tag->align = TextAlignment::Left;
        tag->transform.position = QPointF(460, 180);
        doc->addLayer(std::move(tag));

        auto headline = std::make_unique<TextLayer>();
        headline->name = QStringLiteral("Manchete Finanças");
        headline->content = QStringLiteral("COMO MULTIPLICAR\nSEU CAPITAL\nEM 2026");
        headline->sizePt = 56;
        headline->box = QSizeF(980, 245);
        headline->bold = true;
        headline->color = Qt::white;
        headline->align = TextAlignment::Left;
        headline->effects.outline.enabled = true;
        headline->effects.outline.color = Qt::black;
        headline->effects.outline.width = 5.0;
        headline->transform.position = QPointF(405, 260);
        doc->addLayer(std::move(headline));

        return doc;
    }
    case TemplateKind::SocialBanner: {
        auto doc = std::make_unique<Document>(1584, 396, 96);

        // Fundo gradiente moderno sutil
        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Banner");
        bg->fill = QColor(0x0f, 0x17, 0x2a); // Slate escuro
        doc->addLayer(std::move(bg));

        // Faixa geométrica de destaque
        auto accent = std::make_unique<ShapeLayer>();
        accent->name = QStringLiteral("Faixa Acento");
        accent->kind = ShapeKind::Rectangle;
        accent->points = makeRectPolygon(40, 396);
        accent->fill = QColor(0x38, 0xbd, 0xf8); // Azul celeste
        accent->strokeWidth = 0.0;
        accent->transform.position = QPointF(20, 198);
        doc->addLayer(std::move(accent));

        // Estrela decorativa
        auto star = std::make_unique<ShapeLayer>();
        star->name = QStringLiteral("Estrela Destaque");
        star->kind = ShapeKind::Star;
        star->points = makeRectPolygon(60, 60);
        star->fill = QColor(0xf5, 0x9e, 0x0b);
        star->strokeWidth = 0.0;
        star->transform.position = QPointF(1480, 70);
        doc->addLayer(std::move(star));

        // Título / Nome Profissional
        auto nameText = std::make_unique<TextLayer>();
        nameText->name = QStringLiteral("Nome / Especialidade");
        nameText->content = QStringLiteral("SEU NOME | ESPECIALISTA & CRIADOR");
        nameText->sizePt = 36;
        nameText->box = QSizeF(1000, 55);
        nameText->bold = true;
        nameText->color = Qt::white;
        nameText->align = TextAlignment::Left;
        nameText->transform.position = QPointF(820, 130);
        doc->addLayer(std::move(nameText));

        // Subtítulo / Proposta de Valor
        auto subText = std::make_unique<TextLayer>();
        subText->name = QStringLiteral("Bio / Contato");
        subText->content = QStringLiteral("Ajudando empresas e marcas a escalarem com conteúdo visual e tecnologia.");
        subText->sizePt = 20;
        subText->box = QSizeF(1000, 42);
        subText->color = QColor(0x94, 0xa3, 0xb8);
        subText->align = TextAlignment::Left;
        subText->transform.position = QPointF(820, 210);
        doc->addLayer(std::move(subText));

        // Redes / Site
        auto siteText = std::make_unique<TextLayer>();
        siteText->name = QStringLiteral("Website / Contato");
        siteText->content = QStringLiteral("🔗 seudominio.com  •  ✉ contato@seudominio.com");
        siteText->sizePt = 16;
        siteText->box = QSizeF(800, 34);
        siteText->bold = true;
        siteText->color = QColor(0x38, 0xbd, 0xf8);
        siteText->align = TextAlignment::Left;
        siteText->transform.position = QPointF(720, 280);
        doc->addLayer(std::move(siteText));

        return doc;
    }
    case TemplateKind::YouTubeShortsTrending: {
        auto doc = std::make_unique<Document>(1080, 1920, 96);

        // Fundo escuro vibrante
        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Escuro");
        bg->fill = QColor(0x0f, 0x17, 0x2a); // Slate escuro
        doc->addLayer(std::move(bg));

        // Balão de fala chamativo no topo/centro
        auto bubble = std::make_unique<ShapeLayer>();
        bubble->name = QStringLiteral("Balão de Destaque");
        bubble->kind = ShapeKind::SpeechBubble;
        bubble->points = makeRectPolygon(800, 240);
        bubble->cornerRadius = 32;
        bubble->fill = QColor(0xff, 0x00, 0x55); // Neon Red/Pink
        bubble->strokeWidth = 0.0;
        bubble->transform.position = QPointF(540, 480);
        doc->addLayer(std::move(bubble));

        // Texto dentro do balão de fala
        auto bubbleText = std::make_unique<TextLayer>();
        bubbleText->name = QStringLiteral("Texto do Balão");
        bubbleText->content = QStringLiteral("VOCÊ SABIA DISSO?!");
        bubbleText->sizePt = 38;
        bubbleText->bold = true;
        bubbleText->color = Qt::white;
        bubbleText->align = TextAlignment::Center;
        bubbleText->transform.position = QPointF(540, 460);
        doc->addLayer(std::move(bubbleText));

        // Frame central para vídeo/foto
        auto frame = std::make_unique<ShapeLayer>();
        frame->name = QStringLiteral("Moldura Conteúdo");
        frame->kind = ShapeKind::RoundedRect;
        frame->points = makeRectPolygon(880, 720);
        frame->cornerRadius = 24;
        frame->fill = QColor(0x1e, 0x29, 0x3b);
        frame->stroke = QColor(0x38, 0xbd, 0xf8);
        frame->strokeWidth = 4.0;
        frame->transform.position = QPointF(540, 1020);
        doc->addLayer(std::move(frame));

        auto framePlaceholder = std::make_unique<TextLayer>();
        framePlaceholder->name = QStringLiteral("Dica Frame");
        framePlaceholder->content = QStringLiteral("Insira o clipe ou imagem principal");
        framePlaceholder->sizePt = 24;
        framePlaceholder->color = QColor(0x94, 0xa3, 0xb8);
        framePlaceholder->align = TextAlignment::Center;
        framePlaceholder->transform.position = QPointF(540, 1020);
        doc->addLayer(std::move(framePlaceholder));

        // Badge Pill de Trending
        auto pill = std::make_unique<ShapeLayer>();
        pill->name = QStringLiteral("Badge Trending");
        pill->kind = ShapeKind::RoundedRect;
        pill->points = makeRectPolygon(360, 70);
        pill->cornerRadius = 35;
        pill->fill = QColor(0xf5, 0x9e, 0x0b); // Amber
        pill->strokeWidth = 0.0;
        pill->transform.position = QPointF(540, 1460);
        doc->addLayer(std::move(pill));

        auto pillText = std::make_unique<TextLayer>();
        pillText->name = QStringLiteral("Texto Badge");
        pillText->content = QStringLiteral("🔥 TRENDING SHORTS");
        pillText->sizePt = 20;
        pillText->bold = true;
        pillText->color = QColor(0x0f, 0x17, 0x2a);
        pillText->align = TextAlignment::Center;
        pillText->transform.position = QPointF(540, 1460);
        doc->addLayer(std::move(pillText));

        // Call to action inferior
        auto ctaText = std::make_unique<TextLayer>();
        ctaText->name = QStringLiteral("CTA Inscreva-se");
        ctaText->content = QStringLiteral("INSCREVA-SE PARA MAIS DICAS");
        ctaText->sizePt = 22;
        ctaText->bold = true;
        ctaText->color = Qt::white;
        ctaText->align = TextAlignment::Center;
        ctaText->transform.position = QPointF(540, 1600);
        doc->addLayer(std::move(ctaText));

        return doc;
    }
    case TemplateKind::InstagramCarouselSlide: {
        auto doc = std::make_unique<Document>(1080, 1350, 96);

        // Fundo elegante off-white / dark clean
        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Dark Clean");
        bg->fill = QColor(0x18, 0x18, 0x1b); // Zinc escuro
        doc->addLayer(std::move(bg));

        // Ícone de Coração no topo direito
        auto heart = std::make_unique<ShapeLayer>();
        heart->name = QStringLiteral("Coração Like");
        heart->kind = ShapeKind::Heart;
        heart->points = makeRectPolygon(70, 70);
        heart->fill = QColor(0xf4, 0x3f, 0x5e); // Rose vibrante
        heart->strokeWidth = 0.0;
        heart->transform.position = QPointF(960, 110);
        doc->addLayer(std::move(heart));

        // Indicador de Slide (Pílula "02 / 05")
        auto indicator = std::make_unique<ShapeLayer>();
        indicator->name = QStringLiteral("Pílula Slide");
        indicator->kind = ShapeKind::RoundedRect;
        indicator->points = makeRectPolygon(140, 48);
        indicator->cornerRadius = 24;
        indicator->fill = QColor(0x27, 0x27, 0x2a);
        indicator->stroke = QColor(0x3f, 0x3f, 0x46);
        indicator->strokeWidth = 1.5;
        indicator->transform.position = QPointF(140, 110);
        doc->addLayer(std::move(indicator));

        auto indicatorText = std::make_unique<TextLayer>();
        indicatorText->name = QStringLiteral("Texto Indicador");
        indicatorText->content = QStringLiteral("02 / 05");
        indicatorText->sizePt = 16;
        indicatorText->bold = true;
        indicatorText->color = QColor(0xa1, 0xa1, 0xaa);
        indicatorText->align = TextAlignment::Center;
        indicatorText->transform.position = QPointF(140, 110);
        doc->addLayer(std::move(indicatorText));

        // Título do Tópico
        auto topicText = std::make_unique<TextLayer>();
        topicText->name = QStringLiteral("Título do Slide");
        topicText->content = QStringLiteral("DICA PRÁTICA #2");
        topicText->sizePt = 42;
        topicText->box = QSizeF(760, 64);
        topicText->bold = true;
        topicText->color = Qt::white;
        topicText->align = TextAlignment::Left;
        topicText->transform.position = QPointF(480, 240);
        doc->addLayer(std::move(topicText));

        // Bloco de Cartão com conteúdo principal
        auto card = std::make_unique<ShapeLayer>();
        card->name = QStringLiteral("Cartão de Conteúdo");
        card->kind = ShapeKind::RoundedRect;
        card->points = makeRectPolygon(880, 700);
        card->cornerRadius = 28;
        card->fill = QColor(0x27, 0x27, 0x2a);
        card->stroke = QColor(0x3f, 0x3f, 0x46);
        card->strokeWidth = 2.0;
        card->transform.position = QPointF(540, 700);
        doc->addLayer(std::move(card));

        // Texto dentro do cartão
        auto cardBody = std::make_unique<TextLayer>();
        cardBody->name = QStringLiteral("Texto do Cartão");
        cardBody->content = QStringLiteral("Escreva aqui o ponto principal ou insira uma ilustração impactante que retenha a atenção de quem está passando o carrossel.");
        cardBody->sizePt = 24;
        cardBody->color = QColor(0xd4, 0xd4, 0xd8);
        cardBody->align = TextAlignment::Center;
        cardBody->transform.position = QPointF(540, 700);
        doc->addLayer(std::move(cardBody));

        // Rodapé "Arraste para o lado 👉"
        auto footerText = std::make_unique<TextLayer>();
        footerText->name = QStringLiteral("Rodapé Arraste");
        footerText->content = QStringLiteral("Arraste para o lado 👉  |  Salve para consultar depois 📌");
        footerText->sizePt = 18;
        footerText->bold = true;
        footerText->color = QColor(0x71, 0x71, 0x7a);
        footerText->align = TextAlignment::Center;
        footerText->transform.position = QPointF(540, 1220);
        doc->addLayer(std::move(footerText));

        return doc;
    }
    case TemplateKind::TwitchStreamOffline: {
        auto doc = std::make_unique<Document>(1920, 1080, 96);

        // Fundo Roxo Twitch escuro
        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Twitch");
        bg->fill = QColor(0x0e, 0x0e, 0x10);
        doc->addLayer(std::move(bg));

        // Barra de destaque Twitch Purple
        auto bar = std::make_unique<ShapeLayer>();
        bar->name = QStringLiteral("Barra Twitch Roxo");
        bar->kind = ShapeKind::RoundedRect;
        bar->points = makeRectPolygon(1920, 8);
        bar->cornerRadius = 0;
        bar->fill = QColor(0x91, 0x46, 0xff); // Twitch Purple
        bar->strokeWidth = 0.0;
        bar->transform.position = QPointF(960, 4);
        doc->addLayer(std::move(bar));

        // Cartão Central "TRANSMISSÃO OFFLINE"
        auto card = std::make_unique<ShapeLayer>();
        card->name = QStringLiteral("Painel Central");
        card->kind = ShapeKind::RoundedRect;
        card->points = makeRectPolygon(960, 420);
        card->cornerRadius = 32;
        card->fill = QColor(0x18, 0x18, 0x1b);
        card->stroke = QColor(0x91, 0x46, 0xff);
        card->strokeWidth = 3.0;
        card->transform.position = QPointF(960, 480);
        doc->addLayer(std::move(card));

        // Título OFFLINE
        auto title = std::make_unique<TextLayer>();
        title->name = QStringLiteral("Título Offline");
        title->content = QStringLiteral("TRANSMISSÃO OFFLINE");
        title->sizePt = 44;
        title->bold = true;
        title->color = Qt::white;
        title->align = TextAlignment::Center;
        title->transform.position = QPointF(960, 400);
        doc->addLayer(std::move(title));

        // Subtítulo / Próxima Live
        auto scheduleText = std::make_unique<TextLayer>();
        scheduleText->name = QStringLiteral("Horário Próxima Live");
        scheduleText->content = QStringLiteral("Obrigado por assistir! Voltamos amanhã às 19:00.");
        scheduleText->sizePt = 22;
        scheduleText->color = QColor(0xa1, 0xa1, 0xaa);
        scheduleText->align = TextAlignment::Center;
        scheduleText->transform.position = QPointF(960, 480);
        doc->addLayer(std::move(scheduleText));

        // Pílula / Botão de Redes
        auto badge = std::make_unique<ShapeLayer>();
        badge->name = QStringLiteral("Pílula Notificações");
        badge->kind = ShapeKind::RoundedRect;
        badge->points = makeRectPolygon(440, 56);
        badge->cornerRadius = 28;
        badge->fill = QColor(0x91, 0x46, 0xff);
        badge->strokeWidth = 0.0;
        badge->transform.position = QPointF(960, 570);
        doc->addLayer(std::move(badge));

        auto badgeText = std::make_unique<TextLayer>();
        badgeText->name = QStringLiteral("Texto Notificações");
        badgeText->content = QStringLiteral("🔔 ATIVE O SININHO");
        badgeText->sizePt = 18;
        badgeText->bold = true;
        badgeText->color = Qt::white;
        badgeText->align = TextAlignment::Center;
        badgeText->transform.position = QPointF(960, 570);
        doc->addLayer(std::move(badgeText));

        // Barra inferior com redes sociais
        auto socialsText = std::make_unique<TextLayer>();
        socialsText->name = QStringLiteral("Redes Sociais");
        socialsText->content = QStringLiteral("TWITTER: @seucanal   •   YOUTUBE: @seucanal   •   DISCORD: discord.gg/seucanal");
        socialsText->sizePt = 18;
        socialsText->bold = true;
        socialsText->color = QColor(0x71, 0x71, 0x7a);
        socialsText->align = TextAlignment::Center;
        socialsText->transform.position = QPointF(960, 940);
        doc->addLayer(std::move(socialsText));

        return doc;
    }
    case TemplateKind::YouTubeTrueCrimeMystery: {
        auto doc = std::make_unique<Document>(1280, 720, 96);

        // 1. Fundo Textura / Papel Escuro Grunge
        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Dark Vinheta");
        bg->fill = QColor(0x12, 0x14, 0x16); // Escuro profundo
        doc->addLayer(std::move(bg));

        // 2. Vinheta / Luz fria lateral esquerda para contraste dramático
        auto glow = std::make_unique<ShapeLayer>();
        glow->name = QStringLiteral("Luz de Fundo Cinza");
        glow->kind = ShapeKind::Rectangle;
        glow->points = makeRectPolygon(1280, 720);
        glow->fill = QColor(0x2a, 0x2e, 0x33);
        glow->setOpacity(0.35f);
        glow->strokeWidth = 0.0;
        glow->transform.position = QPointF(640, 360);
        doc->addLayer(std::move(glow));

        // 3. Subtítulo / Gancho Superior ("ELE NÃO É")
        auto subText = std::make_unique<TextLayer>();
        subText->name = QStringLiteral("Gancho Superior");
        subText->content = QStringLiteral("ELE NÃO É");
        subText->sizePt = 38;
        subText->box = QSizeF(500, 56);
        subText->bold = true;
        subText->color = Qt::white;
        subText->align = TextAlignment::Left;
        subText->effects.shadow.enabled = true;
        subText->effects.shadow.color = QColor(0, 0, 0, 220);
        subText->effects.shadow.offsetX = 3.0;
        subText->effects.shadow.offsetY = 3.0;
        subText->effects.shadow.blur = 6.0;
        subText->transform.position = QPointF(320, 160);
        doc->addLayer(std::move(subText));

        // 4. Título Principal Gigante ("HUMANO") com letras maciças e sombra
        auto mainText = std::make_unique<TextLayer>();
        mainText->name = QStringLiteral("Título Principal");
        mainText->content = QStringLiteral("HUMANO");
        mainText->sizePt = 135;
        mainText->box = QSizeF(1080, 240);
        mainText->bold = true;
        mainText->color = Qt::black; // Fundo ou cor base (suporta textura)
        mainText->align = TextAlignment::Left;
        mainText->effects.outline.enabled = true;
        mainText->effects.outline.color = QColor(0x33, 0x33, 0x33);
        mainText->effects.outline.width = 3.0;
        mainText->effects.shadow.enabled = true;
        mainText->effects.shadow.color = QColor(0, 0, 0, 240);
        mainText->effects.shadow.offsetX = 5.0;
        mainText->effects.shadow.offsetY = 5.0;
        mainText->effects.shadow.blur = 10.0;
        mainText->transform.position = QPointF(370, 390);
        doc->addLayer(std::move(mainText));

        // 5. Placeholder para Foto Recortada / Rosto Central
        auto portraitFrame = std::make_unique<ShapeLayer>();
        portraitFrame->name = QStringLiteral("Guia Foto / Personagem");
        portraitFrame->kind = ShapeKind::RoundedRect;
        portraitFrame->points = makeRectPolygon(540, 640);
        portraitFrame->cornerRadius = 24.0;
        portraitFrame->fill = QColor(0x1a, 0x1f, 0x24);
        portraitFrame->stroke = QColor(0x55, 0x66, 0x77);
        portraitFrame->strokeWidth = 2.0;
        portraitFrame->setOpacity(0.65f);
        portraitFrame->transform.position = QPointF(700, 400);
        doc->addLayer(std::move(portraitFrame));

        auto portraitTip = std::make_unique<TextLayer>();
        portraitTip->name = QStringLiteral("Dica Personagem");
        portraitTip->content = QStringLiteral("Posicione a foto recortada\ndo personagem aqui");
        portraitTip->sizePt = 22;
        portraitTip->color = QColor(0x88, 0x99, 0xaa);
        portraitTip->align = TextAlignment::Center;
        portraitTip->transform.position = QPointF(700, 390);
        doc->addLayer(std::move(portraitTip));

        // 6. Badge de Duração de Vídeo (Estilo YouTube "25:25")
        auto timeBadge = std::make_unique<ShapeLayer>();
        timeBadge->name = QStringLiteral("Badge Minutagem");
        timeBadge->kind = ShapeKind::RoundedRect;
        timeBadge->points = makeRectPolygon(120, 46);
        timeBadge->cornerRadius = 8.0;
        timeBadge->fill = QColor(0, 0, 0, 225);
        timeBadge->strokeWidth = 0.0;
        timeBadge->transform.position = QPointF(1200, 675);
        doc->addLayer(std::move(timeBadge));

        auto timeText = std::make_unique<TextLayer>();
        timeText->name = QStringLiteral("Texto Minutagem");
        timeText->content = QStringLiteral("25:25");
        timeText->sizePt = 16;
        timeText->bold = true;
        timeText->color = Qt::white;
        timeText->align = TextAlignment::Center;
        timeText->transform.position = QPointF(1200, 675);
        doc->addLayer(std::move(timeText));

        return doc;
    }

    case TemplateKind::ShowcaseCharacterVS: {
        auto doc = std::make_unique<Document>(1280, 720, 96);

        // Fundo radial com glow central (agora de verdade!)
        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Glow Radial");
        bg->gradient.enabled = true;
        bg->gradient.kind = GradientKind::Radial;
        bg->gradient.center = QPointF(0.5, 0.42);
        bg->gradient.radius = 0.95;
        bg->gradient.stops = {{0.0f, QColor(0x1c, 0x2f, 0x5e)},
                              {1.0f, QColor(0x04, 0x06, 0x0d)}};
        doc->addLayer(std::move(bg));

        // Slots dos personagens (recorte IA)
        auto slotA = std::make_unique<ShapeLayer>();
        slotA->name = QStringLiteral("Slot Personagem A");
        slotA->kind = ShapeKind::RoundedRect;
        slotA->points = makeRectPolygon(380, 420);
        slotA->cornerRadius = 18.0;
        slotA->fill = QColor(0x10, 0x13, 0x1c);
        slotA->stroke = QColor(0x3a, 0x4a, 0x6b);
        slotA->strokeWidth = 3.0;
        slotA->transform.position = QPointF(300, 360);
        doc->addLayer(std::move(slotA));

        auto slotB = std::make_unique<ShapeLayer>();
        slotB->name = QStringLiteral("Slot Personagem B");
        slotB->kind = ShapeKind::RoundedRect;
        slotB->points = makeRectPolygon(380, 420);
        slotB->cornerRadius = 18.0;
        slotB->fill = QColor(0x10, 0x13, 0x1c);
        slotB->stroke = QColor(0xf1, 0x07, 0xa3);
        slotB->strokeWidth = 3.0;
        slotB->transform.position = QPointF(980, 360);
        doc->addLayer(std::move(slotB));

        // VS gigante com outline + glow
        auto vs = std::make_unique<TextLayer>();
        vs->name = QStringLiteral("VS");
        vs->content = QStringLiteral("VS");
        vs->sizePt = 140;
        vs->bold = true;
        vs->color = Qt::white;
        vs->effects.outline.enabled = true;
        vs->effects.outline.color = QColor(0xf1, 0x07, 0xa3);
        vs->effects.outline.width = 10.0;
        vs->effects.shadow.enabled = true;
        vs->effects.shadow.color = QColor(0, 0, 0, 220);
        vs->effects.shadow.blur = 12.0;
        vs->effects.shadow.offsetX = 6.0;
        vs->effects.shadow.offsetY = 6.0;
        vs->transform.position = QPointF(640, 340);
        doc->addLayer(std::move(vs));

        // Faixa inferior em gradiente
        auto strip = std::make_unique<ShapeLayer>();
        strip->name = QStringLiteral("Faixa Gradiente");
        strip->kind = ShapeKind::RoundedRect;
        strip->points = makeRectPolygon(1280, 100);
        strip->cornerRadius = 0.0;
        strip->gradient.enabled = true;
        strip->gradient.kind = GradientKind::Linear;
        strip->gradient.angleDeg = 0.0;
        strip->gradient.stops = {{0.0f, QColor(0x7b, 0x2f, 0xf7)},
                                 {1.0f, QColor(0xf1, 0x07, 0xa3)}};
        strip->transform.position = QPointF(640, 690);
        doc->addLayer(std::move(strip));

        auto hint = std::make_unique<TextLayer>();
        hint->name = QStringLiteral("Dica Faixa");
        hint->content = QStringLiteral("Recorte seus personagens com a IA e solte nos slots");
        hint->sizePt = 22;
        hint->bold = true;
        hint->color = Qt::white;
        hint->transform.position = QPointF(640, 690);
        doc->addLayer(std::move(hint));

        auto placeA = std::make_unique<TextLayer>();
        placeA->name = QStringLiteral("Placeholder A");
        placeA->content = QStringLiteral("Personagem A");
        placeA->sizePt = 24;
        placeA->color = QColor(0x8e, 0x9a, 0xb5);
        placeA->transform.position = QPointF(300, 600);
        doc->addLayer(std::move(placeA));

        auto placeB = std::make_unique<TextLayer>();
        placeB->name = QStringLiteral("Placeholder B");
        placeB->content = QStringLiteral("Personagem B");
        placeB->sizePt = 24;
        placeB->color = QColor(0xc5, 0x86, 0xd8);
        placeB->transform.position = QPointF(980, 600);
        doc->addLayer(std::move(placeB));

        auto badge = std::make_unique<ShapeLayer>();
        badge->name = QStringLiteral("Badge EP");
        badge->kind = ShapeKind::Badge;
        badge->points = makeRectPolygon(170, 60);
        badge->fill = QColor(0xff, 0x2d, 0x55);
        badge->transform.position = QPointF(150, 120);
        doc->addLayer(std::move(badge));

        auto badgeText = std::make_unique<TextLayer>();
        badgeText->name = QStringLiteral("Texto EP");
        badgeText->content = QStringLiteral("EP. 5");
        badgeText->sizePt = 30;
        badgeText->bold = true;
        badgeText->color = Qt::white;
        badgeText->transform.position = QPointF(150, 120);
        doc->addLayer(std::move(badgeText));

        return doc;
    }

    case TemplateKind::ShowcaseKineticType: {
        auto doc = std::make_unique<Document>(1280, 720, 96);

        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Gradiente Noturno");
        bg->gradient.enabled = true;
        bg->gradient.kind = GradientKind::Linear;
        bg->gradient.angleDeg = 90.0;
        bg->gradient.stops = {{0.0f, QColor(0x0f, 0x0c, 0x29)},
                              {0.5f, QColor(0x24, 0x24, 0x3e)},
                              {1.0f, QColor(0x30, 0x2b, 0x63)}};
        doc->addLayer(std::move(bg));

        auto kinetic = std::make_unique<TextLayer>();
        kinetic->name = QStringLiteral("Texto Gradiente");
        kinetic->content = QStringLiteral("KINETIC");
        kinetic->sizePt = 96;
        kinetic->bold = true;
        kinetic->color = Qt::white;
        kinetic->effects.gradient.enabled = true;
        kinetic->effects.gradient.startColor = QColor(0xff, 0xd2, 0x00);
        kinetic->effects.gradient.endColor = QColor(0xff, 0x2d, 0x00);
        kinetic->effects.gradient.angleDeg = 0.0;
        kinetic->effects.outline.enabled = true;
        kinetic->effects.outline.color = Qt::black;
        kinetic->effects.outline.width = 6.0;
        kinetic->transform.position = QPointF(420, 130);
        doc->addLayer(std::move(kinetic));

        auto hollow = std::make_unique<TextLayer>();
        hollow->name = QStringLiteral("Texto Vazado");
        hollow->content = QStringLiteral("HOLLOW");
        hollow->sizePt = 96;
        hollow->bold = true;
        hollow->color = QColor(0xf8, 0xfa, 0xfc);
        hollow->effects.hollow = true;
        hollow->effects.outline.enabled = true;
        hollow->effects.outline.color = QColor(0xf8, 0xfa, 0xfc);
        hollow->effects.outline.width = 3.0;
        hollow->transform.position = QPointF(420, 280);
        doc->addLayer(std::move(hollow));

        auto tilt = std::make_unique<TextLayer>();
        tilt->name = QStringLiteral("Texto Tilt 3D");
        tilt->content = QStringLiteral("TILT 3D");
        tilt->sizePt = 96;
        tilt->bold = true;
        tilt->color = Qt::white;
        tilt->effects.gradient.enabled = true;
        tilt->effects.gradient.startColor = QColor(0x22, 0xd3, 0xee);
        tilt->effects.gradient.endColor = QColor(0xa8, 0x55, 0xf7);
        tilt->effects.outline.enabled = true;
        tilt->effects.outline.color = Qt::black;
        tilt->effects.outline.width = 4.0;
        tilt->transform.shearX = 0.22;
        tilt->transform.shearY = -0.08;
        tilt->transform.position = QPointF(420, 450);
        doc->addLayer(std::move(tilt));

        auto star1 = std::make_unique<ShapeLayer>();
        star1->name = QStringLiteral("Estrela 1");
        star1->kind = ShapeKind::Star;
        star1->points = makeRectPolygon(90, 90);
        star1->fill = QColor(0xff, 0xd2, 0x00);
        star1->transform.rotationDeg = 12.0;
        star1->transform.position = QPointF(1120, 140);
        doc->addLayer(std::move(star1));

        auto star2 = std::make_unique<ShapeLayer>();
        star2->name = QStringLiteral("Estrela 2");
        star2->kind = ShapeKind::Star;
        star2->points = makeRectPolygon(90, 90);
        star2->fill = QColor(0xff, 0xd2, 0x00);
        star2->transform.rotationDeg = -18.0;
        star2->transform.scaleX = 0.6;
        star2->transform.scaleY = 0.6;
        star2->transform.position = QPointF(1190, 250);
        doc->addLayer(std::move(star2));

        auto caption = std::make_unique<TextLayer>();
        caption->name = QStringLiteral("Legenda");
        caption->content = QStringLiteral("Gradiente • Vazado • Perspectiva 3D — tudo em camadas de texto");
        caption->sizePt = 20;
        caption->color = QColor(0xcb, 0xd5, 0xe1);
        caption->transform.position = QPointF(640, 660);
        doc->addLayer(std::move(caption));

        return doc;
    }

    case TemplateKind::ShowcaseBeforeAfter: {
        auto doc = std::make_unique<Document>(1280, 720, 96);

        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Escuro");
        bg->fill = QColor(0x0b, 0x0e, 0x14);
        doc->addLayer(std::move(bg));

        auto title = std::make_unique<TextLayer>();
        title->name = QStringLiteral("Título");
        title->content = QStringLiteral("TRANSFORMAÇÃO");
        title->sizePt = 64;
        title->bold = true;
        title->color = Qt::white;
        title->effects.outline.enabled = true;
        title->effects.outline.color = QColor(0x22, 0xd3, 0xee);
        title->effects.outline.width = 5.0;
        title->transform.position = QPointF(640, 90);
        doc->addLayer(std::move(title));

        auto slotBefore = std::make_unique<ShapeLayer>();
        slotBefore->name = QStringLiteral("Slot Antes");
        slotBefore->kind = ShapeKind::RoundedRect;
        slotBefore->points = makeRectPolygon(480, 340);
        slotBefore->cornerRadius = 16.0;
        slotBefore->fill = QColor(0x1a, 0x1f, 0x29);
        slotBefore->stroke = QColor(0x47, 0x55, 0x69);
        slotBefore->strokeWidth = 3.0;
        slotBefore->transform.position = QPointF(330, 380);
        doc->addLayer(std::move(slotBefore));

        auto slotAfter = std::make_unique<ShapeLayer>();
        slotAfter->name = QStringLiteral("Slot Depois");
        slotAfter->kind = ShapeKind::RoundedRect;
        slotAfter->points = makeRectPolygon(480, 340);
        slotAfter->cornerRadius = 16.0;
        slotAfter->fill = QColor(0x14, 0x26, 0x1b);
        slotAfter->stroke = QColor(0x22, 0xc5, 0x5e);
        slotAfter->strokeWidth = 3.0;
        slotAfter->transform.position = QPointF(950, 380);
        doc->addLayer(std::move(slotAfter));

        auto before = std::make_unique<TextLayer>();
        before->name = QStringLiteral("Antes");
        before->content = QStringLiteral("ANTES");
        before->sizePt = 44;
        before->bold = true;
        before->color = QColor(0x94, 0xa3, 0xb8);
        before->transform.position = QPointF(330, 380);
        doc->addLayer(std::move(before));

        auto after = std::make_unique<TextLayer>();
        after->name = QStringLiteral("Depois");
        after->content = QStringLiteral("DEPOIS");
        after->sizePt = 44;
        after->bold = true;
        after->color = QColor(0x22, 0xc5, 0x5e);
        after->effects.outline.enabled = true;
        after->effects.outline.color = Qt::white;
        after->effects.outline.width = 3.0;
        after->transform.position = QPointF(950, 380);
        doc->addLayer(std::move(after));

        auto arrow = std::make_unique<ShapeLayer>();
        arrow->name = QStringLiteral("Seta Gradiente");
        arrow->kind = ShapeKind::ArrowRight;
        arrow->points = makeRectPolygon(170, 90);
        arrow->gradient.enabled = true;
        arrow->gradient.kind = GradientKind::Linear;
        arrow->gradient.angleDeg = 0.0;
        arrow->gradient.stops = {{0.0f, QColor(0xff, 0xd2, 0x00)},
                                 {1.0f, QColor(0xff, 0x6a, 0x00)}};
        arrow->transform.position = QPointF(640, 380);
        doc->addLayer(std::move(arrow));

        auto badge = std::make_unique<ShapeLayer>();
        badge->name = QStringLiteral("Badge Filtros");
        badge->kind = ShapeKind::Badge;
        badge->points = makeRectPolygon(300, 58);
        badge->fill = QColor(0x16, 0xa3, 0x4a);
        badge->transform.position = QPointF(640, 600);
        doc->addLayer(std::move(badge));

        auto badgeText = std::make_unique<TextLayer>();
        badgeText->name = QStringLiteral("Texto Filtros");
        badgeText->content = QStringLiteral("FILTROS 1-CLIQUE");
        badgeText->sizePt = 24;
        badgeText->bold = true;
        badgeText->color = Qt::white;
        badgeText->transform.position = QPointF(640, 600);
        doc->addLayer(std::move(badgeText));

        auto hint = std::make_unique<TextLayer>();
        hint->name = QStringLiteral("Dica");
        hint->content = QStringLiteral("P&B, Vivid Pop, Cyberpunk e mais no inspetor de Imagem");
        hint->sizePt = 18;
        hint->color = QColor(0x94, 0xa3, 0xb8);
        hint->transform.position = QPointF(640, 690);
        doc->addLayer(std::move(hint));

        return doc;
    }

    case TemplateKind::ShowcaseTutorialArrows: {
        auto doc = std::make_unique<Document>(1280, 720, 96);

        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Azulado");
        bg->gradient.enabled = true;
        bg->gradient.kind = GradientKind::Linear;
        bg->gradient.angleDeg = 90.0;
        bg->gradient.stops = {{0.0f, QColor(0x0a, 0x0f, 0x1e)},
                              {1.0f, QColor(0x16, 0x23, 0x3f)}};
        doc->addLayer(std::move(bg));

        auto frame = std::make_unique<ShapeLayer>();
        frame->name = QStringLiteral("Moldura Screenshot");
        frame->kind = ShapeKind::RoundedRect;
        frame->points = makeRectPolygon(760, 430);
        frame->cornerRadius = 16.0;
        frame->fill = QColor(0x0d, 0x14, 0x20);
        frame->stroke = QColor(0x3b, 0x82, 0xf6);
        frame->strokeWidth = 2.0;
        frame->transform.position = QPointF(560, 330);
        doc->addLayer(std::move(frame));

        auto topBar = std::make_unique<ShapeLayer>();
        topBar->name = QStringLiteral("Barra do App");
        topBar->kind = ShapeKind::RoundedRect;
        topBar->points = makeRectPolygon(720, 44);
        topBar->cornerRadius = 8.0;
        topBar->fill = QColor(0x1d, 0x29, 0x3b);
        topBar->transform.position = QPointF(560, 140);
        doc->addLayer(std::move(topBar));

        auto sidebar = std::make_unique<ShapeLayer>();
        sidebar->name = QStringLiteral("Painel Lateral");
        sidebar->kind = ShapeKind::Rectangle;
        sidebar->points = makeRectPolygon(150, 300);
        sidebar->fill = QColor(0x16, 0x23, 0x3f);
        sidebar->transform.position = QPointF(255, 345);
        doc->addLayer(std::move(sidebar));

        auto content = std::make_unique<ShapeLayer>();
        content->name = QStringLiteral("Area Conteudo");
        content->kind = ShapeKind::Rectangle;
        content->points = makeRectPolygon(500, 280);
        content->fill = QColor(0x11, 0x1a, 0x2c);
        content->transform.position = QPointF(625, 355);
        doc->addLayer(std::move(content));

        auto ring = std::make_unique<ShapeLayer>();
        ring->name = QStringLiteral("Destaque");
        ring->kind = ShapeKind::Ellipse;
        ring->points = makeRectPolygon(170, 170);
        ring->stroke = QColor(0xef, 0x44, 0x44);
        ring->strokeWidth = 6.0;
        ring->transform.position = QPointF(800, 430);
        doc->addLayer(std::move(ring));

        auto arrow = std::make_unique<ShapeLayer>();
        arrow->name = QStringLiteral("Seta Curva");
        arrow->kind = ShapeKind::ArrowCurved;
        arrow->points = makeRectPolygon(230, 230);
        arrow->fill = QColor(0xf5, 0x9e, 0x0b);
        arrow->transform.rotationDeg = 190.0;
        arrow->transform.position = QPointF(950, 590);
        doc->addLayer(std::move(arrow));

        auto bubble = std::make_unique<ShapeLayer>();
        bubble->name = QStringLiteral("Balao de Fala");
        bubble->kind = ShapeKind::SpeechBubble;
        bubble->points = makeRectPolygon(280, 100);
        bubble->fill = QColor(0xf8, 0xfa, 0xfc);
        bubble->transform.position = QPointF(220, 560);
        doc->addLayer(std::move(bubble));

        auto bubbleText = std::make_unique<TextLayer>();
        bubbleText->name = QStringLiteral("Texto Balao");
        bubbleText->content = QStringLiteral("Clique aqui!");
        bubbleText->sizePt = 24;
        bubbleText->bold = true;
        bubbleText->color = QColor(0x0f, 0x17, 0x2a);
        bubbleText->transform.position = QPointF(220, 555);
        doc->addLayer(std::move(bubbleText));

        const QColor stepColor(0xf5, 0x9e, 0x0b);
        const QPointF stepPos[3] = {QPointF(120, 190), QPointF(120, 380), QPointF(120, 560)};
        for (int i = 0; i < 3; ++i) {
            auto step = std::make_unique<ShapeLayer>();
            step->name = QStringLiteral("Passo %1").arg(i + 1);
            step->kind = ShapeKind::Badge;
            step->points = makeRectPolygon(58, 58);
            step->fill = stepColor;
            step->transform.position = stepPos[i];
            doc->addLayer(std::move(step));

            auto stepText = std::make_unique<TextLayer>();
            stepText->name = QStringLiteral("Texto Passo %1").arg(i + 1);
            stepText->content = QString::number(i + 1);
            stepText->sizePt = 28;
            stepText->bold = true;
            stepText->color = QColor(0x0f, 0x17, 0x2a);
            stepText->transform.position = stepPos[i];
            doc->addLayer(std::move(stepText));
        }

        auto caption = std::make_unique<TextLayer>();
        caption->name = QStringLiteral("Legenda");
        caption->content = QStringLiteral("Setas, balões e anéis de destaque para tutoriais");
        caption->sizePt = 20;
        caption->color = QColor(0x94, 0xa3, 0xb8);
        caption->transform.position = QPointF(640, 690);
        doc->addLayer(std::move(caption));

        return doc;
    }

    case TemplateKind::ShowcaseChromaCut: {
        auto doc = std::make_unique<Document>(1280, 720, 96);

        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Fundo Neutro");
        bg->fill = QColor(0x0f, 0x11, 0x17);
        doc->addLayer(std::move(bg));

        // Zona de fundo verde (chroma)
        auto greenZone = std::make_unique<ShapeLayer>();
        greenZone->name = QStringLiteral("Zona Verde (Chroma)");
        greenZone->kind = ShapeKind::Rectangle;
        greenZone->points = makeRectPolygon(620, 400);
        greenZone->fill = QColor(0x00, 0xb1, 0x40);
        greenZone->transform.position = QPointF(330, 330);
        doc->addLayer(std::move(greenZone));

        // Texto ATRAS do sujeito (adicionado antes do slot = fica atras)
        auto behind = std::make_unique<TextLayer>();
        behind->name = QStringLiteral("Texto Atras do Sujeito");
        behind->content = QStringLiteral("SPONSOR");
        behind->sizePt = 84;
        behind->bold = true;
        behind->color = QColor(0xf1, 0xc4, 0x0f);
        behind->effects.outline.enabled = true;
        behind->effects.outline.color = Qt::black;
        behind->effects.outline.width = 6.0;
        behind->transform.position = QPointF(330, 235);
        doc->addLayer(std::move(behind));

        // Slot da personagem por cima do texto
        auto frame = std::make_unique<ShapeLayer>();
        frame->name = QStringLiteral("Slot Foto");
        frame->kind = ShapeKind::RoundedRect;
        frame->points = makeRectPolygon(360, 460);
        frame->cornerRadius = 16.0;
        frame->fill = QColor(0x17, 0x1b, 0x26);
        frame->stroke = QColor(0xe2, 0xe8, 0xf0);
        frame->strokeWidth = 3.0;
        frame->transform.position = QPointF(330, 330);
        doc->addLayer(std::move(frame));

        auto frameText = std::make_unique<TextLayer>();
        frameText->name = QStringLiteral("Placeholder Foto");
        frameText->content = QStringLiteral("Sua foto aqui");
        frameText->sizePt = 20;
        frameText->color = QColor(0x8e, 0x9a, 0xb5);
        frameText->transform.position = QPointF(330, 330);
        doc->addLayer(std::move(frameText));

        auto title = std::make_unique<TextLayer>();
        title->name = QStringLiteral("Titulo Chroma");
        title->content = QStringLiteral("CHROMA KEY");
        title->sizePt = 64;
        title->bold = true;
        title->color = Qt::white;
        title->effects.outline.enabled = true;
        title->effects.outline.color = QColor(0x00, 0xb1, 0x40);
        title->effects.outline.width = 6.0;
        title->transform.position = QPointF(950, 170);
        doc->addLayer(std::move(title));

        auto steps = std::make_unique<TextLayer>();
        steps->name = QStringLiteral("Passos");
        steps->content = QStringLiteral("1. Fundo verde uniforme\n2. Chroma Cut no inspetor\n3. Texto atrás do sujeito");
        steps->sizePt = 24;
        steps->color = QColor(0xcb, 0xd5, 0xe1);
        steps->align = TextAlignment::Left;
        steps->transform.position = QPointF(900, 420);
        doc->addLayer(std::move(steps));

        auto hint = std::make_unique<TextLayer>();
        hint->name = QStringLiteral("Dica");
        hint->content = QStringLiteral("A Varinha Mágica e o Recorte IA também removem fundos");
        hint->sizePt = 18;
        hint->color = QColor(0x8b, 0x93, 0xa5);
        hint->transform.position = QPointF(950, 650);
        doc->addLayer(std::move(hint));

        return doc;
    }
    }
    return nullptr;
}

} // namespace cc
