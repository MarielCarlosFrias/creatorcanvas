// Teste comportamental do hit-test com alfa:
// 1. Imagem COM buraco transparente por cima de uma forma
// 2. Clique esquerdo no buraco -> seleciona a FORMA
// 3. Menu de contexto (botão direito) no mesmo ponto -> também seleciona a FORMA
// 4. Clique na parte opaca da imagem -> seleciona a IMAGEM
#include "ui/canvasview.h"
#include "core/Document.h"
#include "rendering/CanvasRenderer.h"
#include "core/layers/Layer.h"

#include <QContextMenuEvent>
#include <QImage>
#include <QMenu>
#include <QTimer>
#include <QMouseEvent>
#include <QtTest>

using namespace cc;

class TestHitAlpha final : public QObject
{
    Q_OBJECT

private slots:
    void alphaHoleClickThrough()
    {
        CanvasView canvas;
        Document doc(200, 200, 96);

        // Forma vermelha no centro (embaixo)
        auto shape = std::make_unique<ShapeLayer>();
        shape->name = QStringLiteral("forma");
        shape->kind = ShapeKind::Rectangle;
        shape->points = QPolygonF{QPointF(-100, -100), QPointF(100, -100),
                                  QPointF(100, 100), QPointF(-100, 100)};
        shape->fill = QColor(255, 0, 0);
        shape->transform.position = QPointF(100, 100);
        const LayerId shapeId = shape->id();
        doc.addLayer(std::move(shape));

        // Imagem 100x100 por cima: opaca nas bordas, buraco transparente no centro
        QImage img(100, 100, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        for (int y = 0; y < 100; ++y) {
            for (int x = 0; x < 100; ++x) {
                const bool edge = x < 10 || x >= 90 || y < 10 || y >= 90;
                if (edge)
                    img.setPixel(x, y, qRgba(0, 200, 0, 255));
            }
        }
        const LayerId assetId = doc.assets().addImage(img);
        QVERIFY(!assetId.isNull());
        auto image = std::make_unique<ImageLayer>();
        image->name = QStringLiteral("imagem");
        image->assetId = assetId;
        image->naturalWidth = 100;
        image->naturalHeight = 100;
        image->transform.position = QPointF(100, 100);
        const LayerId imageId = image->id();
        doc.addLayer(std::move(image));

        canvas.setDocument(&doc);
        canvas.resize(600, 600);

        // docPos (100,100) = centro da imagem = PIXEL TRANSPARENTE
        const QPoint centerDevice = canvas.docToDevice().map(QPointF(100, 100)).toPoint();

        // 1. clique esquerdo no centro transparente -> forma
        QMouseEvent press(QEvent::MouseButtonPress, centerDevice,
                          centerDevice, Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QApplication::sendEvent(&canvas, &press);
        QCOMPARE(canvas.selectedLayers().first(), shapeId);

        // 2. menu de contexto no MESMO ponto -> continua na forma (nao pula pra imagem)
        canvas.setSelectedLayer(LayerId()); // limpa selecao p/ reproduzir o caso real
        // menu.exec() e modal: fecha qualquer menu aberto no proximo ciclo
        QTimer::singleShot(0, [] {
            for (QWidget* w : QApplication::topLevelWidgets())
                if (auto* m = qobject_cast<QMenu*>(w))
                    m->close();
        });
        QContextMenuEvent ctx(QContextMenuEvent::Mouse, centerDevice,
                              canvas.mapToGlobal(centerDevice));
        QApplication::sendEvent(&canvas, &ctx);
        QCOMPARE(canvas.selectedLayers().first(), shapeId);

        // 3. clique na borda OPACA da imagem (img ocupa 50..150 no doc) -> imagem
        const QPoint edgeDevice = canvas.docToDevice().map(QPointF(55, 55)).toPoint();
        QMouseEvent pressEdge(QEvent::MouseButtonPress, edgeDevice,
                              edgeDevice, Qt::LeftButton, Qt::LeftButton,
                              Qt::NoModifier);
        QApplication::sendEvent(&canvas, &pressEdge);
        QCOMPARE(canvas.selectedLayers().first(), imageId);
    }

    void contextMenuOnGroupedShapeDoesNotFallBack()
    {
        // Cenario exato reportado: imagem selecionada primeiro; forma DENTRO
        // de um grupo embaixo. Botao direito na forma tem que selecionar A
        // FORMA — nao pode cair na selecao anterior (a imagem).
        CanvasView canvas;
        Document doc(200, 200, 96);

        // Grupo com uma forma dentro
        auto group = std::make_unique<GroupLayer>();
        group->name = QStringLiteral("grupo");
        auto shape = std::make_unique<ShapeLayer>();
        shape->name = QStringLiteral("forma no grupo");
        shape->kind = ShapeKind::Rectangle;
        shape->points = QPolygonF{QPointF(-80, -80), QPointF(80, -80),
                                  QPointF(80, 80), QPointF(-80, 80)};
        shape->fill = QColor(0, 90, 255);
        shape->transform.position = QPointF(100, 100);
        const LayerId shapeId = shape->id();
        group->children.push_back(std::move(shape));
        doc.addLayer(std::move(group));

        // Imagem 100x100 opaca por cima, cobrindo parcialmente a forma
        QImage img(100, 100, QImage::Format_ARGB32);
        img.fill(qRgba(0, 200, 0, 255));
        img.setPixel(50, 50, qRgba(0, 0, 0, 0)); // 1 pixel furado no centro
        const LayerId assetId = doc.assets().addImage(img);
        auto image = std::make_unique<ImageLayer>();
        image->name = QStringLiteral("imagem");
        image->assetId = assetId;
        image->naturalWidth = 100;
        image->naturalHeight = 100;
        image->transform.position = QPointF(100, 100);
        const LayerId imageId = image->id();
        doc.addLayer(std::move(image));

        canvas.setDocument(&doc);
        canvas.resize(600, 600);

        // "Seleciono um objeto primeiro" — a imagem
        canvas.setSelectedLayer(imageId);
        QCOMPARE(canvas.selectedLayers().first(), imageId);

        // Botao direito na forma DO GRUPO (ponto visivel da forma, fora da
        // imagem: doc (160,100)) — tem que selecionar a forma, nao a imagem
        const QPoint shapeDevice = canvas.docToDevice().map(QPointF(160, 100)).toPoint();
        QTimer::singleShot(0, [] {
            for (QWidget* w : QApplication::topLevelWidgets())
                if (auto* m = qobject_cast<QMenu*>(w))
                    m->close();
        });
        QContextMenuEvent ctx(QContextMenuEvent::Mouse, shapeDevice,
                              canvas.mapToGlobal(shapeDevice));
        QApplication::sendEvent(&canvas, &ctx);
        QCOMPARE(canvas.selectedLayers().first(), shapeId);
    }

    void imageHandlesMatchRenderedExtent()
    {
        // A caixa de transformação tem de cobrir EXACTAMENTE a área
        // desenhada da imagem — se uma das duas usar um espaço de
        // coordenadas diferente, o utilizador vê a forma mudar de tamanho
        // ao arrastar (sintoma relatado: "buga a visualização do layout").
        Document doc(100, 100);
        QImage photo(40, 30, QImage::Format_ARGB32);
        photo.fill(qRgba(10, 200, 10, 255));
        const LayerId asset = doc.assets().addImage(photo, QStringLiteral("png"));
        auto layer = std::make_unique<ImageLayer>();
        layer->assetId = asset;
        layer->naturalWidth = 40;
        layer->naturalHeight = 30;
        layer->transform.position = QPointF(50, 50);
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        QImage out(100, 100, QImage::Format_ARGB32);
        out.fill(Qt::black);
        {
            QPainter p(&out);
            cc::RenderOptions opt;
            opt.drawCheckerboard = false;
            cc::renderDocument(doc, &p, QTransform(), opt);
        }
        QRect drawn;
        for (int y = 0; y < 100; ++y)
            for (int x = 0; x < 100; ++x)
                if (out.pixelColor(x, y) == QColor(10, 200, 10))
                    drawn |= QRect(x, y, 1, 1);
        QVERIFY2(!drawn.isEmpty(), "a imagem devia ser visível");
        QCOMPARE(drawn.width(), 40);
        QCOMPARE(drawn.height(), 30);

        const Layer* l = doc.findLayer(id);
        const CanvasHandles handles = CanvasGestureHandler::computeHandles(
            *l, QTransform());
        // Extensão (max - min) dos 8 handles da caixa; points[8] é o handle
        // de ROTAÇÃO, que fica acima da caixa e não conta para a extensão.
        QPointF mn(1e9, 1e9), mx(-1e9, -1e9);
        for (int i = 0; i < 8; ++i) {
            const QPointF& p = handles.points[static_cast<size_t>(i)];
            mn.setX(std::min(mn.x(), p.x()));
            mn.setY(std::min(mn.y(), p.y()));
            mx.setX(std::max(mx.x(), p.x()));
            mx.setY(std::max(mx.y(), p.y()));
        }
        QVERIFY2(mx.x() > mn.x() && mx.y() > mn.y(), "handles deviam existir");
        QCOMPARE(qRound(mx.x() - mn.x()), drawn.width());
        QCOMPARE(qRound(mx.y() - mn.y()), drawn.height());
        QCOMPARE(QPoint(qRound(mn.x()), qRound(mn.y())), drawn.topLeft());
    }

    void shapeHitUsesRealGeometry()
    {
        CanvasView canvas;
        Document doc(200, 200, 96);

        auto under = std::make_unique<ShapeLayer>();
        under->kind = ShapeKind::Rectangle;
        under->points = QPolygonF{QPointF(-90, -90), QPointF(90, -90),
                                  QPointF(90, 90), QPointF(-90, 90)};
        under->fill = QColor(Qt::red);
        under->transform.position = QPointF(100, 100);
        const LayerId underId = under->id();
        doc.addLayer(std::move(under));

        auto ellipse = std::make_unique<ShapeLayer>();
        ellipse->kind = ShapeKind::Ellipse;
        ellipse->points = QPolygonF{QPointF(-60, -40), QPointF(60, -40),
                                    QPointF(60, 40), QPointF(-60, 40)};
        ellipse->fill = QColor(Qt::blue);
        ellipse->transform.position = QPointF(100, 100);
        const LayerId ellipseId = ellipse->id();
        doc.addLayer(std::move(ellipse));

        canvas.setDocument(&doc);
        canvas.resize(600, 600);
        const QPoint outsideEllipse = canvas.docToDevice().map(QPointF(45, 45)).toPoint();
        QMouseEvent press(QEvent::MouseButtonPress, outsideEllipse, outsideEllipse,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &press);
        QCOMPARE(canvas.selectedLayers().first(), underId);

        const QPoint insideEllipse = canvas.docToDevice().map(QPointF(100, 100)).toPoint();
        QMouseEvent press2(QEvent::MouseButtonPress, insideEllipse, insideEllipse,
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &press2);
        QCOMPARE(canvas.selectedLayers().first(), ellipseId);
    }

    void groupedTransformIsIncludedInHitTest()
    {
        CanvasView canvas;
        Document doc(300, 200, 96);
        auto group = std::make_unique<GroupLayer>();
        group->transform.position = QPointF(220, 100);
        auto shape = std::make_unique<ShapeLayer>();
        shape->kind = ShapeKind::Rectangle;
        shape->points = QPolygonF{QPointF(-30, -20), QPointF(30, -20),
                                  QPointF(30, 20), QPointF(-30, 20)};
        shape->fill = QColor(Qt::green);
        const LayerId shapeId = shape->id();
        group->children.push_back(std::move(shape));
        doc.addLayer(std::move(group));

        canvas.setDocument(&doc);
        canvas.resize(600, 400);
        const QPoint device = canvas.docToDevice().map(QPointF(220, 100)).toPoint();
        QMouseEvent press(QEvent::MouseButtonPress, device, device,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &press);
        QCOMPARE(canvas.selectedLayers().first(), shapeId);
    }

    void nestedGroupHitUsesLayerThenParentOrder()
    {
        CanvasView canvas;
        Document doc(400, 300, 96);
        auto outer = std::make_unique<GroupLayer>();
        outer->transform.position = QPointF(230, 140);
        outer->transform.rotationDeg = 73.0;
        outer->transform.scaleY = 0.8;
        auto inner = std::make_unique<GroupLayer>();
        inner->transform.position = QPointF(70, 50);
        inner->transform.rotationDeg = -41.0;
        inner->transform.shearX = 0.15;
        auto shape = std::make_unique<ShapeLayer>();
        shape->kind = ShapeKind::Rectangle;
        shape->points = QPolygonF{QPointF(-20, -10), QPointF(20, -10),
                                  QPointF(20, 10), QPointF(-20, 10)};
        shape->fill = QColor(Qt::yellow);
        shape->transform.position = QPointF(35, 25);
        shape->transform.rotationDeg = 27.0;
        shape->transform.scaleX = 1.3;
        const LayerId shapeId = shape->id();
        GroupLayer* innerPtr = inner.get();
        GroupLayer* outerPtr = outer.get();
        inner->children.push_back(std::move(shape));
        outer->children.push_back(std::move(inner));
        QVERIFY(doc.addLayer(std::move(outer)));

        const Layer* layer = doc.findLayer(shapeId);
        const QRectF bounds = layer->contentBounds();
        const QTransform shapeMatrix = layer->transform.matrix(bounds);
        const QTransform innerMatrix = innerPtr->transform.matrix(innerPtr->contentBounds());
        const QTransform outerMatrix = outerPtr->transform.matrix(outerPtr->contentBounds());
        const QTransform expected = shapeMatrix * innerMatrix * outerMatrix;
        const QPointF localSample(4, -3);
        const QPointF docSample = expected.map(localSample);
        const QPointF wrongOrderSample = (outerMatrix * innerMatrix * shapeMatrix).map(localSample);
        QVERIFY((docSample - wrongOrderSample).manhattanLength() > 10.0);

        canvas.setDocument(&doc);
        canvas.resize(800, 600);
        const QPoint device = canvas.docToDevice().map(docSample).toPoint();
        QMouseEvent press(QEvent::MouseButtonPress, device, device,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &press);
        QCOMPARE(canvas.selectedLayers().first(), shapeId);
    }
};

QTEST_MAIN(TestHitAlpha)
#include "test_hit_alpha.moc"
