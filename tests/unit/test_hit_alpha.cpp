// Teste comportamental do hit-test com alfa:
// 1. Imagem COM buraco transparente por cima de uma forma
// 2. Clique esquerdo no buraco -> seleciona a FORMA
// 3. Menu de contexto (botão direito) no mesmo ponto -> também seleciona a FORMA
// 4. Clique na parte opaca da imagem -> seleciona a IMAGEM
#include "ui/canvasview.h"
#include "core/Document.h"
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
};

QTEST_MAIN(TestHitAlpha)
#include "test_hit_alpha.moc"
