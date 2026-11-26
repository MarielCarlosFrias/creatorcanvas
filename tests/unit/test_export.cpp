#include "core/Document.h"
#include "core/layers/Layer.h"
#include "imageio/DocumentExporter.h"
#include "imageio/SvgExporter.h"
#include "rendering/IRenderer.h"

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>
#include <QXmlStreamReader>

using namespace cc;

class TestExport final : public QObject
{
    Q_OBJECT

private slots:
    void exportsPngWithContent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("out.png");

        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(200, 30, 30);
        QVERIFY(doc.addLayer(std::move(bg)));

        QString error;
        QVERIFY(exportDocumentToImage(doc, path, QStringLiteral("png"),
                                      95, 1.0, &error));

        QImage loaded(path);
        QCOMPARE(loaded.width(), 100);
        QCOMPARE(loaded.height(), 100);
        QCOMPARE(loaded.pixelColor(50, 50), QColor(200, 30, 30));
    }

    void exportScaleMultipliesSize()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("big.png");

        Document doc(100, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(Qt::white);
        QVERIFY(doc.addLayer(std::move(bg)));

        QString error;
        QVERIFY(exportDocumentToImage(doc, path, QStringLiteral("png"),
                                      95, 2.0, &error));

        QImage loaded(path);
        QCOMPARE(loaded.width(), 200);
        QCOMPARE(loaded.height(), 200);
    }

    void exportLayerScaleMultipliesContentAndSize()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("layer-2x.png");

        Document doc(10, 10);
        QImage pixels(2, 2, QImage::Format_ARGB32);
        pixels.fill(QColor(20, 40, 200));
        auto image = std::make_unique<ImageLayer>();
        image->assetId = doc.assets().addImage(pixels);
        image->naturalWidth = 2;
        image->naturalHeight = 2;
        image->transform.position = QPointF(5, 5); // local top-left = (4,4)
        const LayerId id = image->id();
        QVERIFY(doc.addLayer(std::move(image)));

        QString error;
        QVERIFY2(exportLayerToImage(doc, id, path, QStringLiteral("png"),
                                    95, 2.0, &error), qPrintable(error));
        const QImage loaded(path);
        QCOMPARE(loaded.size(), QSize(20, 20));
        QCOMPARE(loaded.pixelColor(8, 8), QColor(20, 40, 200));
        QCOMPARE(loaded.pixelColor(0, 0).alpha(), 0);
    }

    void exportJpegFlattensAlpha()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("out.jpg");

        Document doc(100, 100); // no layers = fully transparent
        QString error;
        QVERIFY(exportDocumentToImage(doc, path, QStringLiteral("jpeg"),
                                      95, 1.0, &error));

        QImage loaded(path);
        QCOMPARE(loaded.pixelColor(50, 50).alpha(), 255); // flattened
    }

    void exportToBadPathFails()
    {
        Document doc(100, 100);
        QString error;
        QVERIFY(!exportDocumentToImage(doc, QString(), QStringLiteral("png"),
                                       95, 1.0, &error));
        QVERIFY(!error.isEmpty());
    }

    void exportsPdf()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("out.pdf");

        Document doc(120, 80);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(10, 120, 200);
        QVERIFY(doc.addLayer(std::move(bg)));
        auto shape = std::make_unique<ShapeLayer>();
        shape->kind = ShapeKind::Ellipse;
        shape->points = QPolygonF({QPointF(10, 10), QPointF(60, 10), QPointF(60, 50), QPointF(10, 50)});
        shape->fill = QColor(250, 200, 0);
        shape->transform.position = QPointF(35, 30);
        QVERIFY(doc.addLayer(std::move(shape)));

        auto* cache = dynamic_cast<CachingRenderer*>(&defaultRenderer());
        QVERIFY(cache != nullptr);
        const int missesBefore = cache->cacheMisses();
        const int compositeMissesBefore = cache->compositeMisses();

        QString error;
        QVERIFY2(exportDocumentToPdf(doc, path, 1.0, &error), qPrintable(error));

        // Exportar não pode tocar no CachingRenderer interactivo: os seus
        // pixmaps transformariam este documento simples numa página raster.
        QCOMPARE(cache->cacheMisses(), missesBefore);
        QCOMPARE(cache->compositeMisses(), compositeMissesBefore);

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray pdf = f.readAll();
        QVERIFY(pdf.startsWith("%PDF-"));
        QVERIFY(pdf.size() > 500); // conteúdo real, não um ficheiro vazio
        QVERIFY(!pdf.contains("/Subtype /Image"));
    }

    void exportsSvgVector()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("out.svg");

        Document doc(200, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(255, 255, 255);
        QVERIFY(doc.addLayer(std::move(bg)));

        auto rect = std::make_unique<ShapeLayer>();
        rect->kind = ShapeKind::Rectangle;
        rect->points = QPolygonF({QPointF(0, 0), QPointF(50, 0), QPointF(50, 30), QPointF(0, 30)});
        rect->fill = QColor(200, 30, 30);
        rect->transform.position = QPointF(25, 15);
        QVERIFY(doc.addLayer(std::move(rect)));

        auto text = std::make_unique<TextLayer>();
        text->content = QStringLiteral("Olá <mundo>"); // deve ser escapado
        text->color = QColor(0, 0, 0);
        text->transform.position = QPointF(100, 50);
        QVERIFY(doc.addLayer(std::move(text)));

        QString error;
        QVERIFY2(exportDocumentToSvg(doc, path, &error), qPrintable(error));

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString svg = QString::fromUtf8(f.readAll());

        // XML bem formado?
        QXmlStreamReader xml(svg);
        while (!xml.atEnd())
            xml.readNext();
        QVERIFY2(!xml.hasError(), qPrintable(xml.errorString()));

        QVERIFY(svg.contains(QStringLiteral("<svg")));
        QVERIFY(svg.contains(QStringLiteral("viewBox=\"0 0 200.000 100.000\"")));
        QVERIFY(svg.contains(QStringLiteral("<rect")));          // fundo
        QVERIFY(svg.contains(QStringLiteral("#c81e1e")));        // fill do rect (200,30,30)
        QVERIFY(svg.contains(QStringLiteral("<text")));           // texto vetorial
        QVERIFY(svg.contains(QStringLiteral("Olá &lt;mundo&gt;"))); // escapado
        QVERIFY(!svg.contains(QStringLiteral("<mundo>")));        // sem tag crua

        // QTEST_GUILESS_MAIN dá ao texto bounds locais 100x50. O centro local
        // (50,25) é transformado uma única vez para position=(100,50); x/y não
        // podem repetir a posição global dentro da mesma matriz.
        QVERIFY(svg.contains(QStringLiteral("x=\"50.000\"")));
        QVERIFY(svg.contains(QStringLiteral("y=\"25.000\"")));
        QVERIFY(svg.contains(QStringLiteral(
            "transform=\"matrix(1.000,0.000,0.000,1.000,50.000,25.000)\"")));
    }

    void svgExportScaleChangesViewportNotViewBox()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("scaled.svg");
        Document doc(120, 80);

        QString error;
        QVERIFY2(exportDocumentToSvg(doc, path, 2.0, &error), qPrintable(error));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString svg = QString::fromUtf8(f.readAll());
        QVERIFY(svg.contains(QStringLiteral("width=\"240.000\"")));
        QVERIFY(svg.contains(QStringLiteral("height=\"160.000\"")));
        QVERIFY(svg.contains(QStringLiteral("viewBox=\"0 0 120.000 80.000\"")));
    }

    void svgExportHandlesImageLayerAsEmbeddedPng()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("img.svg");

        Document doc(40, 40);
        QImage px(8, 6, QImage::Format_ARGB32);
        px.fill(QColor(1, 2, 3, 255));
        auto img = std::make_unique<ImageLayer>();
        img->assetId = doc.assets().addImage(px);
        img->naturalWidth = 8;
        img->naturalHeight = 6;
        img->transform.position = QPointF(20, 18);
        img->transform.scaleX = 2.0;
        img->transform.scaleY = 1.5;
        img->setOpacity(0.5f);
        QVERIFY(doc.addLayer(std::move(img)));

        QString error;
        QVERIFY2(exportDocumentToSvg(doc, path, &error), qPrintable(error));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString svg = QString::fromUtf8(f.readAll());
        QVERIFY(svg.contains(QStringLiteral("<image")));
        QVERIFY(svg.contains(QStringLiteral("data:image/png;base64,")));

        QXmlStreamReader xml(svg);
        bool foundImage = false;
        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement() || xml.name() != QLatin1String("image"))
                continue;
            foundImage = true;
            const auto attrs = xml.attributes();
            // O PNG já é um canvas completo com transform e opacidade
            // rasterizados. Estes atributos aplicariam ambos outra vez.
            QVERIFY(!attrs.hasAttribute(QStringLiteral("transform")));
            QVERIFY(!attrs.hasAttribute(QStringLiteral("opacity")));
            QCOMPARE(attrs.value(QStringLiteral("width")).toString(), QStringLiteral("40.000"));
            QCOMPARE(attrs.value(QStringLiteral("height")).toString(), QStringLiteral("40.000"));
        }
        QVERIFY(!xml.hasError());
        QVERIFY(foundImage);
    }
};

QTEST_GUILESS_MAIN(TestExport)
#include "test_export.moc"
