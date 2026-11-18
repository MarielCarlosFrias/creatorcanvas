#include <QtTest>

#include <algorithm>

#include "ui/canvastransform.h"

using namespace cc;

/// The viewport math is widget-free on purpose: these tests pin the fit,
/// anchoring and clamping rules that used to live inline in CanvasView.
class TestCanvasTransform : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testFitCentersAndScales();
    void testZoomAnchorsAtThePointer();
    void testZoomClampedToRange();
    void testManualZoomDisablesAutoFit();
    void testResizeKeepsContentCentered();
    void testPendingFitWaitsForGeometry();
    void testDocumentSizeRearmsFit();

private:
    CanvasTransform m_transform;
};

void TestCanvasTransform::initTestCase()
{
    m_transform.setViewportSize(QSize(1000, 800));
    m_transform.setDocumentSize(QSize(800, 600));
}

void TestCanvasTransform::testFitCentersAndScales()
{
    m_transform.fit();

    // 92% of the smaller dimension ratio: 1000*0.92/800 = 1.15.
    QCOMPARE(m_transform.zoom(), 1.15);
    const QPointF pan = m_transform.panOffset();
    QCOMPARE(pan.x(), (1000.0 - 800.0 * 1.15) / 2.0); // 40
    QCOMPARE(pan.y(), (800.0 - 600.0 * 1.15) / 2.0);  // 55
    QVERIFY(m_transform.autoFit());

    // The document corners land inside the viewport, centred.
    const QTransform t = m_transform.docToDevice();
    const QPointF topLeft = t.map(QPointF(0, 0));
    const QPointF bottomRight = t.map(QPointF(800, 600));
    QCOMPARE(topLeft, QPointF(40, 55));
    QCOMPARE(bottomRight.x(), 40.0 + 800.0 * 1.15);
    QCOMPARE(bottomRight.y(), 55.0 + 600.0 * 1.15);
}

void TestCanvasTransform::testZoomAnchorsAtThePointer()
{
    m_transform.fit();
    const QPointF anchor(150.0, 300.0);

    // The document point under the cursor before the zoom...
    const QPointF docUnderCursor = m_transform.deviceToDoc().map(anchor);

    m_transform.zoomAt(anchor, 2.0);

    // ...must still be under the cursor afterwards (anchor invariant).
    const QPointF after = m_transform.docToDevice().map(docUnderCursor);
    QVERIFY(qAbs(after.x() - anchor.x()) < 0.001);
    QVERIFY(qAbs(after.y() - anchor.y()) < 0.001);
    QCOMPARE(m_transform.zoom(), 2.3); // 1.15 * 2
    QVERIFY(!m_transform.autoFit());
}

void TestCanvasTransform::testZoomClampedToRange()
{
    m_transform.fit();
    m_transform.zoomAt(QPointF(500, 400), 1e9);
    QCOMPARE(m_transform.zoom(), CanvasTransform::kMaxZoom);

    m_transform.zoomAt(QPointF(500, 400), 1e-9);
    QCOMPARE(m_transform.zoom(), CanvasTransform::kMinZoom);
}

void TestCanvasTransform::testManualZoomDisablesAutoFit()
{
    m_transform.fit();
    QVERIFY(m_transform.autoFit());

    m_transform.zoomIn();
    QVERIFY(!m_transform.autoFit());

    m_transform.setAutoFit(true);
    m_transform.fit(); // explicit fit re-arms it
    QVERIFY(m_transform.autoFit());
}

void TestCanvasTransform::testResizeKeepsContentCentered()
{
    m_transform.fit();
    const double zoom = m_transform.zoom();

    m_transform.keepCenteredOnResize(QSize(1000, 800), QSize(1100, 860));

    QCOMPARE(m_transform.zoom(), zoom); // zoom survives
    const QPointF pan = m_transform.panOffset();
    QCOMPARE(pan.x(), (1000.0 - 800.0 * zoom) / 2.0 + (1100.0 - 1000.0) / 2.0);
    QCOMPARE(pan.y(), (800.0 - 600.0 * zoom) / 2.0 + (860.0 - 800.0) / 2.0);
}

void TestCanvasTransform::testPendingFitWaitsForGeometry()
{
    CanvasTransform fresh;
    fresh.setDocumentSize(QSize(800, 600));
    fresh.fitIfNeeded(); // no viewport yet: must stay at 1.0, not divide by zero
    QCOMPARE(fresh.zoom(), 1.0);

    fresh.setViewportSize(QSize(1000, 800));
    fresh.fitIfNeeded();
    QCOMPARE(fresh.zoom(), 1.15);
    QCOMPARE(fresh.autoFit(), true);
}

void TestCanvasTransform::testDocumentSizeRearmsFit()
{
    m_transform.fit();
    m_transform.zoomIn(); // manual: autoFit off
    QVERIFY(!m_transform.autoFit());

    m_transform.setDocumentSize(QSize(1920, 1080));
    m_transform.fitIfNeeded(); // new document: fits again
    const double expected = std::min(1000.0 * 0.92 / 1920.0, 800.0 * 0.92 / 1080.0);
    QVERIFY(qAbs(m_transform.zoom() - expected) < 1e-9);
    QVERIFY(m_transform.autoFit());
}

QTEST_MAIN(TestCanvasTransform)

#include "test_canvas_transform.moc"