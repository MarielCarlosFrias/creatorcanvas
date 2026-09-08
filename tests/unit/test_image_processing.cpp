#include <QTest>
#include <QImage>
#include <QPainter>
#include <QPolygonF>

#include "core/image/ImageProcessing.h"
#include "core/assets/AssetStore.h"
#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"

using namespace cc;

class TestImageProcessing : public QObject
{
    Q_OBJECT

private slots:
    void testCropImage();
    void testCalculateAspectCropRect();
    void testRemoveBackgroundContiguous();
    void testRemoveBackgroundNonContiguous();
    void testScissorsCutKeepInside();
    void testScissorsCutEraseInside();
    void testCloneStamp();
    void testCloneStampHardnessAndOpacity();
    void testMagicWandTolerance();
    void testScissorsCutFreehand();
    void testAssetStoreAddImage();
    void testModifyImageLayerCommand();
};

void TestImageProcessing::testCropImage()
{
    QImage img(100, 100, QImage::Format_ARGB32);
    img.fill(Qt::blue);

    // Recorte valido 40x30
    QRect cropRect(10, 20, 40, 30);
    QImage cropped = ImageProcessing::cropImage(img, cropRect);
    QCOMPARE(cropped.width(), 40);
    QCOMPARE(cropped.height(), 30);
    QCOMPARE(cropped.pixelColor(0, 0), QColor(Qt::blue));

    // Recorte que ultrapassa os limites da imagem original (deve intersectar)
    QRect overflowRect(80, 80, 50, 50);
    QImage clipped = ImageProcessing::cropImage(img, overflowRect);
    QCOMPARE(clipped.width(), 20);
    QCOMPARE(clipped.height(), 20);

    // Recorte totalmente fora dos limites
    QRect outsideRect(200, 200, 50, 50);
    QImage empty = ImageProcessing::cropImage(img, outsideRect);
    QVERIFY(empty.isNull());
}

void TestImageProcessing::testCalculateAspectCropRect()
{
    QSize size(200, 100);

    // Proporcao quadrada 1:1 -> deve gerar 100x100 centralizado (x=50, y=0)
    QRect squareCrop = ImageProcessing::calculateAspectCropRect(size, 1.0);
    QCOMPARE(squareCrop.width(), 100);
    QCOMPARE(squareCrop.height(), 100);
    QCOMPARE(squareCrop.x(), 50);
    QCOMPARE(squareCrop.y(), 0);

    // Proporcao 2:1 -> cabe perfeitamente na imagem 200x100
    QRect exactCrop = ImageProcessing::calculateAspectCropRect(size, 2.0);
    QCOMPARE(exactCrop.width(), 200);
    QCOMPARE(exactCrop.height(), 100);
}

void TestImageProcessing::testRemoveBackgroundContiguous()
{
    // Cria imagem 100x100 com fundo vermelho, retangulo central verde e circulo interno vermelho
    QImage img(100, 100, QImage::Format_ARGB32);
    img.fill(QColor(255, 0, 0)); // Fundo vermelho

    {
        QPainter p(&img);
        p.fillRect(20, 20, 60, 60, QColor(0, 255, 0)); // Ilha verde isolando o centro
        p.fillRect(40, 40, 20, 20, QColor(255, 0, 0)); // Ponto interno vermelho
    }

    // Varinha magica no ponto (0, 0) com contiguous = true
    QImage result = ImageProcessing::removeBackground(img, QPoint(0, 0), 10, true);

    // O fundo externo deve estar transparente
    QCOMPARE(result.pixelColor(0, 0).alpha(), 0);
    QCOMPARE(result.pixelColor(10, 10).alpha(), 0);

    // A barreira verde deve continuar opaca
    QCOMPARE(result.pixelColor(25, 25).alpha(), 255);
    QCOMPARE(result.pixelColor(25, 25), QColor(0, 255, 0));

    // O ponto vermelho interno estava isolado pela barreira verde, portanto NAO deve ter sido removido
    QCOMPARE(result.pixelColor(45, 45).alpha(), 255);
    QCOMPARE(result.pixelColor(45, 45), QColor(255, 0, 0));
}

void TestImageProcessing::testRemoveBackgroundNonContiguous()
{
    // Mesma imagem anterior
    QImage img(100, 100, QImage::Format_ARGB32);
    img.fill(QColor(255, 0, 0));

    {
        QPainter p(&img);
        p.fillRect(20, 20, 60, 60, QColor(0, 255, 0));
        p.fillRect(40, 40, 20, 20, QColor(255, 0, 0));
    }

    // Varinha magica com contiguous = false (remove todos os vermelhos da imagem)
    QImage result = ImageProcessing::removeBackground(img, QPoint(0, 0), 10, false);

    // Fundo externo transparente
    QCOMPARE(result.pixelColor(0, 0).alpha(), 0);
    // Ponto vermelho interno agora TAMBEM deve estar transparente
    QCOMPARE(result.pixelColor(45, 45).alpha(), 0);
    // Barreira verde permanece intacta
    QCOMPARE(result.pixelColor(25, 25).alpha(), 255);
}

