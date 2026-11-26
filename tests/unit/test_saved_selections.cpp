#include <QtTest>
#include <QTemporaryDir>

#include "core/Document.h"
#include "core/commands/SelectionCommands.h"
#include "core/history/CommandStack.h"
#include "core/layers/Layer.h"
#include "core/selection/SelectionOperations.h"
#include "core/serialization/ProjectFile.h"

using namespace cc;

class TestSavedSelections : public QObject
{
    Q_OBJECT

private slots:
    void previousSelectionTracked();
    void reselectThroughHistory();
    void savedSelectionsCrud();
    void defaultEmpty();
    void savedSelectionsRoundtrip();
    void layerSpaceSelectionsRoundtrip();
    void explicitLayerSpaceWithCanvasSizedMask();
    void historyAndInvertPreserveLayerSpace();
    void reselectRestoresPreviousLayerSpace();
    void layerMaskMappingUsesRendererTransformOrder();
    void layerMaskMappingHonorsExplicitSpace();
    void inconsistentDocumentSelectionSaveFails();
};

void TestSavedSelections::previousSelectionTracked()
{
    Document doc(100, 100);
    CommandStack history;

    QImage a(100, 100, QImage::Format_Alpha8);
    a.fill(255);
    QImage b(100, 100, QImage::Format_Alpha8);
    b.fill(128);

    QVERIFY(doc.previousPixelSelection().isNull());

    executeSetPixelSelection(doc, &history, a, QStringLiteral("test.a"));
    QCOMPARE(doc.pixelSelection().size(), QSize(100, 100));
    QVERIFY(doc.previousPixelSelection().isNull()); // a primeira não guarda anterior

    executeSetPixelSelection(doc, &history, b, QStringLiteral("test.b"));
    QVERIFY(!doc.previousPixelSelection().isNull());
    QCOMPARE(doc.previousPixelSelection().constScanLine(0)[0], uchar(255)); // era a

    executeSetPixelSelection(doc, &history, QImage(), QStringLiteral("test.clear"));
    QVERIFY(doc.pixelSelection().isNull());
    QVERIFY(!doc.previousPixelSelection().isNull());
    QCOMPARE(doc.previousPixelSelection().constScanLine(0)[0], uchar(128)); // era b
}

void TestSavedSelections::reselectThroughHistory()
{
    Document doc(64, 64);
    CommandStack history;

    QImage a(64, 64, QImage::Format_Alpha8);
    a.fill(255);
    executeSetPixelSelection(doc, &history, a, QStringLiteral("test.a"));
    executeSetPixelSelection(doc, &history, QImage(), QStringLiteral("test.clear"));
    QVERIFY(doc.pixelSelection().isNull());

    // Reselect restaura a anterior como um passo de histórico normal.
    const QImage prev = doc.previousPixelSelection();
    QVERIFY(!prev.isNull());
    executeSetPixelSelection(doc, &history, prev, QStringLiteral("selection.reselect"));
    QVERIFY(!doc.pixelSelection().isNull());
    QCOMPARE(doc.pixelSelection().constScanLine(0)[0], uchar(255));

    history.undo(); // desfaz o reselect -> volta a null
    QVERIFY(doc.pixelSelection().isNull());
    history.redo();
    QVERIFY(!doc.pixelSelection().isNull());
}

void TestSavedSelections::savedSelectionsCrud()
{
    Document doc(80, 60);
    QImage mask(80, 60, QImage::Format_ARGB32);
    mask.fill(Qt::white);

    const QUuid id = doc.addSavedSelection(QStringLiteral("S1"), mask);
    QVERIFY(!id.isNull());
    QCOMPARE(doc.savedSelections().size(), size_t(1));

    const SavedSelection* found = doc.findSavedSelection(id);
    QVERIFY(found != nullptr);
    QCOMPARE(found->name, QStringLiteral("S1"));
    QCOMPARE(found->mask.format(), QImage::Format_Alpha8); // normalizada
    QCOMPARE(found->mask.size(), QSize(80, 60));

    // Nome vazio recebe padrão.
    const QUuid id2 = doc.addSavedSelection(QString(), mask);
    QCOMPARE(doc.findSavedSelection(id2)->name, QStringLiteral("Selection"));
    QCOMPARE(doc.savedSelections().size(), size_t(2));

    QVERIFY(doc.removeSavedSelection(id));
    QVERIFY(!doc.removeSavedSelection(id)); // já não existe
    QCOMPARE(doc.savedSelections().size(), size_t(1));

    doc.clearSavedSelections();
    QVERIFY(doc.savedSelections().empty());
    QVERIFY(doc.findSavedSelection(id2) == nullptr);
}

