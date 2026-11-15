#include "core/Document.h"
#include "core/layers/Layer.h"
#include "core/serialization/ProjectFile.h"
#include "rendering/CanvasRenderer.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <iostream>

using namespace cc;

namespace {

QPolygonF makeRectPoly(double w, double h)
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

QImage renderThumb(const Document& doc)
{
    QImage thumb(320, 180, QImage::Format_ARGB32);
    thumb.fill(QColor(0x18, 0x1a, 0x20));
    {
        QPainter p(&thumb);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const double sx = 320.0 / doc.width();
        const double sy = 180.0 / doc.height();
        const double s = std::min(sx, sy);
        QTransform t;
        t.translate((320.0 - doc.width() * s) / 2.0, (180.0 - doc.height() * s) / 2.0);
        t.scale(s, s);
        RenderOptions opts;
        opts.drawCheckerboard = false;
        renderDocument(doc, &p, t, opts);
    }
    return thumb;
}

bool saveWithThumb(const Document& doc, const QString& path)
{
    const QImage thumb = renderThumb(doc);
    QString err;
    if (!saveDocument(doc, path, &err, &thumb)) {
        std::cerr << "Erro salvando " << path.toStdString() << ": " << err.toStdString() << "\n";
        return false;
    }
    std::cout << "Criado com sucesso: " << path.toStdString() << "\n";
    return true;
}

// -------------------------------------------------------------
// 01. Banner Social (1080x1080)
// -------------------------------------------------------------
std::unique_ptr<Document> buildCenario01()
{
    auto doc = std::make_unique<Document>(1080, 1080);

    auto bg = std::make_unique<BackgroundLayer>();
    bg->name = QStringLiteral("Fundo Degradê");
    bg->gradient.enabled = true;
    bg->gradient.kind = GradientKind::Linear;
    bg->gradient.angleDeg = 45.0;
    bg->gradient.stops = {
        {0.0f, QColor(0x1a, 0x08, 0x36)},
        {0.5f, QColor(0x2a, 0x14, 0x5a)},
        {1.0f, QColor(0x0d, 0x1b, 0x3e)}
    };
    doc->addLayer(std::move(bg));

    // Card central translúcido
    auto card = std::make_unique<ShapeLayer>();
    card->name = QStringLiteral("Card Central");
    card->kind = ShapeKind::RoundedRect;
    card->points = makeRectPoly(880, 780);
    card->cornerRadius = 32;
    card->fill = QColor(0x18, 0x1e, 0x2e, 220);
    card->stroke = QColor(0x4a, 0x55, 0x68, 120);
    card->strokeWidth = 2.0;
    card->shadow.enabled = true;
    card->shadow.color = QColor(0, 0, 0, 140);
    card->shadow.blur = 30.0;
    card->shadow.offsetY = 16.0;
    card->transform.position = QPointF(540, 520);
    doc->addLayer(std::move(card));

    // Badge de Oferta (Estrela 6 pontas)
    auto star = std::make_unique<ShapeLayer>();
    star->name = QStringLiteral("Selo Oferta");
    star->kind = ShapeKind::Star;
    star->points = makeRectPoly(130, 130);
    star->starPoints = 6;
    star->starInnerRatio = 0.55;
    star->gradient.enabled = true;
    star->gradient.kind = GradientKind::Linear;
    star->gradient.stops = {
        {0.0f, QColor(0xff, 0xd7, 0x00)},
        {1.0f, QColor(0xff, 0x8c, 0x00)}
    };
    star->stroke = QColor(0xff, 0xff, 0xff, 200);
    star->strokeWidth = 2.0;
    star->shadow.enabled = true;
    star->shadow.color = QColor(0, 0, 0, 100);
    star->shadow.blur = 12.0;
    star->transform.position = QPointF(200, 240);
    doc->addLayer(std::move(star));

    // Título Principal
    auto title = std::make_unique<TextLayer>();
    title->name = QStringLiteral("Título Principal");
    title->content = QStringLiteral("MEGA PROMOÇÃO");
    title->sizePt = 48.0;
    title->bold = true;
    title->color = Qt::white;
    title->align = TextAlignment::Center;
    title->effects.outline.enabled = true;
    title->effects.outline.color = QColor(0x43, 0x78, 0xff);
    title->effects.outline.width = 4.0;
    title->effects.shadow.enabled = true;
    title->effects.shadow.color = QColor(0, 0, 0, 180);
    title->effects.shadow.offsetX = 0.0;
    title->effects.shadow.offsetY = 8.0;
    title->effects.shadow.blur = 16.0;
    title->transform.position = QPointF(540, 360);
    doc->addLayer(std::move(title));

    // Subtítulo
    auto sub = std::make_unique<TextLayer>();
    sub->name = QStringLiteral("Subtítulo");
    sub->content = QStringLiteral("Até 50% de Desconto em Todo o Catálogo");
    sub->sizePt = 22.0;
    sub->bold = false;
    sub->color = QColor(0xeb, 0xf8, 0xff);
    sub->align = TextAlignment::Center;
    sub->transform.position = QPointF(540, 470);
    doc->addLayer(std::move(sub));

    // Botão CTA
    auto btn = std::make_unique<ShapeLayer>();
    btn->name = QStringLiteral("Botão CTA Fundo");
    btn->kind = ShapeKind::RoundedRect;
    btn->points = makeRectPoly(380, 72);
    btn->cornerRadius = 20;
    btn->gradient.enabled = true;
    btn->gradient.kind = GradientKind::Linear;
    btn->gradient.stops = {
        {0.0f, QColor(0xff, 0x57, 0x22)},
        {1.0f, QColor(0xe9, 0x1e, 0x63)}
    };
    btn->shadow.enabled = true;
    btn->shadow.color = QColor(0, 0, 0, 120);
    btn->shadow.blur = 18.0;
    btn->shadow.offsetY = 6.0;
    btn->transform.position = QPointF(540, 600);
    doc->addLayer(std::move(btn));

    auto btnText = std::make_unique<TextLayer>();
    btnText->name = QStringLiteral("Botão CTA Texto");
    btnText->content = QStringLiteral("GARANTA O SEU");
    btnText->sizePt = 20.0;
    btnText->bold = true;
    btnText->color = Qt::white;
    btnText->align = TextAlignment::Center;
    btnText->transform.position = QPointF(540, 608);
    doc->addLayer(std::move(btnText));

    // Instruções de Treino
    auto tips = std::make_unique<TextLayer>();
    tips->name = QStringLiteral("💡 DICAS DE TREINO");
    tips->content = QStringLiteral("PRATIQUE AQUI:\n"
                                   "1. Pressione 'V' (Mover): Arraste os elementos e use as guias magnéticas para alinhar.\n"
                                   "2. Pressione 'T' (Texto): Dê um clique duplo nos textos para editá-los livremente.\n"
                                   "3. No painel direito, troque cores de preenchimento, bordas e efeitos!");
    tips->sizePt = 13.0;
    tips->color = QColor(0xa0, 0xae, 0xc0);
    tips->align = TextAlignment::Center;
    tips->transform.position = QPointF(540, 780);
    doc->addLayer(std::move(tips));

    return doc;
}

// -------------------------------------------------------------
// 02. Retoque de Foto (1280x800)
// -------------------------------------------------------------
std::unique_ptr<Document> buildCenario02()
{
    auto doc = std::make_unique<Document>(1280, 800);

    auto bg = std::make_unique<BackgroundLayer>();
    bg->name = QStringLiteral("Fundo Estúdio");
    bg->fill = QColor(0x21, 0x25, 0x2b);
    doc->addLayer(std::move(bg));

    // Criar uma imagem rica representando pele / superfície texturizada com imperfeições
    const int imgW = 900;
    const int imgH = 550;
    QImage surface(imgW, imgH, QImage::Format_ARGB32);
    surface.fill(QColor(0xe6, 0xc8, 0xb0));

    {
        QPainter p(&surface);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRadialGradient grad(imgW * 0.45, imgH * 0.4, imgW * 0.55);
        grad.setColorAt(0.0, QColor(0xf5, 0xdb, 0xc4));
        grad.setColorAt(0.7, QColor(0xdb, 0xb5, 0x9a));
        grad.setColorAt(1.0, QColor(0xba, 0x92, 0x76));
        p.fillRect(surface.rect(), grad);

        struct Blemish { int x; int y; int r; int alpha; };
        const Blemish blemishes[] = {
            {280, 160, 12, 180},
            {340, 220, 8, 160},
            {420, 170, 14, 200},
            {550, 260, 10, 170},
            {630, 190, 16, 210},
            {500, 370, 11, 190},
            {310, 390, 15, 180},
            {680, 340, 9, 160}
        };

        for (const auto& b : blemishes) {
            QRadialGradient spot(b.x, b.y, b.r);
            spot.setColorAt(0.0, QColor(0x4a, 0x28, 0x1e, b.alpha));
            spot.setColorAt(0.6, QColor(0x6e, 0x3d, 0x2f, b.alpha / 2));
            spot.setColorAt(1.0, Qt::transparent);
            p.setBrush(spot);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPoint(b.x, b.y), b.r, b.r);
        }

        p.setPen(QPen(QColor(0x3a, 0x1f, 0x18, 170), 3, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(440, 290, 490, 320);
    }

    const LayerId assetId = doc->assets().addImage(surface);
    auto photo = std::make_unique<ImageLayer>();
    photo->name = QStringLiteral("Superficie_Para_Retoque");
    photo->assetId = assetId;
    photo->naturalWidth = imgW;
    photo->naturalHeight = imgH;
    photo->transform.position = QPointF(640, 320);
    doc->addLayer(std::move(photo));

    auto guide = std::make_unique<TextLayer>();
    guide->name = QStringLiteral("🎯 GUIA DE RETOQUE");
    guide->content = QStringLiteral("EXERCÍCIO DE RETOQUE:\n"
                                    "1. Selecione a camada 'Superficie_Para_Retoque' no painel de camadas à direita.\n"
                                    "2. Pressione 'J' (Healing Brush) ou 'S' (Clone Stamp).\n"
                                    "3. No Linux: Mantenha 'Shift' ou 'Ctrl' pressionado e clique (ou use o BOTÃO DIREITO) numa área limpa para coletar a amostra.\n"
                                    "4. Pinte com o botão esquerdo sobre cada mancha escura para fazê-la sumir!");
    guide->sizePt = 14.0;
    guide->bold = true;
    guide->color = QColor(0x00, 0xd2, 0xff);
    guide->align = TextAlignment::Center;
    guide->transform.position = QPointF(640, 680);
    doc->addLayer(std::move(guide));

    return doc;
}

// -------------------------------------------------------------
// 03. Recorte de Objeto (1080x1080)
// -------------------------------------------------------------
std::unique_ptr<Document> buildCenario03()
{
    auto doc = std::make_unique<Document>(1080, 1080);

    auto bg = std::make_unique<BackgroundLayer>();
    bg->name = QStringLiteral("Fundo Base");
    bg->fill = QColor(0x1a, 0x1c, 0x23);
    doc->addLayer(std::move(bg));

    const int w = 840;
    const int h = 580;
    QImage bgImg(w, h, QImage::Format_ARGB32);
    {
        QPainter p(&bgImg);
        p.setRenderHint(QPainter::Antialiasing, true);
        QLinearGradient sunset(0, 0, 0, h);
        sunset.setColorAt(0.0, QColor(0xff, 0x6e, 0x40));
        sunset.setColorAt(0.5, QColor(0xff, 0x9e, 0x80));
        sunset.setColorAt(1.0, QColor(0x31, 0x1b, 0x92));
        p.fillRect(bgImg.rect(), sunset);
    }
    const LayerId bgAsset = doc->assets().addImage(bgImg);
    auto newScene = std::make_unique<ImageLayer>();
    newScene->name = QStringLiteral("Novo_Cenario_Fundo");
    newScene->assetId = bgAsset;
    newScene->naturalWidth = w;
    newScene->naturalHeight = h;
    newScene->transform.position = QPointF(540, 360);
    doc->addLayer(std::move(newScene));

    QImage objImg(w, h, QImage::Format_ARGB32);
    objImg.fill(QColor(0x00, 0xff, 0xff));
    {
        QPainter p(&objImg);
        p.setRenderHint(QPainter::Antialiasing, true);

        QPainterPath path;
        path.moveTo(350, 460);
        path.lineTo(490, 460);
        path.lineTo(470, 380);
        path.cubicTo(560, 350, 560, 200, 460, 180);
        path.lineTo(460, 140);
        path.lineTo(380, 140);
        path.lineTo(380, 180);
        path.cubicTo(280, 200, 280, 350, 370, 380);
        path.closeSubpath();

        QLinearGradient gold(300, 140, 540, 460);
        gold.setColorAt(0.0, QColor(0xff, 0xf1, 0x76));
        gold.setColorAt(0.5, QColor(0xff, 0xb3, 0x00));
        gold.setColorAt(1.0, QColor(0xff, 0x6f, 0x00));
        p.setBrush(gold);
        p.setPen(QPen(QColor(0xbf, 0x36, 0x0c), 4));
        p.drawPath(path);

        p.setBrush(Qt::white);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(420, 270), 22, 22);
    }

    const LayerId objAsset = doc->assets().addImage(objImg);
    auto targetObj = std::make_unique<ImageLayer>();
    targetObj->name = QStringLiteral("Objeto_Para_Recortar");
    targetObj->assetId = objAsset;
    targetObj->naturalWidth = w;
    targetObj->naturalHeight = h;
    targetObj->transform.position = QPointF(540, 360);
    doc->addLayer(std::move(targetObj));

    auto guide = std::make_unique<TextLayer>();
    guide->name = QStringLiteral("🎯 GUIA DE RECORTE");
    guide->content = QStringLiteral("3 FORMAS DE RECORTE PARA PRATICAR:\n\n"
                                    "1. Remoção por IA: Selecione 'Objeto_Para_Recortar' e vá em Imagem > Remover Fundo com IA.\n"
                                    "2. Varinha Mágica (W): Clique no fundo ciano brilhante e pressione Delete.\n"
                                    "3. Tesoura / Laço (L): Contorne o troféu clicando nos cantos e pressione Enter para fechar!");
    guide->sizePt = 13.0;
    guide->bold = true;
    guide->color = QColor(0x68, 0xd3, 0x91);
    guide->align = TextAlignment::Center;
    guide->transform.position = QPointF(540, 800);
    doc->addLayer(std::move(guide));

    return doc;
}

// -------------------------------------------------------------
// 04. Desenho Vetorial e Curvas (1200x800)
// -------------------------------------------------------------
std::unique_ptr<Document> buildCenario04()
{
    auto doc = std::make_unique<Document>(1200, 800);

    auto bg = std::make_unique<BackgroundLayer>();
    bg->name = QStringLiteral("Fundo Tech");
    bg->fill = QColor(0x0a, 0x0f, 0x1d);
    doc->addLayer(std::move(bg));

    // Asa vetorial estilizada 1 (emblema principal)
    auto vec = std::make_unique<VectorLayer>();
    vec->name = QStringLiteral("Simbolo_Asa_Curvas");
    vec->fill = QColor(0x38, 0xbd, 0xf8, 70);
    vec->stroke = QColor(0x38, 0xbd, 0xf8);
    vec->strokeWidth = 4.0;
    vec->fillEnabled = true;
    vec->strokeEnabled = true;
    vec->closed = true;
    vec->transform.position = QPointF(600, 340);

    VectorNode n1;
    n1.position = QPointF(-220, 60);
    n1.handleIn = QPointF(-20, 40);
    n1.handleOut = QPointF(40, -80);
    n1.smooth = true;

    VectorNode n2;
    n2.position = QPointF(-70, -110);
    n2.handleIn = QPointF(-70, -10);
    n2.handleOut = QPointF(80, 10);
    n2.smooth = true;

    VectorNode n3;
    n3.position = QPointF(190, -70);
    n3.handleIn = QPointF(-60, -40);
    n3.handleOut = QPointF(40, 70);
    n3.smooth = true;

    VectorNode n4;
    n4.position = QPointF(130, 90);
    n4.handleIn = QPointF(40, -30);
    n4.handleOut = QPointF(-70, 30);
    n4.smooth = true;

    VectorNode n5;
    n5.position = QPointF(-40, 95);
    n5.handleIn = QPointF(50, 5);
    n5.handleOut = QPointF(-50, -5);
    n5.smooth = true;

    vec->nodes = {n1, n2, n3, n4, n5};
    vec->rebuildPath();
    doc->addLayer(std::move(vec));

    // Elemento vetorial secundário interno (asa menor)
    auto innerVec = std::make_unique<VectorLayer>();
    innerVec->name = QStringLiteral("Detalhe_Interno");
    innerVec->fill = QColor(0x81, 0x8c, 0xf8, 90);
    innerVec->stroke = QColor(0xc7, 0xd2, 0xfe);
    innerVec->strokeWidth = 3.0;
    innerVec->fillEnabled = true;
    innerVec->strokeEnabled = true;
    innerVec->closed = true;
    innerVec->transform.position = QPointF(600, 340);

    VectorNode in1;
    in1.position = QPointF(-130, 40);
    in1.handleIn = QPointF(-10, 25);
    in1.handleOut = QPointF(25, -50);
    in1.smooth = true;

    VectorNode in2;
    in2.position = QPointF(-30, -50);
    in2.handleIn = QPointF(-40, -5);
    in2.handleOut = QPointF(45, 5);
    in2.smooth = true;

    VectorNode in3;
    in3.position = QPointF(100, -25);
    in3.handleIn = QPointF(-35, -20);
    in3.handleOut = QPointF(20, 35);
    in3.smooth = true;

    VectorNode in4;
    in4.position = QPointF(50, 55);
    in4.handleIn = QPointF(25, -15);
    in4.handleOut = QPointF(-45, 10);
    in4.smooth = true;

    innerVec->nodes = {in1, in2, in3, in4};
    innerVec->rebuildPath();
    doc->addLayer(std::move(innerVec));

    auto guide = std::make_unique<TextLayer>();
    guide->name = QStringLiteral("🎯 GUIA DE VETORES");
    guide->content = QStringLiteral("EXERCÍCIO DE CURVAS VETORIAIS:\n\n"
                                    "1. Caneta (P): Clique e arraste para puxar curvas suaves com alças Bézier!\n"
                                    "2. Edição de Nós (A): Clique em qualquer nó da 'Simbolo_Asa_Curvas' para inspecionar e puxar suas alças.\n"
                                    "3. Pressione Enter para fechar caminhos ou Esc para cancelar.");
    guide->sizePt = 14.0;
    guide->bold = true;
    guide->color = QColor(0x38, 0xbd, 0xf8);
    guide->align = TextAlignment::Center;
    guide->transform.position = QPointF(600, 680);
    doc->addLayer(std::move(guide));

    return doc;
}

// -------------------------------------------------------------
// 05. Fatias para Web (1920x1080)
// -------------------------------------------------------------
std::unique_ptr<Document> buildCenario05()
{
    auto doc = std::make_unique<Document>(1920, 1080);

    auto bg = std::make_unique<BackgroundLayer>();
    bg->name = QStringLiteral("Fundo Web");
    bg->fill = QColor(0x11, 0x18, 0x27);
    doc->addLayer(std::move(bg));

    auto headerBg = std::make_unique<ShapeLayer>();
    headerBg->name = QStringLiteral("Header Fundo");
    headerBg->kind = ShapeKind::Rectangle;
    headerBg->points = makeRectPoly(1920, 220);
    headerBg->fill = QColor(0x1f, 0x29, 0x37);
    headerBg->transform.position = QPointF(960, 110);
    doc->addLayer(std::move(headerBg));

    auto headerLogo = std::make_unique<TextLayer>();
    headerLogo->name = QStringLiteral("Header Logo");
    headerLogo->content = QStringLiteral("CREATORCANVAS — WEB DESIGN");
    headerLogo->sizePt = 24.0;
    headerLogo->bold = true;
    headerLogo->color = QColor(0x60, 0xa5, 0xfa);
    headerLogo->align = TextAlignment::Center;
    headerLogo->transform.position = QPointF(960, 110);
    doc->addLayer(std::move(headerLogo));

    auto heroBg = std::make_unique<ShapeLayer>();
    heroBg->name = QStringLiteral("Hero Banner Fundo");
    heroBg->kind = ShapeKind::Rectangle;
    heroBg->points = makeRectPoly(1800, 520);
    heroBg->cornerRadius = 24;
    heroBg->gradient.enabled = true;
    heroBg->gradient.kind = GradientKind::Linear;
    heroBg->gradient.stops = {
        {0.0f, QColor(0x31, 0x2e, 0x81)},
        {1.0f, QColor(0x43, 0x38, 0xca)}
    };
    heroBg->transform.position = QPointF(960, 500);
    doc->addLayer(std::move(heroBg));

    auto heroTitle = std::make_unique<TextLayer>();
    heroTitle->name = QStringLiteral("Hero Título");
    heroTitle->content = QStringLiteral("FATIAMENTO INTELIGENTE DE LAYOUTS");
    heroTitle->sizePt = 36.0;
    heroTitle->bold = true;
    heroTitle->color = Qt::white;
    heroTitle->align = TextAlignment::Center;
    heroTitle->transform.position = QPointF(960, 460);
    doc->addLayer(std::move(heroTitle));

    auto heroSub = std::make_unique<TextLayer>();
    heroSub->name = QStringLiteral("Hero Instruções");
    heroSub->content = QStringLiteral("🎯 EXERCÍCIO DE WEB SLICES:\n"
                                      "1. Vá em Arquivo > Fatias para Web...\n"
                                      "2. Note que o Topo e o Rodapé já têm fatias registradas.\n"
                                      "3. Passe o mouse sobre a prévia e arraste a mira (+) para selecionar este Banner Hero central!\n"
                                      "4. Clique em 'Adicionar fatia' e depois em 'Exportar todas...'!");
    heroSub->sizePt = 16.0;
    heroSub->color = QColor(0x93, 0xc5, 0xfd);
    heroSub->align = TextAlignment::Center;
    heroSub->transform.position = QPointF(960, 560);
    doc->addLayer(std::move(heroSub));

    auto footerBg = std::make_unique<ShapeLayer>();
    footerBg->name = QStringLiteral("Footer Fundo");
    footerBg->kind = ShapeKind::Rectangle;
    footerBg->points = makeRectPoly(1920, 260);
    footerBg->fill = QColor(0x11, 0x18, 0x27);
    footerBg->stroke = QColor(0x37, 0x41, 0x51);
    footerBg->strokeWidth = 2.0;
    footerBg->transform.position = QPointF(960, 950);
    doc->addLayer(std::move(footerBg));

    auto footerText = std::make_unique<TextLayer>();
    footerText->name = QStringLiteral("Footer Texto");
    footerText->content = QStringLiteral("© 2026 CreatorCanvas Inc. — Todos os direitos reservados");
    footerText->sizePt = 16.0;
    footerText->color = QColor(0x9c, 0xa3, 0xaf);
    footerText->align = TextAlignment::Center;
    footerText->transform.position = QPointF(960, 950);
    doc->addLayer(std::move(footerText));

    WebSlice s1;
    s1.name = QStringLiteral("header_topo");
    s1.rect = QRectF(0, 0, 1920, 220);
    s1.kind = SliceKind::Image;
    s1.exportFormat = QStringLiteral("png");
    doc->addWebSlice(std::move(s1));

    WebSlice s2;
    s2.name = QStringLiteral("footer_rodape");
    s2.rect = QRectF(0, 820, 1920, 260);
    s2.kind = SliceKind::Image;
    s2.exportFormat = QStringLiteral("jpeg");
    doc->addWebSlice(std::move(s2));

    return doc;
}

// -------------------------------------------------------------
// 06. Enquadramento e Crop (1920x1080)
// -------------------------------------------------------------
std::unique_ptr<Document> buildCenario06()
{
    auto doc = std::make_unique<Document>(1920, 1080);

    auto bg = std::make_unique<BackgroundLayer>();
    bg->name = QStringLiteral("Fundo Escuro");
    bg->fill = QColor(0x12, 0x12, 0x16);
    doc->addLayer(std::move(bg));

    const int w = 1800;
    const int h = 1000;
    QImage panorama(w, h, QImage::Format_ARGB32);
    {
        QPainter p(&panorama);
        p.setRenderHint(QPainter::Antialiasing, true);

        QLinearGradient sky(0, 0, 0, h * 0.7);
        sky.setColorAt(0.0, QColor(0x1a, 0x10, 0x2f));
        sky.setColorAt(0.5, QColor(0x4a, 0x1c, 0x40));
        sky.setColorAt(1.0, QColor(0xbf, 0x43, 0x42));
        p.fillRect(QRect(0, 0, w, h * 0.7), sky);

        QLinearGradient water(0, h * 0.7, 0, h);
        water.setColorAt(0.0, QColor(0x1f, 0x31, 0x40));
        water.setColorAt(1.0, QColor(0x0a, 0x15, 0x20));
        p.fillRect(QRect(0, h * 0.7, w, h * 0.3), water);

        p.setBrush(QColor(0xff, 0xd1, 0x66));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(1200, 480), 90, 90);

        QPolygonF mtn;
        mtn << QPointF(0, 700) << QPointF(400, 450) << QPointF(750, 700)
            << QPointF(1100, 520) << QPointF(1450, 700) << QPointF(1800, 600)
            << QPointF(1800, 700);
        p.setBrush(QColor(0x18, 0x10, 0x24));
        p.drawPolygon(mtn);
    }

