#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>

#include "core/Document.h"
#include "core/layers/Layer.h"
#include "ui/navigatorpanel.h"
#include "ui/swatchpanel.h"
#include "ui/tooloptionsbar.h"

using namespace cc;

class TestPanels final : public QObject
{
    Q_OBJECT

private slots:
    void navigatorBuildsInitialThumbnail();
    void navigatorRebuildsWhenDocumentChanges();
    void swatchesFollowToolOptionsColor();
    void toolOptionsEmitsColorChange();
};

static Document* makeRedDocument()
{
    auto* doc = new Document(100, 60);
    auto background = std::make_unique<BackgroundLayer>();
    background->fill = QColor(210, 20, 30);
    doc->addLayer(std::move(background));
    return doc;
}

static bool containsReddishPixel(const QImage& image)
{
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const QColor c = image.pixelColor(x, y);
            if (c.red() > 150 && c.green() < 100 && c.blue() < 100)
                return true;
        }
    return false;
}

void TestPanels::navigatorBuildsInitialThumbnail()
{
    std::unique_ptr<Document> doc(makeRedDocument());
    NavigatorPanel panel;
    panel.resize(240, 160);
    panel.setDocument(doc.get());
    const QImage frame = panel.grab().toImage();
    QVERIFY2(containsReddishPixel(frame),
             "Navigator não construiu a miniatura no primeiro paint");
}

void TestPanels::navigatorRebuildsWhenDocumentChanges()
{
    std::unique_ptr<Document> doc(makeRedDocument());
    NavigatorPanel panel;
    panel.resize(240, 160);
    panel.setDocument(doc.get());
    QVERIFY(containsReddishPixel(panel.grab().toImage()));

    auto* background = static_cast<BackgroundLayer*>(doc->rootGroup()->children.front().get());
    background->fill = QColor(20, 180, 40);
    doc->touchLayer(background->id());
    QCoreApplication::processEvents();
    const QImage frame = panel.grab().toImage();
    bool green = false;
    for (int y = 0; y < frame.height() && !green; ++y)
        for (int x = 0; x < frame.width(); ++x) {
            const QColor c = frame.pixelColor(x, y);
            if (c.green() > 140 && c.red() < 100) {
                green = true;
                break;
            }
        }
    QVERIFY(green);
}

void TestPanels::swatchesFollowToolOptionsColor()
{
    ToolOptionsBar options;
    SwatchPanel swatches;
    connect(&options, &ToolOptionsBar::paintColorChanged,
            &swatches, &SwatchPanel::setCurrentColor);

    const QColor color(18, 52, 86);
    options.setPaintColor(color);
    QCOMPARE(swatches.currentColor(), color);
    // A integração usa exatamente este signal; valida que o destino existe e
    // que a cor foi emitida sem loops.
    QSignalSpy spy(&options, &ToolOptionsBar::paintColorChanged);
    options.setPaintColor(QColor(90, 40, 10));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(swatches.currentColor(), QColor(90, 40, 10));
}

void TestPanels::toolOptionsEmitsColorChange()
{
    ToolOptionsBar options;
    QSignalSpy spy(&options, &ToolOptionsBar::paintColorChanged);
    const QColor color(1, 2, 3);
    options.setPaintColor(color);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).value<QColor>(), color);
}

QTEST_MAIN(TestPanels)
#include "test_panels.moc"