void TestSavedSelections::defaultEmpty()
{
    Document doc(10, 10);
    QVERIFY(doc.savedSelections().empty());
    QVERIFY(doc.previousPixelSelection().isNull());
    QVERIFY(doc.pixelSelection().isNull());
}

void TestSavedSelections::savedSelectionsRoundtrip()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("sel_roundtrip.creatorcanvas"));

    Document doc(120, 90);
    QImage mask(120, 90, QImage::Format_ARGB32);
    mask.fill(Qt::transparent); // alfa 0 = não selecionado
    // metade esquerda selecionada (alfa 255)
    for (int y = 0; y < 90; ++y)
        for (int x = 0; x < 60; ++x)
            mask.setPixelColor(x, y, QColor(255, 255, 255, 255));
    const QUuid id = doc.addSavedSelection(QStringLiteral("Metade esquerda"), mask);

    QImage activeSel(120, 90, QImage::Format_Alpha8);
    activeSel.fill(255);
    doc.setPixelSelection(activeSel);

    QString saveErr;
    QImage thumb(32, 24, QImage::Format_ARGB32);
    thumb.fill(Qt::blue);
    QVERIFY2(cc::saveDocument(doc, path, &saveErr, &thumb), qPrintable(saveErr));

    QString loadErr;
    auto reloaded = cc::loadDocument(path, &loadErr);
    QVERIFY2(reloaded != nullptr, qPrintable(loadErr));
    QVERIFY(!reloaded->pixelSelection().isNull()); // seleção ativa também persiste
    QCOMPARE(reloaded->pixelSelectionSpace(), SelectionSpace::Document);

    QCOMPARE(reloaded->savedSelections().size(), size_t(1));
    const SavedSelection* s = reloaded->findSavedSelection(id);
    QVERIFY(s != nullptr); // id preservado
    QCOMPARE(s->name, QStringLiteral("Metade esquerda"));
    QCOMPARE(s->mask.format(), QImage::Format_Alpha8);
    QCOMPARE(s->mask.size(), QSize(120, 90));
    QCOMPARE(s->space, SelectionSpace::Document);
    // a máscara sobreviveu byte a byte (255 na metade esquerda, 0 na direita)
    QCOMPARE(s->mask.constScanLine(10)[10], uchar(255));
    QCOMPARE(s->mask.constScanLine(10)[100], uchar(0));

    // Documento sem seleções guardadas continua a abrir (chave opcional).
    Document plain(50, 50);
    const QString path2 = tmp.filePath(QStringLiteral("plain.creatorcanvas"));
    QString e2;
    QVERIFY(cc::saveDocument(plain, path2, &e2, &thumb));
    auto reloaded2 = cc::loadDocument(path2, &loadErr);
    QVERIFY(reloaded2 != nullptr);
    QVERIFY(reloaded2->savedSelections().empty());
}

