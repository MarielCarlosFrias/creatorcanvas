#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "core/layers/Layer.h"
#include "core/serialization/ProjectFile.h"

using namespace cc;

class TestCanvasOps : public QObject
{
    Q_OBJECT

private slots:
    void setCanvasSizeBasic();
    void setCanvasSizeWithAnchorOffset();
    void resizeCommandUndoRedo();
    void rotateCanvas90();
    void rotateCanvas180();
    void rotateCommandUndoRedo();
    void rotateIgnoresGroupDoubleRotation();
    void guidesRoundtrip();
    void resizeTransformsAuxiliaryStateWithUndoRedo();
    void rotateTransformsAuxiliaryStateWithUndoRedo();
    void rotateGuidesQuarterTurns_data();
    void rotateGuidesQuarterTurns();
};

static std::unique_ptr<ShapeLayer> makeRect(const QString& name, double x, double y)
{
    auto layer = std::make_unique<ShapeLayer>();
    layer->name = name;
    layer->kind = ShapeKind::Rectangle;
    layer->points = QPolygonF({QPointF(0, 0), QPointF(40, 20), QPointF(40, 20), QPointF(0, 20)});
    layer->transform.position = QPointF(x, y);
    return layer;
}

static WebSlice makeSlice(const QRectF& rect)
{
    WebSlice slice;
    slice.name = QStringLiteral("Slice");
    slice.rect = rect;
    return slice;
}

void TestCanvasOps::setCanvasSizeBasic()
{
    Document doc(800, 600);
    QSignalSpy spy(&doc, &Document::canvasSizeChanged);

    auto layer = makeRect("L", 100, 100);
    const LayerId id = layer->id();
    const QPointF oldPos = layer->transform.position;
    doc.addLayer(std::move(layer));

    QVERIFY(doc.setCanvasSize(1024, 768));
    QCOMPARE(doc.width(), 1024);
    QCOMPARE(doc.height(), 768);
    QCOMPARE(spy.count(), 1);
    // Sem offset, a posição da camada não muda.
    QCOMPARE(doc.findLayer(id)->transform.position, oldPos);

    // Tamanho igual sem offset = no-op (sem sinal).
    QVERIFY(!doc.setCanvasSize(1024, 768));
    QCOMPARE(spy.count(), 1);
}

void TestCanvasOps::setCanvasSizeWithAnchorOffset()
{
    Document doc(800, 600);
    auto layer = makeRect("L", 100, 100);
    const LayerId id = layer->id();
    doc.addLayer(std::move(layer));

    // Âncora canto inferior direito: dobra o canvas para cima/esquerda,
    // logo as camadas deslocam-se (+w, +h).
    QVERIFY(doc.setCanvasSize(1600, 1200, QPointF(800, 600)));
    QCOMPARE(doc.findLayer(id)->transform.position, QPointF(900, 700));
}

void TestCanvasOps::resizeCommandUndoRedo()
{
    Document doc(800, 600);
    CommandStack stack;
    auto layer = makeRect("L", 50, 60);
    const LayerId id = layer->id();
    doc.addLayer(std::move(layer));

    stack.execute(std::make_unique<ResizeCanvasCommand>(doc, 400, 300, QPointF(-200, -150)));
    QCOMPARE(doc.width(), 400);
    QCOMPARE(doc.height(), 300);
    QCOMPARE(doc.findLayer(id)->transform.position, QPointF(-150, -90));

    stack.undo();
    QCOMPARE(doc.width(), 800);
    QCOMPARE(doc.height(), 600);
    QCOMPARE(doc.findLayer(id)->transform.position, QPointF(50, 60));

    stack.redo();
    QCOMPARE(doc.width(), 400);
    QCOMPARE(doc.findLayer(id)->transform.position, QPointF(-150, -90));
}