void TestImageProcessing::testScissorsCutKeepInside()
{
    QImage img(100, 100, QImage::Format_ARGB32);
    img.fill(Qt::yellow);

    // Poligono triangular de corte
    QPolygonF triangle;
    triangle << QPointF(50, 10) << QPointF(90, 80) << QPointF(10, 80);

    ScissorsCutResult res = ImageProcessing::scissorsCut(img, triangle, true, true);

    QVERIFY(!res.image.isNull());
    // Caixa delimitadora do triangulo: x=10..90 (w=80), y=10..80 (h=70)
    QCOMPARE(res.offset.x(), 10);
    QCOMPARE(res.offset.y(), 10);
    QCOMPARE(res.image.width(), 80);
    QCOMPARE(res.image.height(), 70);

    // Ponto no centro do triangulo deve ser opaco
    // Centro do triangulo original (50, 55), relativo ao recorte (50-10, 55-10) = (40, 45)
    QVERIFY(res.image.pixelColor(40, 45).alpha() > 200);

    // Canto superior esquerdo da caixa delimitadora (0, 0) relativo ao recorte esta fora do triangulo
    QCOMPARE(res.image.pixelColor(0, 0).alpha(), 0);
}

void TestImageProcessing::testScissorsCutEraseInside()
{
    QImage img(100, 100, QImage::Format_ARGB32);
    img.fill(Qt::yellow);

    QPolygonF rectPoly;
    rectPoly << QPointF(30, 30) << QPointF(70, 30) << QPointF(70, 70) << QPointF(30, 70);

    ScissorsCutResult res = ImageProcessing::scissorsCut(img, rectPoly, false, false);

    QCOMPARE(res.image.size(), img.size());
    // O interior do retangulo foi apagado (transparente)
    QCOMPARE(res.image.pixelColor(50, 50).alpha(), 0);
    // O exterior permanece opaco
    QCOMPARE(res.image.pixelColor(10, 10).alpha(), 255);
    QCOMPARE(res.image.pixelColor(10, 10), QColor(Qt::yellow));
}

void TestImageProcessing::testCloneStamp()
{
    QImage src(100, 100, QImage::Format_ARGB32);
    src.fill(Qt::white);
    // Desenha uma marca vermelha na regiao de origem (20, 20)
    {
        QPainter p(&src);
        p.fillRect(15, 15, 10, 10, Qt::red);
    }

    QImage dst(100, 100, QImage::Format_ARGB32);
    dst.fill(Qt::white);

    // Clona da origem (20, 20) para o destino (80, 80) com raio 8 e dureza total (1.0)
    QImage result = ImageProcessing::cloneStamp(dst, src, QPoint(20, 20), QPoint(80, 80), 8, 1.0, 1.0);

    // O destino agora deve conter a cor vermelha clonada
    QCOMPARE(result.pixelColor(80, 80), QColor(Qt::red));
    // Areas fora do raio devem permanecer brancas
    QCOMPARE(result.pixelColor(10, 10), QColor(Qt::white));
}

void TestImageProcessing::testCloneStampHardnessAndOpacity()
{
    // Testa mistura ponderada por opacidade (50% alpha blend)
    QImage src(50, 50, QImage::Format_ARGB32);
    src.fill(QColor(255, 0, 0)); // Vermelho 255

    QImage dst(50, 50, QImage::Format_ARGB32);
    dst.fill(QColor(0, 0, 0)); // Preto 0

    // Opacidade 0.5: 255 * 0.5 + 0 * 0.5 ~= 128
    QImage result = ImageProcessing::cloneStamp(dst, src, QPoint(25, 25), QPoint(25, 25), 10, 0.5, 1.0);
    const QColor centerCol = result.pixelColor(25, 25);
    QVERIFY(std::abs(centerCol.red() - 128) <= 2);
    QCOMPARE(centerCol.green(), 0);
    QCOMPARE(centerCol.blue(), 0);
}

void TestImageProcessing::testMagicWandTolerance()
{
    // Testa limiares de tolerância de cor
    QImage img(60, 60, QImage::Format_ARGB32);
    img.fill(QColor(255, 0, 0)); // Vermelho puro (255, 0, 0)
    {
        QPainter p(&img);
        p.fillRect(20, 0, 20, 60, QColor(235, 0, 0)); // Vermelho próximo (delta R = 20)
        p.fillRect(40, 0, 20, 60, QColor(0, 0, 255));   // Azul distante
    }

    // Com tolerância baixa (2%), delta 20 NÃO deve ser removido
    QImage lowTol = ImageProcessing::removeBackground(img, QPoint(5, 30), 2, false);
    QCOMPARE(lowTol.pixelColor(5, 30).alpha(), 0);       // Vermelho puro removido
    QCOMPARE(lowTol.pixelColor(25, 30).alpha(), 255);    // Vermelho próximo preservado
    QCOMPARE(lowTol.pixelColor(45, 30).alpha(), 255);    // Azul preservado

    // Com tolerância moderada (10%), delta 20 DEVE ser removido, mas azul preservado
    QImage midTol = ImageProcessing::removeBackground(img, QPoint(5, 30), 10, false);
    QCOMPARE(midTol.pixelColor(5, 30).alpha(), 0);       // Vermelho puro removido
    QCOMPARE(midTol.pixelColor(25, 30).alpha(), 0);      // Vermelho próximo removido
    QCOMPARE(midTol.pixelColor(45, 30).alpha(), 255);    // Azul permanece intacto
}