void TestSavedSelections::layerSpaceSelectionsRoundtrip()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("layer-space.creatorcanvas"));
    const QString secondPath = tmp.filePath(QStringLiteral("layer-space-2.creatorcanvas"));

    Document doc(120, 90);
    QImage active(40, 30, QImage::Format_Alpha8);
    active.fill(0);
    active.scanLine(4)[3] = 73;
    active.scanLine(29)[39] = 201;
    doc.setPixelSelection(active); // legacy API: tamanho diferente => Layer
    QCOMPARE(doc.pixelSelectionSpace(), SelectionSpace::Layer);

    QImage savedMask(50, 20, QImage::Format_ARGB32);
    savedMask.fill(Qt::transparent);
    savedMask.setPixelColor(7, 8, QColor(255, 255, 255, 117));
    const QUuid savedId = doc.addSavedSelection(QStringLiteral("Canal local"), savedMask);
    const SavedSelection* originalSaved = doc.findSavedSelection(savedId);
    QVERIFY(originalSaved != nullptr);
    QCOMPARE(originalSaved->space, SelectionSpace::Layer);
    const QImage expectedSaved = originalSaved->mask;

    QString error;
    QVERIFY2(cc::saveDocument(doc, path, &error), qPrintable(error));
    auto loaded = cc::loadDocument(path, &error);
    QVERIFY2(loaded != nullptr, qPrintable(error));
    QCOMPARE(loaded->pixelSelectionSpace(), SelectionSpace::Layer);
    QVERIFY(loaded->pixelSelection() == active);
    QCOMPARE(loaded->pixelSelection().constScanLine(4)[3], uchar(73));
    QCOMPARE(loaded->pixelSelection().constScanLine(29)[39], uchar(201));
    const SavedSelection* saved = loaded->findSavedSelection(savedId);
    QVERIFY(saved != nullptr);
    QCOMPARE(saved->space, SelectionSpace::Layer);
    QVERIFY(saved->mask == expectedSaved);
    QCOMPARE(saved->mask.constScanLine(8)[7], uchar(117));

    // Resize/rotate não reamostram seleções locais: o bitmap da camada não
    // mudou. O estado continua serializável depois dessas operações.
    QVERIFY(loaded->setCanvasSize(100, 80, QPointF(-10, -5)));
    QVERIFY(loaded->rotateCanvas(90.0));
    QCOMPARE(loaded->width(), 80);
    QCOMPARE(loaded->height(), 100);
    QVERIFY(loaded->pixelSelection() == active);
    QCOMPARE(loaded->pixelSelectionSpace(), SelectionSpace::Layer);
    saved = loaded->findSavedSelection(savedId);
    QVERIFY(saved != nullptr);
    QVERIFY(saved->mask == expectedSaved);
    QCOMPARE(saved->space, SelectionSpace::Layer);

    QVERIFY2(cc::saveDocument(*loaded, secondPath, &error), qPrintable(error));
    auto reloaded = cc::loadDocument(secondPath, &error);
    QVERIFY2(reloaded != nullptr, qPrintable(error));
    QVERIFY(reloaded->pixelSelection() == active);
    QCOMPARE(reloaded->pixelSelectionSpace(), SelectionSpace::Layer);
    saved = reloaded->findSavedSelection(savedId);
    QVERIFY(saved != nullptr);
    QVERIFY(saved->mask == expectedSaved);
    QCOMPARE(saved->space, SelectionSpace::Layer);
}

void TestSavedSelections::explicitLayerSpaceWithCanvasSizedMask()
{
    Document doc(20, 10);
    QImage mask(20, 10, QImage::Format_Alpha8);
    mask.fill(0);
    mask.scanLine(2)[3] = 255;
    doc.setPixelSelection(mask, SelectionSpace::Layer);
    const QUuid id = doc.addSavedSelection(QStringLiteral("Local"), mask,
                                           SelectionSpace::Layer);

    // Sem metadados explícitos, a igualdade inicial de tamanhos tornaria este
    // caso ambíguo. Rotate altera o tamanho do documento, não o da camada.
    QVERIFY(doc.rotateCanvas(90.0));
    QCOMPARE(doc.width(), 10);
    QCOMPARE(doc.height(), 20);
    QVERIFY(doc.pixelSelection() == mask);
    QCOMPARE(doc.pixelSelectionSpace(), SelectionSpace::Layer);
    QVERIFY(doc.findSavedSelection(id)->mask == mask);
    QCOMPARE(doc.findSavedSelection(id)->space, SelectionSpace::Layer);

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("same-size-layer.creatorcanvas"));
    QString error;
    QVERIFY2(cc::saveDocument(doc, path, &error), qPrintable(error));
    auto loaded = cc::loadDocument(path, &error);
    QVERIFY2(loaded != nullptr, qPrintable(error));
    QCOMPARE(loaded->pixelSelectionSpace(), SelectionSpace::Layer);
    QVERIFY(loaded->pixelSelection() == mask);
    const SavedSelection* saved = loaded->findSavedSelection(id);
    QVERIFY(saved != nullptr);
    QCOMPARE(saved->space, SelectionSpace::Layer);
    QVERIFY(saved->mask == mask);
}

