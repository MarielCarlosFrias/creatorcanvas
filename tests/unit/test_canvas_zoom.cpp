#include <QTest>

#include "core/Document.h"
#include "ui/canvasview.h"

using namespace cc;

class TestCanvasZoom final : public QObject
{
    Q_OBJECT

private slots:
    void fitModeRefitsOnResize()
    {
        CanvasView view;
        auto doc = std::make_unique<Document>(200, 100);
        view.setDocument(doc.get());
        // show() é necessário: um widget oculto não recebe QResizeEvent.
        view.show();
        view.resize(800, 600);
        QApplication::processEvents();

        const double fitted = view.zoom();
        QVERIFY2(fitted > 1.0, qPrintable(QStringLiteral("fit não aplicou: %1").arg(fitted)));
        QVERIFY(fitted > 0.0);

        view.resize(400, 300);
        QApplication::processEvents();
        // Em modo "ajustar", o resize re-ajusta: zoom menor que antes.
        QVERIFY2(view.zoom() < fitted,
                 qPrintable(QStringLiteral("fitted=%1 depois=%2")
                                .arg(fitted).arg(view.zoom())));
    }

    void manualZoomSurvivesResize()
    {
        CanvasView view;
        auto doc = std::make_unique<Document>(200, 100);
        view.setDocument(doc.get());
        view.show();
        view.resize(800, 600);
        QApplication::processEvents();

        // Zoom manual assume o controle (m_autoFit = false).
        view.zoomIn();
        view.zoomIn();
        const double manual = view.zoom();
        QVERIFY(manual > 0.0);

        view.resize(1200, 900);
        QApplication::processEvents();
        // Resize NÃO pode zerar o zoom do usuário.
        QCOMPARE(view.zoom(), manual);
    }
};

QTEST_MAIN(TestCanvasZoom)
#include "test_canvas_zoom.moc"