    const LayerId panoAsset = doc->assets().addImage(panorama);
    auto panoLayer = std::make_unique<ImageLayer>();
    panoLayer->name = QStringLiteral("Foto_Panoramica_16x9");
    panoLayer->assetId = panoAsset;
    panoLayer->naturalWidth = w;
    panoLayer->naturalHeight = h;
    panoLayer->transform.position = QPointF(960, 540);
    doc->addLayer(std::move(panoLayer));

    auto guide = std::make_unique<TextLayer>();
    guide->name = QStringLiteral("🎯 GUIA DE CORTE");
    guide->content = QStringLiteral("EXERCÍCIO DE ENQUADRAMENTO (CROP):\n\n"
                                    "1. Pressione 'C' (Crop Tool).\n"
                                    "2. Na barra superior, escolha a proporção '1:1' (Feed Quadrado) ou '9:16' (Stories Vertical).\n"
                                    "3. Enquadre o Sol na linha de interseção da regra dos terços.\n"
                                    "4. Pressione Enter para aplicar o corte!");
    guide->sizePt = 16.0;
    guide->bold = true;
    guide->color = Qt::white;
    guide->effects.shadow.enabled = true;
    guide->effects.shadow.color = QColor(0, 0, 0, 220);
    guide->effects.shadow.blur = 12.0;
    guide->align = TextAlignment::Center;
    guide->transform.position = QPointF(960, 960);
    doc->addLayer(std::move(guide));