void TestCanvasOps::rotateCanvas90()
{
    Document doc(800, 600);
    QSignalSpy spy(&doc, &Document::canvasSizeChanged);

    // Camada no canto superior esquerdo do canvas (centro do conteúdo em 100,100).
    auto layer = makeRect("L", 100, 100);
    const LayerId id = layer->id();
    doc.addLayer(std::move(layer));

    QVERIFY(doc.rotateCanvas(90.0)); // CW
    QCOMPARE(doc.width(), 600);
    QCOMPARE(doc.height(), 800);
    QCOMPARE(spy.count(), 1);

    // p' = R90(p à volta de C antigo) + (C' - C), com C=(400,300), C'=(300,400).
    QTransform expected;
    expected.translate(400.0, 300.0);
    expected.rotate(90.0);
    expected.translate(-400.0, -300.0);
    const QPointF expectedPos = expected.map(QPointF(100, 100)) + QPointF(-100, 100);
    const QPointF pos = doc.findLayer(id)->transform.position;
    QVERIFY(qAbs(pos.x() - expectedPos.x()) < 1e-6);
    QVERIFY(qAbs(pos.y() - expectedPos.y()) < 1e-6);
    QVERIFY(qAbs(doc.findLayer(id)->transform.rotationDeg - 90.0) < 1e-9);

    // Rotação inválida (0°) é no-op.
    QVERIFY(!doc.rotateCanvas(0.0));
}

void TestCanvasOps::rotateCanvas180()
{
    Document doc(800, 600);
    auto layer = makeRect("L", 100, 100);
    const LayerId id = layer->id();
    doc.addLayer(std::move(layer));

    QVERIFY(doc.rotateCanvas(180.0));
    QCOMPARE(doc.width(), 800);  // 180° não troca W/H
    QCOMPARE(doc.height(), 600);
    // p' = 2C - p = (800-100, 600-100) = (700, 500)
    const QPointF pos = doc.findLayer(id)->transform.position;
    QVERIFY(qAbs(pos.x() - 700.0) < 1e-6);
    QVERIFY(qAbs(pos.y() - 500.0) < 1e-6);
    QVERIFY(qAbs(doc.findLayer(id)->transform.rotationDeg - 180.0) < 1e-9);
}

void TestCanvasOps::rotateCommandUndoRedo()
{
    Document doc(800, 600);
    CommandStack stack;
    auto layer = makeRect("L", 100, 100);
    const LayerId id = layer->id();
    doc.addLayer(std::move(layer));
    const AffineTransform original = doc.findLayer(id)->transform;

    stack.execute(std::make_unique<RotateCanvasCommand>(doc, 90.0));
    QCOMPARE(doc.width(), 600);
    QCOMPARE(doc.height(), 800);
    QVERIFY(doc.findLayer(id)->transform != original);

    stack.undo();
    QCOMPARE(doc.width(), 800);
    QCOMPARE(doc.height(), 600);
    QCOMPARE(doc.findLayer(id)->transform.position, original.position);
    QCOMPARE(doc.findLayer(id)->transform.rotationDeg, original.rotationDeg);

    stack.redo();
    QCOMPARE(doc.width(), 600);
    QCOMPARE(doc.height(), 800);
}

void TestCanvasOps::rotateIgnoresGroupDoubleRotation()
{
    // Uma camada dentro de um grupo deve rodar apenas 1× (a transform do
    // grupo propaga-se; rodar as duas rodaria 2×).
    Document doc(800, 600);
    auto group = std::make_unique<GroupLayer>();
    group->name = QStringLiteral("G");
    group->transform.position = QPointF(400, 300);
    auto child = makeRect("C", 100, 100);
    const LayerId childId = child->id();
    group->children.push_back(std::move(child));
    doc.addLayer(std::move(group));

    QVERIFY(doc.rotateCanvas(90.0));
    // O filho mantém a transform local (só o grupo roda).
    const Layer* c = doc.findLayer(childId);
    QVERIFY(c != nullptr);
    QCOMPARE(c->transform.position, QPointF(100, 100));
    QCOMPARE(c->transform.rotationDeg, 0.0);
    // O grupo rodou 90° à volta do centro do canvas.
    const Layer* g = doc.rootGroup()->children.front().get();
    QVERIFY(qAbs(g->transform.rotationDeg - 90.0) < 1e-9);
}

