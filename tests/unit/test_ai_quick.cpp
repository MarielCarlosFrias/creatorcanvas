#include <QTest>
#include <QSignalSpy>
#include <QImage>
#include <QPainter>
#include <QPointer>
#include <atomic>
#include <memory>

#include "core/Document.h"
#include "core/document/NewDocumentSpec.h"
#include "core/image/BackgroundRemover.h"
#include "ui/canvasview.h"

using namespace cc;

class TestAiQuick final : public QObject
{
    Q_OBJECT

private slots:
    void testRemoverCancellationPreSet();
    void testCanvasViewDestructionDuringQuickAi();
    void testCanvasViewConsecutiveCalls();
    void testResultAppliedOnlyToTargetLayer();
};

void TestAiQuick::testRemoverCancellationPreSet()
{
    QImage testImg(128, 128, QImage::Format_ARGB32_Premultiplied);
    testImg.fill(Qt::red);

    std::atomic<bool> cancelFlag(true);
    BackgroundRemover remover;
    QImage result = remover.removeBackground(testImg, &cancelFlag);

    // Deve abortar imediatamente e retornar imagem nula
    QVERIFY(result.isNull());
}

void TestAiQuick::testCanvasViewDestructionDuringQuickAi()
{
    NewDocumentSpec spec;
    spec.width = 400;
    spec.height = 400;
    spec.transparentBackground = true;
    auto doc = createDocument(spec);

    QImage img(64, 64, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::blue);
    LayerId assetId = doc->assets().addImage(img);

    auto imgLayer = std::make_unique<ImageLayer>();
    imgLayer->assetId = assetId;
    imgLayer->naturalWidth = 64;
    imgLayer->naturalHeight = 64;
    LayerId layerId = imgLayer->id();
    doc->addLayer(std::move(imgLayer));

    // Instancia CanvasView dinamicamente
    auto* view = new CanvasView();
    view->setDocument(doc.get());

    // Dispara a remoção rápida
    view->removeBackgroundAiQuick(layerId);
    QVERIFY(view->isQuickAiRunning());

    // Deleta o CanvasView imediatamente com a thread ainda em execução
    delete view;

    // Se chegou aqui sem falha de segmentação (use-after-free) ou assert, o teste passou
    QVERIFY(true);
}

void TestAiQuick::testCanvasViewConsecutiveCalls()
{
    NewDocumentSpec spec;
    spec.width = 400;
    spec.height = 400;
    spec.transparentBackground = true;
    auto doc = createDocument(spec);

    QImage img1(64, 64, QImage::Format_ARGB32_Premultiplied);
    img1.fill(Qt::yellow);
    LayerId asset1 = doc->assets().addImage(img1);

    auto layer1 = std::make_unique<ImageLayer>();
    layer1->assetId = asset1;
    layer1->naturalWidth = 64;
    layer1->naturalHeight = 64;
    LayerId id1 = layer1->id();
    doc->addLayer(std::move(layer1));

    QImage img2(64, 64, QImage::Format_ARGB32_Premultiplied);
    img2.fill(Qt::cyan);
    LayerId asset2 = doc->assets().addImage(img2);

    auto layer2 = std::make_unique<ImageLayer>();
    layer2->assetId = asset2;
    layer2->naturalWidth = 64;
    layer2->naturalHeight = 64;
    LayerId id2 = layer2->id();
    doc->addLayer(std::move(layer2));

    CanvasView view;
    view.setDocument(doc.get());

    QSignalSpy busySpy(&view, &CanvasView::quickAiBusyChanged);

    // Dispara a primeira chamada para a camada 1
    view.removeBackgroundAiQuick(id1);
    QVERIFY(view.isQuickAiRunning());

    // Imediatamente dispara a segunda chamada para a camada 2
    // Não deve travar a UI e deve agendar a camada 2
    view.removeBackgroundAiQuick(id2);
    QVERIFY(view.isQuickAiRunning());

    // Aguarda conclusão de todo o processamento de background
    QTRY_VERIFY_WITH_TIMEOUT(!view.isQuickAiRunning(), 10000);

    QVERIFY(busySpy.count() >= 1);
}

void TestAiQuick::testResultAppliedOnlyToTargetLayer()
{
    NewDocumentSpec spec;
    spec.width = 400;
    spec.height = 400;
    spec.transparentBackground = true;
    auto doc = createDocument(spec);

    QImage img1(64, 64, QImage::Format_ARGB32_Premultiplied);
    img1.fill(Qt::green);
    LayerId asset1 = doc->assets().addImage(img1);

    auto layer1 = std::make_unique<ImageLayer>();
    layer1->name = QStringLiteral("Layer1");
    layer1->assetId = asset1;
    layer1->naturalWidth = 64;
    layer1->naturalHeight = 64;
    LayerId id1 = layer1->id();
    doc->addLayer(std::move(layer1));

    QImage img2(48, 48, QImage::Format_ARGB32_Premultiplied);
    img2.fill(Qt::magenta);
    LayerId asset2 = doc->assets().addImage(img2);

    auto layer2 = std::make_unique<ImageLayer>();
    layer2->name = QStringLiteral("Layer2");
    layer2->assetId = asset2;
    layer2->naturalWidth = 48;
    layer2->naturalHeight = 48;
    LayerId id2 = layer2->id();
    doc->addLayer(std::move(layer2));

    CanvasView view;
    view.setDocument(doc.get());

    // Conecta sinal de modificação para atualizar o Document como o MainWindow faz
    connect(&view, &CanvasView::imageLayerModified,
            [&doc](const LayerId& targetId,
                   const LayerId&, int, int, const AffineTransform&,
                   const LayerId& newAssetId, int newW, int newH, const AffineTransform& newTransform,
                   const QString&) {
        Layer* l = doc->findLayer(targetId);
        if (l && l->type() == LayerType::Image) {
            auto* imgL = static_cast<ImageLayer*>(l);
            imgL->assetId = newAssetId;
            imgL->naturalWidth = newW;
            imgL->naturalHeight = newH;
            imgL->transform = newTransform;
        }
    });

    // Dispara remoção rápida apenas na Layer 1
    view.removeBackgroundAiQuick(id1);

    // Aguarda finalização
    QTRY_VERIFY_WITH_TIMEOUT(!view.isQuickAiRunning(), 10000);

    // Verifica que Layer 2 permaneceu estritamente intacta
    Layer* l2 = doc->findLayer(id2);
    QVERIFY(l2 != nullptr);
    auto* imgL2 = static_cast<ImageLayer*>(l2);
    QCOMPARE(imgL2->assetId, asset2);
    QCOMPARE(imgL2->naturalWidth, 48);
    QCOMPARE(imgL2->naturalHeight, 48);
    QCOMPARE(imgL2->name, QStringLiteral("Layer2"));
}

QTEST_MAIN(TestAiQuick)
#include "test_ai_quick.moc"