    return doc;
}

// -------------------------------------------------------------
// 07. Tipografia e Efeitos Visuais (1080x1350 - Cartaz Vertical)
// -------------------------------------------------------------
std::unique_ptr<Document> buildCenario07()
{
    auto doc = std::make_unique<Document>(1080, 1350);

    auto bg = std::make_unique<BackgroundLayer>();
    bg->name = QStringLiteral("Fundo Cósmico");
    bg->fill = QColor(0x0b, 0x0f, 0x19);
    doc->addLayer(std::move(bg));

    // Círculo de brilho de fundo
    auto glow = std::make_unique<ShapeLayer>();
    glow->name = QStringLiteral("Brilho_Neon_Fundo");
    glow->kind = ShapeKind::Ellipse;
    glow->fill = QColor(0x63, 0x66, 0xf1, 80);
    glow->strokeWidth = 0.0;
    glow->stroke = Qt::transparent;
    glow->points = {QPointF(-250, -250), QPointF(250, 250)};
    glow->transform.position = QPointF(540, 480);
    doc->addLayer(std::move(glow));

    // Título Principal com Sombra
    auto title = std::make_unique<TextLayer>();
    title->name = QStringLiteral("Titulo_Principal");
    title->content = QStringLiteral("CREATIVE\nHORIZONS");
    title->sizePt = 76.0;
    title->bold = true;
    title->color = QColor(0xff, 0xff, 0xff);
    title->align = TextAlignment::Center;
    title->effects.shadow.enabled = true;
    title->effects.shadow.color = QColor(0, 0, 0, 230);
    title->effects.shadow.offsetX = 6.0;
    title->effects.shadow.offsetY = 8.0;
    title->effects.shadow.blur = 18.0;
    title->effects.outline.enabled = true;
    title->effects.outline.color = QColor(0x81, 0x8c, 0xf8);
    title->effects.outline.width = 3.0;
    title->transform.position = QPointF(540, 460);
    doc->addLayer(std::move(title));

    // Subtítulo
    auto sub = std::make_unique<TextLayer>();
    sub->name = QStringLiteral("Subtitulo_Design");
    sub->content = QStringLiteral("MASTER DIGITAL ART & TYPOGRAPHY");
    sub->sizePt = 16.0;
    sub->bold = true;
    sub->color = QColor(0x38, 0xbd, 0xf8);
    sub->align = TextAlignment::Center;
    sub->transform.position = QPointF(540, 620);
    doc->addLayer(std::move(sub));

    // Guia do Usuário
    auto guide = std::make_unique<TextLayer>();
    guide->name = QStringLiteral("🎯 GUIA DE TIPOGRAFIA");
    guide->content = QStringLiteral("EXERCÍCIO DE TIPOGRAFIA E EFEITOS:\n\n"
                                    "1. Selecione a camada 'Titulo_Principal' no painel à direita.\n"
                                    "2. No Inspetor de Texto, experimente alterar a cor, adicionar contorno ou sombra projetada.\n"
                                    "3. Use a ferramenta de Texto (T) para editar o conteúdo ou testar novas fontes instaladas!\n"
                                    "4. Ajuste a escala e o espaçamento para obter uma composição equilibrada.");
    guide->sizePt = 14.0;
    guide->bold = true;
    guide->color = QColor(0xfb, 0xbf, 0x24);
    guide->align = TextAlignment::Center;
    guide->transform.position = QPointF(540, 1000);
    doc->addLayer(std::move(guide));

    return doc;
}