void TestCanvasOps::guidesRoundtrip()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("guides.creatorcanvas"));

    Document doc(300, 200);
    doc.addGuide(Guide{true, 50.0});
    doc.addGuide(Guide{false, 120.5});
    QCOMPARE(doc.guides().size(), size_t(2));

    QString err;
    QImage thumb(16, 16, QImage::Format_ARGB32);
    thumb.fill(Qt::white);
    QVERIFY2(cc::saveDocument(doc, path, &err, &thumb), qPrintable(err));

    auto reloaded = cc::loadDocument(path, &err);
    QVERIFY2(reloaded != nullptr, qPrintable(err));
    QCOMPARE(reloaded->guides().size(), size_t(2));
    QCOMPARE(reloaded->guides()[0], (Guide{true, 50.0}));
    QCOMPARE(reloaded->guides()[1], (Guide{false, 120.5}));

    // clearGuides também persiste (array vazio → chave omitida)
    reloaded->clearGuides();
    QVERIFY(cc::saveDocument(*reloaded, path, &err, &thumb));
    auto again = cc::loadDocument(path, &err);
    QVERIFY(again != nullptr);
    QVERIFY(again->guides().empty());
}

void TestCanvasOps::resizeTransformsAuxiliaryStateWithUndoRedo()
{
    Document doc(4, 3);

    QImage previous(4, 3, QImage::Format_Alpha8);
    previous.fill(7);
    previous.scanLine(2)[3] = 123;
    doc.setPixelSelection(previous);

    QImage active(4, 3, QImage::Format_Alpha8);
    active.fill(0);
    active.scanLine(0)[0] = 255; // será cortado e tem de voltar no undo
    active.scanLine(1)[1] = 200;
    active.scanLine(2)[3] = 90;
    doc.setPixelSelection(active);

    QImage savedDocumentMask(4, 3, QImage::Format_Alpha8);
    savedDocumentMask.fill(0);
    savedDocumentMask.scanLine(1)[2] = 111;
    const QUuid documentId = doc.addSavedSelection(QStringLiteral("Documento"),
                                                   savedDocumentMask);
    QImage savedLayerMask(2, 1, QImage::Format_Alpha8);
    savedLayerMask.fill(0);
    savedLayerMask.scanLine(0)[1] = 201;
    const QUuid layerId = doc.addSavedSelection(QStringLiteral("Camada"),
                                                savedLayerMask);

    doc.addGuide(Guide{true, 0.5}); // fica negativa e deve persistir
    doc.addGuide(Guide{false, 3.0});
    doc.addWebSlice(makeSlice(QRectF(0, 0, 2, 2)));
    doc.addWebSlice(makeSlice(QRectF(0, 2, 1, 1))); // sai totalmente no crop
    const auto originalGuides = doc.guides();
    const auto originalSlices = doc.webSlices();

    CommandStack stack;
    stack.execute(std::make_unique<ResizeCanvasCommand>(
        doc, 3, 2, QPointF(-1, -1)));

    QImage expectedActive(3, 2, QImage::Format_Alpha8);
    expectedActive.fill(0);
    expectedActive.scanLine(0)[0] = 200;
    expectedActive.scanLine(1)[2] = 90;
    QVERIFY(doc.pixelSelection() == expectedActive);
    QCOMPARE(doc.pixelSelectionSpace(), SelectionSpace::Document);

    QImage expectedPrevious(3, 2, QImage::Format_Alpha8);
    expectedPrevious.fill(7);
    expectedPrevious.scanLine(1)[2] = 123;
    QVERIFY(doc.previousPixelSelection() == expectedPrevious);

    QImage expectedSavedDocument(3, 2, QImage::Format_Alpha8);
    expectedSavedDocument.fill(0);
    expectedSavedDocument.scanLine(0)[1] = 111;
    QVERIFY(doc.findSavedSelection(documentId)->mask == expectedSavedDocument);
    QCOMPARE(doc.findSavedSelection(documentId)->space, SelectionSpace::Document);
    QVERIFY(doc.findSavedSelection(layerId)->mask == savedLayerMask);
    QCOMPARE(doc.findSavedSelection(layerId)->space, SelectionSpace::Layer);

    QCOMPARE(doc.guides().size(), size_t(2));
    QCOMPARE(doc.guides()[0], (Guide{true, -0.5}));
    QCOMPARE(doc.guides()[1], (Guide{false, 2.0}));
    QCOMPARE(doc.webSlices().size(), size_t(1));
    QCOMPARE(doc.webSlices()[0].rect, QRectF(0, 0, 1, 1));

    stack.undo();
    QCOMPARE(doc.width(), 4);
    QCOMPARE(doc.height(), 3);
    QVERIFY(doc.pixelSelection() == active);
    QVERIFY(doc.previousPixelSelection() == previous);
    QVERIFY(doc.findSavedSelection(documentId)->mask == savedDocumentMask);
    QVERIFY(doc.findSavedSelection(layerId)->mask == savedLayerMask);
    QVERIFY(doc.guides() == originalGuides);
    QVERIFY(doc.webSlices() == originalSlices);

    stack.redo();
    QVERIFY(doc.pixelSelection() == expectedActive);
    QVERIFY(doc.previousPixelSelection() == expectedPrevious);
    QVERIFY(doc.findSavedSelection(documentId)->mask == expectedSavedDocument);
    QVERIFY(doc.findSavedSelection(layerId)->mask == savedLayerMask);
    QVERIFY(doc.webSlices().size() == 1);

    // O estado transformado (não apenas as dimensões) tem de sobreviver ao
    // round-trip e manter o espaço de cada canal.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("resized-state.creatorcanvas"));
    QString error;
    QVERIFY2(cc::saveDocument(doc, path, &error), qPrintable(error));
    auto loaded = cc::loadDocument(path, &error);
    QVERIFY2(loaded != nullptr, qPrintable(error));
    QVERIFY(loaded->pixelSelection() == expectedActive);
    QCOMPARE(loaded->pixelSelectionSpace(), SelectionSpace::Document);
    QVERIFY(loaded->findSavedSelection(documentId)->mask == expectedSavedDocument);
    QCOMPARE(loaded->findSavedSelection(documentId)->space, SelectionSpace::Document);
    QVERIFY(loaded->findSavedSelection(layerId)->mask == savedLayerMask);
    QCOMPARE(loaded->findSavedSelection(layerId)->space, SelectionSpace::Layer);
    QVERIFY(loaded->guides() == doc.guides());
    QVERIFY(loaded->webSlices() == doc.webSlices());
}