void TestSavedSelections::historyAndInvertPreserveLayerSpace()
{
    Document doc(20, 10);
    CommandStack history;
    QImage original(20, 10, QImage::Format_Alpha8);
    original.fill(0);
    original.scanLine(2)[3] = 255;

    executeSetPixelSelection(doc, &history, original, QStringLiteral("test.layer"),
                             SelectionSpace::Layer);
    QCOMPARE(doc.pixelSelectionSpace(), SelectionSpace::Layer);

    const QImage inverse = SelectionOperations::inverted(doc.pixelSelection());
    // Mesmo caminho legacy usado pela ação Invert da MainWindow.
    executeSetPixelSelection(doc, &history, inverse,
                             QStringLiteral("selection.invert"));
    QCOMPARE(doc.pixelSelectionSpace(), SelectionSpace::Layer);
    QCOMPARE(doc.pixelSelection().constScanLine(2)[3], uchar(0));
    QCOMPARE(doc.pixelSelection().constScanLine(0)[0], uchar(255));

    history.undo();
    QCOMPARE(doc.pixelSelectionSpace(), SelectionSpace::Layer);
    QVERIFY(doc.pixelSelection() == original);
    history.redo();
    QCOMPARE(doc.pixelSelectionSpace(), SelectionSpace::Layer);
    QVERIFY(doc.pixelSelection() == inverse);

    executeSetPixelSelection(doc, &history, QImage(),
                             QStringLiteral("selection.deselect"));
    QVERIFY(doc.pixelSelection().isNull());
    history.undo();
    QCOMPARE(doc.pixelSelectionSpace(), SelectionSpace::Layer);
    QVERIFY(doc.pixelSelection() == inverse);

    Document withoutHistory(20, 10);
    withoutHistory.setPixelSelection(original, SelectionSpace::Layer);
    executeSetPixelSelection(withoutHistory, nullptr,
                             SelectionOperations::inverted(original),
                             QStringLiteral("selection.invert"));
    QCOMPARE(withoutHistory.pixelSelectionSpace(), SelectionSpace::Layer);
}

void TestSavedSelections::reselectRestoresPreviousLayerSpace()
{
    Document doc(20, 10);
    CommandStack history;
    QImage local(20, 10, QImage::Format_Alpha8);
    local.fill(0);
    local.scanLine(7)[11] = 193;

    executeSetPixelSelection(doc, &history, local, QStringLiteral("test.layer"),
                             SelectionSpace::Layer);
    const QUuid savedId = doc.addSavedSelection(
        QStringLiteral("Canvas-sized local"), doc.pixelSelection(),
        doc.pixelSelectionSpace());
    QCOMPARE(doc.findSavedSelection(savedId)->space, SelectionSpace::Layer);

    executeSetPixelSelection(doc, &history, QImage(),
                             QStringLiteral("selection.deselect"));
    const SelectionState previous = doc.selectionSnapshot();
    QCOMPARE(previous.previousImageSpace(), SelectionSpace::Layer);
    executeSetPixelSelection(doc, &history, doc.previousPixelSelection(),
                             QStringLiteral("selection.reselect"),
                             previous.previousImageSpace());
    QCOMPARE(doc.pixelSelectionSpace(), SelectionSpace::Layer);
    QVERIFY(doc.pixelSelection() == local);
    history.undo();
    QVERIFY(doc.pixelSelection().isNull());
    history.redo();
    QCOMPARE(doc.pixelSelectionSpace(), SelectionSpace::Layer);

    const SavedSelection* saved = doc.findSavedSelection(savedId);
    QVERIFY(saved != nullptr);
    executeSetPixelSelection(doc, &history, saved->mask,
                             QStringLiteral("selection.load"), saved->space);
    QCOMPARE(doc.pixelSelectionSpace(), SelectionSpace::Layer);
    QVERIFY(doc.pixelSelection() == local);
    history.undo();
    QCOMPARE(doc.pixelSelectionSpace(), SelectionSpace::Layer);
    history.redo();
    QCOMPARE(doc.pixelSelectionSpace(), SelectionSpace::Layer);
}

