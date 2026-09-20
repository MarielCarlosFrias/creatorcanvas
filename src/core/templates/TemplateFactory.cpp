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
            QStringLiteral("templates.yt_tech.title"),
            QStringLiteral("Review de Tecnologia (YouTube)"),
            QStringLiteral("templates.yt_tech.desc"),
            QStringLiteral("Thumbnail 16:9 profissional com contraste para reviews e análises."),
            1280, 720,
            QStringLiteral("YouTube")
        },
        {
            TemplateKind::YouTubeGamingEpic,
            QStringLiteral("yt_gaming_epic"),
            QStringLiteral("templates.yt_gaming.title"),
            QStringLiteral("Gaming / Gameplay Épica (YouTube)"),
            QStringLiteral("templates.yt_gaming.desc"),
            QStringLiteral("Thumbnail de alto impacto visual com texto em destaque e badge 4K."),
            1280, 720,
            QStringLiteral("YouTube")
        },
        {
            TemplateKind::InstagramPromoSale,
            QStringLiteral("insta_promo_sale"),
            QStringLiteral("templates.insta_promo.title"),
            QStringLiteral("Post Promocional / Oferta (Instagram)"),
            QStringLiteral("templates.insta_promo.desc"),
            QStringLiteral("Formato quadrado 1:1 limpo e moderno com chamada para ação."),
            1080, 1080,
            QStringLiteral("Instagram")
        },
        {
            TemplateKind::TikTokReelsViral,
            QStringLiteral("tiktok_reels_viral"),
            QStringLiteral("templates.tiktok_viral.title"),
            QStringLiteral("Curiosidade / Dica Viral (TikTok & Reels)"),
            QStringLiteral("templates.tiktok_viral.desc"),
            QStringLiteral("Formato vertical 9:16 perfeitamente alinhado na zona segura dos botões."),
            1080, 1920,
            QStringLiteral("TikTok")
        },
        {
            TemplateKind::YouTubePodcast,
            QStringLiteral("yt_podcast"),
            QStringLiteral("templates.yt_podcast.title"),
            QStringLiteral("Podcast / Entrevista (YouTube)"),
            QStringLiteral("templates.yt_podcast.desc"),
            QStringLiteral("Thumbnail 16:9 sofisticada para podcasts e mesas redondas com moldura e selo de episódio."),
            1280, 720,
            QStringLiteral("YouTube")
        },
        {
            TemplateKind::YouTubeFinance,
            QStringLiteral("yt_finance"),
            QStringLiteral("templates.yt_finance.title"),
            QStringLiteral("Finanças & Investimentos (YouTube)"),
            QStringLiteral("templates.yt_finance.desc"),
            QStringLiteral("Thumbnail 16:9 de alta conversão com setas e badges de ganhos/estatísticas."),
            1280, 720,
            QStringLiteral("YouTube")
        },
        {
            TemplateKind::SocialBanner,
            QStringLiteral("social_banner"),
            QStringLiteral("templates.social_banner.title"),
            QStringLiteral("Banner para Redes Sociais (LinkedIn / X)"),
            QStringLiteral("templates.social_banner.desc"),
            QStringLiteral("Banner panorâmico moderno para cabeçalho de perfil pessoal ou corporativo."),
            1584, 396,
            QStringLiteral("LinkedIn")
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
        title->content = QStringLiteral("O FINAL SECRETO!");
        title->sizePt = 78;
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
        title->transform.position = QPointF(380, 240);
        doc->addLayer(std::move(title));

        auto sub = std::make_unique<TextLayer>();
        sub->name = QStringLiteral("Subtítulo");
        sub->content = QStringLiteral("EPISÓDIO FINAL");
        sub->sizePt = 38;
        sub->bold = true;
        sub->color = QColor(0x00, 0xff, 0xff);
        sub->align = TextAlignment::Left;
        sub->transform.position = QPointF(380, 360);
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
        title->bold = true;
        title->color = Qt::white;
        title->align = TextAlignment::Left;
        title->effects.shadow.enabled = true;
        title->effects.shadow.blur = 12.0;
        title->effects.shadow.offsetY = 4.0;
        title->transform.position = QPointF(80, 240);
        doc->addLayer(std::move(title));

        // Nome do Convidado
        auto guest = std::make_unique<TextLayer>();
        guest->name = QStringLiteral("Nome Convidado");
        guest->content = QStringLiteral("COM NOME DO CONVIDADO");
        guest->sizePt = 22;
        guest->bold = true;
        guest->color = QColor(0xc0, 0x84, 0xfc);
        guest->align = TextAlignment::Left;
        guest->transform.position = QPointF(80, 590);
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
        tag->bold = true;
        tag->color = QColor(0xfb, 0xbf, 0x24); // Amarelo ouro
        tag->align = TextAlignment::Left;
        tag->transform.position = QPointF(80, 180);
        doc->addLayer(std::move(tag));

        auto headline = std::make_unique<TextLayer>();
        headline->name = QStringLiteral("Manchete Finanças");
        headline->content = QStringLiteral("COMO MULTIPLICAR\nSEU CAPITAL\nEM 2026");
        headline->sizePt = 56;
        headline->bold = true;
        headline->color = Qt::white;
        headline->align = TextAlignment::Left;
        headline->effects.outline.enabled = true;
        headline->effects.outline.color = Qt::black;
        headline->effects.outline.width = 5.0;
        headline->transform.position = QPointF(80, 260);
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
        nameText->bold = true;
        nameText->color = Qt::white;
        nameText->align = TextAlignment::Left;
        nameText->transform.position = QPointF(320, 130);
        doc->addLayer(std::move(nameText));

        // Subtítulo / Proposta de Valor
        auto subText = std::make_unique<TextLayer>();
        subText->name = QStringLiteral("Bio / Contato");
        subText->content = QStringLiteral("Ajudando empresas e marcas a escalarem com conteúdo visual e tecnologia.");
        subText->sizePt = 20;
        subText->color = QColor(0x94, 0xa3, 0xb8);
        subText->align = TextAlignment::Left;
        subText->transform.position = QPointF(320, 210);
        doc->addLayer(std::move(subText));

        // Redes / Site
        auto siteText = std::make_unique<TextLayer>();
        siteText->name = QStringLiteral("Website / Contato");
        siteText->content = QStringLiteral("🔗 seudominio.com  •  ✉ contato@seudominio.com");
        siteText->sizePt = 16;
        siteText->bold = true;
        siteText->color = QColor(0x38, 0xbd, 0xf8);
        siteText->align = TextAlignment::Left;
        siteText->transform.position = QPointF(320, 280);
        doc->addLayer(std::move(siteText));

        return doc;
    }
    }
    return nullptr;
}

} // namespace cc