void TestCanvasOps::rotateTransformsAuxiliaryStateWithUndoRedo()
{
    Document doc(4, 3);
    QImage previous(4, 3, QImage::Format_Alpha8);
    previous.fill(7);
    previous.scanLine(2)[0] = 50;
    doc.setPixelSelection(previous);

    QImage active(4, 3, QImage::Format_Alpha8);
    active.fill(0);
    active.scanLine(0)[0] = 255;
    active.scanLine(1)[2] = 64; // cobertura parcial não pode ser binarizada
    doc.setPixelSelection(active);

    QImage savedDocumentMask(4, 3, QImage::Format_Alpha8);
    savedDocumentMask.fill(0);
    savedDocumentMask.scanLine(2)[3] = 99;
    const QUuid documentId = doc.addSavedSelection(QStringLiteral("Documento"),
                                                   savedDocumentMask);
    QImage savedLayerMask(2, 1, QImage::Format_Alpha8);
    savedLayerMask.fill(0);
    savedLayerMask.scanLine(0)[1] = 201;
    const QUuid layerId = doc.addSavedSelection(QStringLiteral("Camada"),
                                                savedLayerMask);
    doc.addGuide(Guide{true, 1.0});
    doc.addGuide(Guide{false, 1.0});
    doc.addWebSlice(makeSlice(QRectF(0, 0, 1, 2)));
    const auto originalGuides = doc.guides();
    const auto originalSlices = doc.webSlices();

    CommandStack stack;
    stack.execute(std::make_unique<RotateCanvasCommand>(doc, 90.0));
    QCOMPARE(doc.width(), 3);
    QCOMPARE(doc.height(), 4);

    QImage expectedActive(3, 4, QImage::Format_Alpha8);
    expectedActive.fill(0);
    expectedActive.scanLine(0)[2] = 255;
    expectedActive.scanLine(2)[1] = 64;
    QVERIFY(doc.pixelSelection() == expectedActive);

    QImage expectedPrevious(3, 4, QImage::Format_Alpha8);
    expectedPrevious.fill(7);
    expectedPrevious.scanLine(0)[0] = 50;
    QVERIFY(doc.previousPixelSelection() == expectedPrevious);

    QImage expectedSavedDocument(3, 4, QImage::Format_Alpha8);
    expectedSavedDocument.fill(0);
    expectedSavedDocument.scanLine(3)[0] = 99;
    QVERIFY(doc.findSavedSelection(documentId)->mask == expectedSavedDocument);
    QVERIFY(doc.findSavedSelection(layerId)->mask == savedLayerMask);

    QCOMPARE(doc.guides()[0], (Guide{false, 2.0}));
    QCOMPARE(doc.guides()[1], (Guide{true, 1.0}));
    QCOMPARE(doc.webSlices()[0].rect, QRectF(1, 0, 2, 1));

    stack.undo();
    QCOMPARE(doc.width(), 4);
    QCOMPARE(doc.height(), 3);
    QVERIFY(doc.pixelSelection() == active);
    QVERIFY(doc.previousPixelSelection() == previous);
    QVERIFY(doc.findSavedSelection(documentId)->mask == savedDocumentMask);
    QVERIFY(doc.findSavedSelection(layerId)->mask == savedLayerMask);
    QVERIFY(doc.guides() == originalGuides);
    QVERIFY(doc.webSlices() == originalSlices);

    stack.redo();
    QVERIFY(doc.pixelSelection() == expectedActive);
    QCOMPARE(doc.guides()[0], (Guide{false, 2.0}));
    QCOMPARE(doc.guides()[1], (Guide{true, 1.0}));
    QCOMPARE(doc.webSlices()[0].rect, QRectF(1, 0, 2, 1));

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("rotated-state.creatorcanvas"));
    QString error;
    QVERIFY2(cc::saveDocument(doc, path, &error), qPrintable(error));
    auto loaded = cc::loadDocument(path, &error);
    QVERIFY2(loaded != nullptr, qPrintable(error));
    QVERIFY(loaded->pixelSelection() == expectedActive);
    QCOMPARE(loaded->pixelSelectionSpace(), SelectionSpace::Document);
    QVERIFY(loaded->guides() == doc.guides());
    QVERIFY(loaded->webSlices() == doc.webSlices());
}