// -------------------------------------------------------------
// 08. Modos de Mesclagem e Composição (1200x800)
// -------------------------------------------------------------
std::unique_ptr<Document> buildCenario08()
{
    auto doc = std::make_unique<Document>(1200, 800);

    auto bg = std::make_unique<BackgroundLayer>();
    bg->name = QStringLiteral("Fundo Escuro");
    bg->fill = QColor(0x11, 0x18, 0x27);
    doc->addLayer(std::move(bg));

    // Luz Magenta com Blend Mode Screen
    auto light1 = std::make_unique<ShapeLayer>();
    light1->name = QStringLiteral("Luz_Magenta (Screen)");
    light1->kind = ShapeKind::Ellipse;
    light1->fill = QColor(0xec, 0x48, 0x99);
    light1->strokeWidth = 0.0;
    light1->stroke = Qt::transparent;
    light1->blendMode = BlendMode::Screen;
    light1->setOpacity(0.85f);
    light1->points = {QPointF(-200, -200), QPointF(200, 200)};
    light1->transform.position = QPointF(480, 360);
    doc->addLayer(std::move(light1));

    // Luz Ciano com Blend Mode Screen
    auto light2 = std::make_unique<ShapeLayer>();
    light2->name = QStringLiteral("Luz_Ciano (Screen)");
    light2->kind = ShapeKind::Ellipse;
    light2->fill = QColor(0x06, 0xb6, 0xd4);
    light2->strokeWidth = 0.0;
    light2->stroke = Qt::transparent;
    light2->blendMode = BlendMode::Screen;
    light2->setOpacity(0.85f);
    light2->points = {QPointF(-200, -200), QPointF(200, 200)};
    light2->transform.position = QPointF(720, 360);
    doc->addLayer(std::move(light2));

    // Luz Amarela Superior
    auto light3 = std::make_unique<ShapeLayer>();
    light3->name = QStringLiteral("Luz_Dourada (Overlay)");
    light3->kind = ShapeKind::Ellipse;
    light3->fill = QColor(0xf5, 0x9e, 0x0b);
    light3->strokeWidth = 0.0;
    light3->stroke = Qt::transparent;
    light3->blendMode = BlendMode::Overlay;
    light3->setOpacity(0.75f);
    light3->points = {QPointF(-150, -150), QPointF(150, 150)};
    light3->transform.position = QPointF(600, 260);
    doc->addLayer(std::move(light3));

    // Título em destaque
    auto badge = std::make_unique<TextLayer>();
    badge->name = QStringLiteral("Texto_Neon");
    badge->content = QStringLiteral("NEON PULSE");
    badge->sizePt = 54.0;
    badge->bold = true;
    badge->color = Qt::white;
    badge->align = TextAlignment::Center;
    badge->effects.shadow.enabled = true;
    badge->effects.shadow.color = QColor(0, 0, 0, 200);
    badge->effects.shadow.blur = 14.0;
    badge->transform.position = QPointF(600, 370);
    doc->addLayer(std::move(badge));

    // Guia do Usuário
    auto guide = std::make_unique<TextLayer>();
    guide->name = QStringLiteral("🎯 GUIA DE MESCLAGEM");
    guide->content = QStringLiteral("EXERCÍCIO DE MODOS DE MESCLAGEM (BLEND MODES):\n\n"
                                    "1. Selecione a camada 'Luz_Magenta (Screen)' ou 'Luz_Ciano (Screen)' no painel à direita.\n"
                                    "2. No topo do painel de camadas, clique no menu suspenso de Modo de Mesclagem.\n"
                                    "3. Troque entre 'Multiply', 'Overlay', 'Color Dodge' e veja a interação das cores em tempo real!\n"
                                    "4. Ajuste a Opacidade de cada camada para dosar a intensidade da iluminação.");
    guide->sizePt = 14.0;
    guide->bold = true;
    guide->color = QColor(0xa7, 0xf3, 0xd0);
    guide->align = TextAlignment::Center;
    guide->transform.position = QPointF(600, 680);
    doc->addLayer(std::move(guide));

    return doc;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QString outDir = QStringLiteral("/home/theking1x/creatorcanvas/exemplos_projetos");
    if (argc > 1) {
        outDir = QString::fromLocal8Bit(argv[1]);
    }

    QDir dir(outDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    std::cout << "Gerando projetos de exemplo em: " << outDir.toStdString() << "\n";

    saveWithThumb(*buildCenario01(), dir.filePath(QStringLiteral("01_Cenario_Banner_Social.creatorcanvas")));
    saveWithThumb(*buildCenario02(), dir.filePath(QStringLiteral("02_Cenario_Retoque_Foto.creatorcanvas")));
    saveWithThumb(*buildCenario03(), dir.filePath(QStringLiteral("03_Cenario_Recorte_Objeto.creatorcanvas")));
    saveWithThumb(*buildCenario04(), dir.filePath(QStringLiteral("04_Cenario_Vetor_Logotipo.creatorcanvas")));
    saveWithThumb(*buildCenario05(), dir.filePath(QStringLiteral("05_Cenario_Web_Slices.creatorcanvas")));
    saveWithThumb(*buildCenario06(), dir.filePath(QStringLiteral("06_Cenario_Enquadramento_Crop.creatorcanvas")));
    saveWithThumb(*buildCenario07(), dir.filePath(QStringLiteral("07_Cenario_Tipografia_Efeitos.creatorcanvas")));
    saveWithThumb(*buildCenario08(), dir.filePath(QStringLiteral("08_Cenario_Gradientes_Composicao.creatorcanvas")));

    std::cout << "Todos os 8 projetos de exemplo foram gerados com sucesso!\n";
    return 0;
}