void TestSavedSelections::layerMaskMappingUsesRendererTransformOrder()
{
    Document doc(80, 80);
    QImage bitmap(2, 2, QImage::Format_ARGB32);
    bitmap.fill(Qt::white);

    auto image = std::make_unique<ImageLayer>();
    image->assetId = doc.assets().addImage(bitmap);
    image->naturalWidth = bitmap.width();
    image->naturalHeight = bitmap.height();
    image->transform.position = QPointF(12.0, 30.0);
    image->transform.rotationDeg = 20.0;
    image->transform.scaleX = 2.0;
    image->transform.scaleY = 1.5;
    const LayerId imageId = image->id();

    auto group = std::make_unique<GroupLayer>();
    group->transform.position = QPointF(45.0, 40.0);
    group->transform.rotationDeg = 70.0;
    group->transform.scaleX = 1.25;
    group->transform.shearX = 0.2;
    const LayerId groupId = group->id();
    QVERIFY(doc.addLayer(std::move(group), doc.rootGroup()));
    auto* parent = static_cast<GroupLayer*>(doc.findLayer(groupId));
    QVERIFY(doc.addLayer(std::move(image), parent));
    const auto* layer = static_cast<const ImageLayer*>(doc.findLayer(imageId));

    const QTransform layerMatrix = layer->transform.matrix(layer->contentBounds());
    const QTransform parentMatrix = parent->transform.matrix(parent->contentBounds());
    const QTransform rendererOrder = layerMatrix * parentMatrix;
    const QTransform incorrectOrder = parentMatrix * layerMatrix;

    QImage selection(doc.width(), doc.height(), QImage::Format_Alpha8);
    for (int y = 0; y < selection.height(); ++y)
        for (int x = 0; x < selection.width(); ++x)
            selection.scanLine(y)[x] = static_cast<uchar>(1 + (x * 3 + y * 5) % 254);

    const QImage mapped = SelectionOperations::selectionMaskInLayerSpace(
        doc, *layer, selection, SelectionSpace::Document);
    QCOMPARE(mapped.size(), bitmap.size());
    bool distinguishesOrder = false;
    const QRect documentRect(QPoint(0, 0), selection.size());
    for (int y = 0; y < bitmap.height(); ++y) {
        for (int x = 0; x < bitmap.width(); ++x) {
            const QPointF localCenter(x + 0.5, y + 0.5);
            const QPointF expectedPoint = rendererOrder.map(localCenter);
            const QPointF wrongPoint = incorrectOrder.map(localCenter);
            const QPoint expected(qFloor(expectedPoint.x()), qFloor(expectedPoint.y()));
            const QPoint wrong(qFloor(wrongPoint.x()), qFloor(wrongPoint.y()));
            QVERIFY(documentRect.contains(expected));
            const uchar expectedCoverage =
                selection.constScanLine(expected.y())[expected.x()];
            const uchar wrongCoverage = documentRect.contains(wrong)
                ? selection.constScanLine(wrong.y())[wrong.x()]
                : uchar(0);
            QCOMPARE(mapped.constScanLine(y)[x], expectedCoverage);
            distinguishesOrder |= expectedCoverage != wrongCoverage;
        }
    }
    QVERIFY(distinguishesOrder);
}

void TestSavedSelections::layerMaskMappingHonorsExplicitSpace()
{
    Document doc(20, 10);
    QImage bitmap(doc.width(), doc.height(), QImage::Format_ARGB32);
    bitmap.fill(Qt::white);
    auto image = std::make_unique<ImageLayer>();
    image->assetId = doc.assets().addImage(bitmap);
    image->naturalWidth = bitmap.width();
    image->naturalHeight = bitmap.height();
    // O centro padrão (10,5) mais três pixels: Document não pode usar a
    // máscara diretamente apenas porque as dimensões coincidem.
    image->transform.position = QPointF(13.0, 5.0);
    const LayerId imageId = image->id();
    QVERIFY(doc.addLayer(std::move(image), doc.rootGroup()));
    const auto* layer = static_cast<const ImageLayer*>(doc.findLayer(imageId));

    QImage selection(doc.width(), doc.height(), QImage::Format_Alpha8);
    selection.fill(0);
    selection.scanLine(4)[5] = 255;

    const QImage documentMapped = SelectionOperations::selectionMaskInLayerSpace(
        doc, *layer, selection, SelectionSpace::Document);
    QCOMPARE(documentMapped.constScanLine(4)[2], uchar(255));
    QCOMPARE(documentMapped.constScanLine(4)[5], uchar(0));

    const QImage layerMapped = SelectionOperations::selectionMaskInLayerSpace(
        doc, *layer, selection, SelectionSpace::Layer);
    QVERIFY(layerMapped == selection);
    QCOMPARE(layerMapped.constScanLine(4)[5], uchar(255));

    const QImage wrongSize(19, 10, QImage::Format_Alpha8);
    QVERIFY(SelectionOperations::selectionMaskInLayerSpace(
        doc, *layer, wrongSize, SelectionSpace::Layer).isNull());
}

void TestSavedSelections::inconsistentDocumentSelectionSaveFails()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("invalid.creatorcanvas"));
    Document doc(20, 10);
    QImage wrongSize(5, 4, QImage::Format_Alpha8);
    wrongSize.fill(255);

    doc.setPixelSelection(wrongSize, SelectionSpace::Document);
    QString error;
    QVERIFY(!cc::saveDocument(doc, path, &error));
    QVERIFY(error.contains(QStringLiteral("active pixel selection")));
    QVERIFY(!QFile::exists(path));

    doc.clearPixelSelection();
    doc.addSavedSelection(QStringLiteral("Inválida"), wrongSize,
                          SelectionSpace::Document);
    error.clear();
    QVERIFY(!cc::saveDocument(doc, path, &error));
    QVERIFY(error.contains(QStringLiteral("saved selection")));
    QVERIFY(!QFile::exists(path));
}

QTEST_GUILESS_MAIN(TestSavedSelections)
#include "test_saved_selections.moc"
