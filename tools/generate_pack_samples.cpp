// Gera os projetos de exemplo construídos sobre o Pacote Inicial
// (resources/packs/starter). Uso: ./generate_pack_samples [dir-de-saida]
//
// Os fundos vem do pacote incluido no programa; os textos sao placeholders
// para o utilizador editar. Mantem o mesmo formato .creatorcanvas dos outros
// exemplos (exemplos_projetos/).
#include "core/Document.h"
#include "core/layers/Layer.h"
#include "core/serialization/ProjectFile.h"
#include "rendering/CanvasRenderer.h"

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <iostream>

using namespace cc;

namespace {

const char* kPackDir = "/home/theking1x/creatorcanvas/resources/packs/starter";

QImage loadPackImage(const QString& name)
{
    const QString path = QString::fromUtf8(kPackDir) + QStringLiteral("/") + name;
    const QImage image(path);
    if (image.isNull())
        std::cerr << "Falha a abrir " << path.toStdString() << "\n";
    return image;
}

/// Camada de imagem que cobre a tela inteira (caso de uso "fundo").
void addCoverImageLayer(Document& doc, const QImage& image, const QString& name)
{
    if (image.isNull())
        return;
    const LayerId assetId = doc.assets().addImage(image, QStringLiteral("png"));
    if (assetId.isNull())
        return;

    auto layer = std::make_unique<ImageLayer>();
    layer->name = name;
    layer->assetId = assetId;
    layer->naturalWidth = image.width();
    layer->naturalHeight = image.height();
    layer->transform.scaleX = static_cast<double>(doc.width()) / image.width();
    layer->transform.scaleY = static_cast<double>(doc.height()) / image.height();
    layer->transform.position = QPointF(doc.width() / 2.0, doc.height() / 2.0);
    doc.addLayer(std::move(layer));
}

/// Ultima camada do grupo raiz (addLayer devolve bool, não o ponteiro).
Layer* lastLayer(Document& doc)
{
    auto* root = doc.rootGroup();
    if (!root || root->children.empty())
        return nullptr;
    return root->children.back().get();
}

TextLayer* addText(Document& doc, const QString& name, const QString& content,
                   qreal sizePt, bool bold, const QColor& color,
                   const QPointF& position, TextAlignment align = TextAlignment::Center)
{
    auto text = std::make_unique<TextLayer>();
    text->name = name;
    text->content = content;
    text->sizePt = sizePt;
    text->bold = bold;
    text->color = color;
    text->align = align;
    text->transform.position = position;
    doc.addLayer(std::move(text));
    return static_cast<TextLayer*>(lastLayer(doc));
}

void addOutline(TextLayer* text, const QColor& color, qreal width)
{
    text->effects.outline.enabled = true;
    text->effects.outline.color = color;
    text->effects.outline.width = width;
}

void addShadow(TextLayer* text, int alpha, qreal offsetY, qreal blur)
{
    text->effects.shadow.enabled = true;
    text->effects.shadow.color = QColor(0, 0, 0, alpha);
    text->effects.shadow.offsetY = offsetY;
    text->effects.shadow.blur = blur;
}

void addGuidance(Document& doc, const QString& text, const QPointF& position)
{
    addText(doc, QStringLiteral("Dica"), text, 16.0, false,
            QColor(0xd8, 0xde, 0xe9), position);
}

ShapeLayer* addPill(Document& doc, const QString& name, double width, double height,
                    double radius, const QColor& fill, const QPointF& position)
{
    auto shape = std::make_unique<ShapeLayer>();
    shape->name = name;
    shape->kind = ShapeKind::RoundedRect;
    const double hw = width / 2.0;
    const double hh = height / 2.0;
    shape->points = QPolygonF{QPointF(-hw, -hh), QPointF(hw, -hh),
                              QPointF(hw, hh), QPointF(-hw, hh)};
    shape->cornerRadius = radius;
    shape->fill = fill;
    shape->transform.position = position;
    doc.addLayer(std::move(shape));
    return static_cast<ShapeLayer*>(lastLayer(doc));
}

QImage renderThumb(const Document& doc)
{
    QImage thumb(320, 180, QImage::Format_ARGB32);
    thumb.fill(QColor(0x18, 0x1a, 0x20));
    QPainter p(&thumb);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const double s = std::min(320.0 / doc.width(), 180.0 / doc.height());
    QTransform t;
    t.translate((320.0 - doc.width() * s) / 2.0, (180.0 - doc.height() * s) / 2.0);
    t.scale(s, s);
    RenderOptions opts;
    opts.drawCheckerboard = false;
    renderDocument(doc, &p, t, opts);
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

// 09. Miniatura YouTube sobre fundo do pacote (1280x720)
std::unique_ptr<Document> buildMiniatura()
{
    auto doc = std::make_unique<Document>(1280, 720);
    addCoverImageLayer(*doc, loadPackImage(QStringLiteral("blue-dots.jpg")),
                       QStringLiteral("Fundo Pontos Azuis"));

    TextLayer* title = addText(*doc, QStringLiteral("Titulo"),
                               QStringLiteral("O TEU TITULO AQUI"), 96.0, true,
                               Qt::white, QPointF(640, 300));
    addOutline(title, QColor(0x1b, 0x4d, 0xd8), 6.0);
    addShadow(title, 200, 10.0, 18.0);

    TextLayer* sub = addText(*doc, QStringLiteral("Subtitulo"),
                             QStringLiteral("Escreve aqui a tua chamada"), 40.0, false,
                             QColor(0xcb, 0xe3, 0xff), QPointF(640, 430));
    addShadow(sub, 190, 6.0, 12.0);

    addPill(*doc, QStringLiteral("Faixa"), 880, 10, 5,
            QColor(0x2f, 0x6f, 0xed), QPointF(640, 500));

    addGuidance(*doc, QStringLiteral("Edita os textos e arrasta para mover"),
                QPointF(640, 660));
    return doc;
}

// 10. Ecra final de video sobre moldura do pacote (1920x1080)
std::unique_ptr<Document> buildEcraFinal()
{
    auto doc = std::make_unique<Document>(1920, 1080);
    addCoverImageLayer(*doc, loadPackImage(QStringLiteral("endscreen-yellow.jpg")),
                       QStringLiteral("Moldura Ecra Final"));

    // O círculo central é escuro: texto branco para contrastar.
    TextLayer* title = addText(*doc, QStringLiteral("Titulo"),
                               QStringLiteral("O TEU CANAL"), 72.0, true,
                               Qt::white, QPointF(560, 470));
    addShadow(title, 160, 4.0, 8.0);

    addText(*doc, QStringLiteral("Chamada"),
            QStringLiteral("Escreve aqui a mensagem"), 34.0, false,
            QColor(0x2b, 0x2b, 0x2b), QPointF(560, 545));

    addGuidance(*doc, QStringLiteral("Substitui os textos e adiciona as tuas miniaturas"),
                QPointF(1500, 1010));
    return doc;
}

// 11. Publicacao Instagram com textura do pacote (1080x1350)
std::unique_ptr<Document> buildPost()
{
    auto doc = std::make_unique<Document>(1080, 1350);
    addCoverImageLayer(*doc, loadPackImage(QStringLiteral("hexagon-5.jpg")),
                       QStringLiteral("Textura Hexagonos"));

    // Cartao central para o texto respirar sobre a textura.
    ShapeLayer* card = addPill(*doc, QStringLiteral("Cartao"), 820, 520, 28,
                               QColor(0x11, 0x14, 0x1c), QPointF(540, 675));
    card->setOpacity(0.82f);

    TextLayer* title = addText(*doc, QStringLiteral("Titulo"),
                               QStringLiteral("NOVO EPISODIO"), 78.0, true,
                               Qt::white, QPointF(540, 560));
    addOutline(title, QColor(0x16, 0xa3, 0x4a), 4.0);

    addText(*doc, QStringLiteral("Subtitulo"),
            QStringLiteral("Terca-feira, as 18h"), 38.0, false,
            QColor(0xe6, 0xea, 0xf2), QPointF(540, 680));

    addPill(*doc, QStringLiteral("Botao"), 360, 84, 42,
            QColor(0x16, 0xa3, 0x4a), QPointF(540, 800));
    addText(*doc, QStringLiteral("Botao Texto"),
            QStringLiteral("VER AGORA"), 32.0, true, Qt::white, QPointF(540, 809));

    addGuidance(*doc, QStringLiteral("Troca a textura em Fundos & Texturas (Ctrl+B)"),
                QPointF(540, 1290));
    return doc;
}

// 12. Banner com brasas do pacote (1500x500)
std::unique_ptr<Document> buildBanner()
{
    auto doc = std::make_unique<Document>(1500, 500);
    addCoverImageLayer(*doc, loadPackImage(QStringLiteral("embers.jpg")),
                       QStringLiteral("Fundo Brasas"));

    TextLayer* title = addText(*doc, QStringLiteral("Titulo"),
                               QStringLiteral("NOVA SERIE"), 110.0, true,
                               Qt::white, QPointF(750, 190));
    addOutline(title, QColor(0xff, 0x5a, 0x1e), 5.0);
    addShadow(title, 210, 8.0, 16.0);

    addText(*doc, QStringLiteral("Subtitulo"),
            QStringLiteral("Episodios novos todas as semanas"), 38.0, false,
            QColor(0xff, 0xd9, 0xb0), QPointF(750, 300));

    addPill(*doc, QStringLiteral("Botao"), 420, 78, 39,
            QColor(0xff, 0x5a, 0x1e), QPointF(750, 400));
    addText(*doc, QStringLiteral("Botao Texto"),
            QStringLiteral("SUBSCREVE"), 34.0, true, Qt::white, QPointF(750, 409));
    return doc;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QString outDir = QStringLiteral("/home/theking1x/creatorcanvas/exemplos_projetos");
    if (argc > 1)
        outDir = QString::fromLocal8Bit(argv[1]);

    QDir dir(outDir);
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    std::cout << "Gerando projetos do pacote em: " << outDir.toStdString() << "\n";

    saveWithThumb(*buildMiniatura(),
                  dir.filePath(QStringLiteral("09_Cenario_Miniatura_Fundo.creatorcanvas")));
    saveWithThumb(*buildEcraFinal(),
                  dir.filePath(QStringLiteral("10_Cenario_Ecra_Final.creatorcanvas")));
    saveWithThumb(*buildPost(),
                  dir.filePath(QStringLiteral("11_Cenario_Post_Textura.creatorcanvas")));
    saveWithThumb(*buildBanner(),
                  dir.filePath(QStringLiteral("12_Cenario_Banner_Brasas.creatorcanvas")));

    std::cout << "Projetos do pacote gerados com sucesso.\n";
    return 0;
}
