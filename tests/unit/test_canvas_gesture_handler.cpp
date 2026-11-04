#include <QTest>
#include <QObject>
#include "ui/canvasgesturehandler.h"
#include "core/layers/Layer.h"

class TestCanvasGestureHandler : public QObject
{
    Q_OBJECT

private slots:
    void testComputeHandlesAndHitTest();
    void testCursorDetermination();
    void testMoveGesture();
    void testRotateGesture();
    void testScaleGesture();
    void testFinishReset();
};

void TestCanvasGestureHandler::testComputeHandlesAndHitTest()
{
    cc::ShapeLayer shape;
    shape.points = {QPointF(0, 0), QPointF(200, 100)};
    shape.transform.position = QPointF(100, 100);
    shape.transform.scaleX = 1.0;
    shape.transform.scaleY = 1.0;

    QTransform docToDevice; // Identidade

    cc::CanvasHandles handles = cc::CanvasGestureHandler::computeHandles(shape, docToDevice);
    QVERIFY(handles.valid);

    // O ponto 8 é o handle de rotação
    QCOMPARE(handles.points[8].x(), handles.points[4].x());
    QVERIFY(handles.points[8].y() < handles.points[4].y());

    // Hit-testing no ponto 0 (topLeft)
    int hit0 = cc::CanvasGestureHandler::hitTestHandle(handles.points[0], handles);
    QCOMPARE(hit0, 0);

    // Hit-testing no ponto 8 (rotate)
    int hit8 = cc::CanvasGestureHandler::hitTestHandle(handles.points[8], handles);
    QCOMPARE(hit8, 8);

    // Hit-testing longe
    int miss = cc::CanvasGestureHandler::hitTestHandle(QPointF(-500, -500), handles);
    QCOMPARE(miss, -1);
}

void TestCanvasGestureHandler::testCursorDetermination()
{
    cc::ShapeLayer shape;
    shape.points = {QPointF(0, 0), QPointF(100, 100)};
    cc::CanvasHandles handles = cc::CanvasGestureHandler::computeHandles(shape, QTransform());
    QVERIFY(handles.valid);

    // Cursor normal nos cantos e rotação
    QCOMPARE(cc::CanvasGestureHandler::handleCursor(0), Qt::SizeFDiagCursor);
    QCOMPARE(cc::CanvasGestureHandler::handleCursor(1), Qt::SizeBDiagCursor);
    QCOMPARE(cc::CanvasGestureHandler::handleCursor(4), Qt::SizeVerCursor);
    QCOMPARE(cc::CanvasGestureHandler::handleCursor(5), Qt::SizeHorCursor);
    QCOMPARE(cc::CanvasGestureHandler::handleCursor(8), Qt::CrossCursor);

    // determineCursor com lock
    Qt::CursorShape lockedCursor = cc::CanvasGestureHandler::determineCursor(handles.points[0], handles, /*isLocked=*/true);
    QCOMPARE(lockedCursor, Qt::ForbiddenCursor);

    // determineCursor sem lock
    Qt::CursorShape unlockedCursor = cc::CanvasGestureHandler::determineCursor(handles.points[0], handles, /*isLocked=*/false);
    QCOMPARE(unlockedCursor, Qt::SizeFDiagCursor);
}

void TestCanvasGestureHandler::testMoveGesture()
{
    cc::CanvasGestureHandler handler;
    cc::ShapeLayer layer1;
    layer1.points = {QPointF(0, 0), QPointF(50, 50)};
    layer1.transform.position = QPointF(50, 50);

    cc::ShapeLayer layer2;
    layer2.points = {QPointF(0, 0), QPointF(50, 50)};
    layer2.transform.position = QPointF(200, 200);

    QList<cc::Layer*> multi = {&layer1, &layer2};

    handler.startMove(layer1, QPointF(50, 50), multi);
    QVERIFY(handler.isInteracting());
    QCOMPARE(handler.gestureType(), cc::CanvasGestureType::Move);

    // Mover 30px em X e 20px em Y
    cc::AffineTransform t1 = handler.updateTransform(QPointF(80, 70));
    QCOMPARE(t1.position, QPointF(80, 70));

    QPointF delta = handler.computeMoveDelta(QPointF(80, 70));
    QCOMPARE(delta, QPointF(30, 20));
}

void TestCanvasGestureHandler::testRotateGesture()
{
    cc::CanvasGestureHandler handler;
    cc::ShapeLayer layer;
    layer.points = {QPointF(0, 0), QPointF(100, 100)};
    layer.transform.position = QPointF(100, 100);
    layer.transform.rotationDeg = 0.0;

    QRectF bounds = layer.contentBounds();
    QPointF center = layer.transform.matrix(bounds).map(bounds.center());

    // Pressiona acima do centro (ângulo de -90 graus / -pi/2)
    QPointF pressDoc(center.x(), center.y() - 50);
    handler.startRotate(layer, pressDoc);

    QVERIFY(handler.isInteracting());
    QCOMPARE(handler.gestureType(), cc::CanvasGestureType::Rotate);

    // Move para a direita do centro (ângulo de 0 graus) -> rotação de +90 graus
    QPointF nowDoc(center.x() + 50, center.y());
    cc::AffineTransform rotated = handler.updateTransform(nowDoc);

    QVERIFY(qAbs(rotated.rotationDeg - 90.0) < 1.0);
}

void TestCanvasGestureHandler::testScaleGesture()
{
    cc::CanvasGestureHandler handler;
    cc::ShapeLayer layer;
    layer.points = {QPointF(0, 0), QPointF(100, 100)};
    layer.transform.position = QPointF(100, 100);
    layer.transform.scaleX = 1.0;
    layer.transform.scaleY = 1.0;

    QRectF bounds = layer.contentBounds();
    QTransform mat = layer.transform.matrix(bounds);

    // Canto inferior direito (handle 2)
    QPointF br = mat.map(bounds.bottomRight());
    handler.startScale(layer, 2, br);

    QVERIFY(handler.isInteracting());
    QCOMPARE(handler.gestureType(), cc::CanvasGestureType::Scale);
    QCOMPARE(handler.activeHandle(), 2);

    // Ponto fixo oposto deve ser topLeft (handle 0)
    // Afastar o mouse dobra a escala
    QPointF tl = mat.map(bounds.topLeft());
    QPointF stretched = tl + (br - tl) * 2.0;

    cc::AffineTransform scaled = handler.updateTransform(stretched);
    QVERIFY(scaled.scaleX > 1.8 && scaled.scaleX < 2.2);
    QVERIFY(scaled.scaleY > 1.8 && scaled.scaleY < 2.2);
}

void TestCanvasGestureHandler::testFinishReset()
{
    cc::CanvasGestureHandler handler;
    cc::ShapeLayer layer;
    handler.startMove(layer, QPointF(10, 10));
    QVERIFY(handler.isInteracting());

    handler.finish();
    QVERIFY(!handler.isInteracting());
    QCOMPARE(handler.gestureType(), cc::CanvasGestureType::None);
    QCOMPARE(handler.activeHandle(), -1);
}

QTEST_MAIN(TestCanvasGestureHandler)
#include "test_canvas_gesture_handler.moc"