void TestCanvasOps::rotateGuidesQuarterTurns_data()
{
    QTest::addColumn<double>("degrees");
    QTest::addColumn<bool>("firstHorizontal");
    QTest::addColumn<double>("firstPosition");
    QTest::addColumn<bool>("secondHorizontal");
    QTest::addColumn<double>("secondPosition");

    QTest::newRow("ccw90") << -90.0 << false << 1.0 << true << 3.0;
    QTest::newRow("180") << 180.0 << true << 2.0 << false << 3.0;
    QTest::newRow("270-normalized") << 270.0 << false << 1.0 << true << 3.0;
}

void TestCanvasOps::rotateGuidesQuarterTurns()
{
    QFETCH(double, degrees);
    QFETCH(bool, firstHorizontal);
    QFETCH(double, firstPosition);
    QFETCH(bool, secondHorizontal);
    QFETCH(double, secondPosition);

    Document doc(4, 3);
    doc.addGuide(Guide{true, 1.0});
    doc.addGuide(Guide{false, 1.0});
    QVERIFY(doc.rotateCanvas(degrees));
    QCOMPARE(doc.guides()[0], (Guide{firstHorizontal, firstPosition}));
    QCOMPARE(doc.guides()[1], (Guide{secondHorizontal, secondPosition}));
}

QTEST_GUILESS_MAIN(TestCanvasOps)
#include "test_canvas_ops.moc"