void TestImageProcessing::testScissorsCutFreehand()
{
    // Testa polígono livre de corte com 6 vértices
    QImage img(120, 120, QImage::Format_ARGB32);
    img.fill(Qt::magenta);

    QPolygonF poly;
    poly << QPointF(20, 20) << QPointF(60, 10) << QPointF(100, 30)
         << QPointF(90, 80) << QPointF(50, 100) << QPointF(15, 70);

    ScissorsCutResult res = ImageProcessing::scissorsCut(img, poly, true, true);
    QVERIFY(!res.image.isNull());
    QCOMPARE(res.offset.x(), 15);
    QCOMPARE(res.offset.y(), 10);
    QCOMPARE(res.image.width(), 85);
    QCOMPARE(res.image.height(), 90);

    // Centro do polígono deve ser mantido opaco
    QPoint localCenter = (QPointF(60, 50) - QPointF(res.offset)).toPoint();
    QVERIFY(res.image.pixelColor(localCenter.x(), localCenter.y()).alpha() > 200);
}

void TestImageProcessing::testAssetStoreAddImage()
{
    AssetStore store;
    QImage img(64, 48, QImage::Format_ARGB32);
    img.fill(Qt::cyan);

    LayerId id = store.addImage(img, QStringLiteral("png"));
    QVERIFY(!id.isNull());

    const Asset* asset = store.find(id);
    QVERIFY(asset != nullptr);
    QCOMPARE(asset->width, 64);
    QCOMPARE(asset->height, 48);
    QVERIFY(!asset->encoded.isEmpty());

    QImage decoded = store.decodedImage(id);
    QCOMPARE(decoded.size(), QSize(64, 48));
    QCOMPARE(decoded.pixelColor(0, 0), QColor(Qt::cyan));
}

void TestImageProcessing::testModifyImageLayerCommand()
{
    Document doc(800, 600);
    CommandStack stack;

    // Adiciona uma camada de imagem
    QImage originalImg(100, 100, QImage::Format_ARGB32);
    originalImg.fill(Qt::red);
    LayerId oldAssetId = doc.assets().addImage(originalImg);

    auto layer = std::make_unique<ImageLayer>();
    layer->assetId = oldAssetId;
    layer->naturalWidth = 100;
    layer->naturalHeight = 100;
    layer->transform.position = QPointF(200, 200);
    LayerId layerId = layer->id();
    doc.addLayer(std::move(layer));

    // Novo asset (apos corte ou processamento)
    QImage newImg(50, 50, QImage::Format_ARGB32);
    newImg.fill(Qt::green);
    LayerId newAssetId = doc.assets().addImage(newImg);

    AffineTransform oldTransform;
    oldTransform.position = QPointF(200, 200);

    AffineTransform newTransform;
    newTransform.position = QPointF(225, 225);

    auto cmd = std::make_unique<ModifyImageLayerCommand>(
        doc, layerId,
        oldAssetId, 100, 100, oldTransform,
        newAssetId, 50, 50, newTransform,
        QStringLiteral("Crop")
    );

    stack.execute(std::move(cmd));

    // Verifica que aplicou o novo estado
    auto* imgLayer = static_cast<ImageLayer*>(doc.findLayer(layerId));
    QCOMPARE(imgLayer->assetId, newAssetId);
    QCOMPARE(imgLayer->naturalWidth, 50);
    QCOMPARE(imgLayer->naturalHeight, 50);
    QCOMPARE(imgLayer->transform.position, QPointF(225, 225));

    // Desfazer (Undo)
    stack.undo();
    QCOMPARE(imgLayer->assetId, oldAssetId);
    QCOMPARE(imgLayer->naturalWidth, 100);
    QCOMPARE(imgLayer->naturalHeight, 100);
    QCOMPARE(imgLayer->transform.position, QPointF(200, 200));

    // Refazer (Redo)
    stack.redo();
    QCOMPARE(imgLayer->assetId, newAssetId);
    QCOMPARE(imgLayer->naturalWidth, 50);
    QCOMPARE(imgLayer->naturalHeight, 50);
    QCOMPARE(imgLayer->transform.position, QPointF(225, 225));
}

QTEST_MAIN(TestImageProcessing)
#include "test_image_processing.moc"
