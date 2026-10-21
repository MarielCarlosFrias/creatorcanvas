/// Reprodução do bug reportado: "quando faço Ctrl+Z fica cinza".
/// Cria um documento real, executa ações via histórico, desfaz e verifica
/// que o canvas renderiza conteúdo (não fica todo cinza-escuro).
#include <QTest>

#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "ui/canvasview.h"

using namespace cc;

namespace {

/// Conta a proporção de pixels "cinza-escuro de fundo" numa captura do canvas.
/// O fundo do editor é #0e1013 (DWindow) — o documento renderiza por cima.
double darkRatio(const QImage& image)
{
    int dark = 0, total = 0;
    for (int y = 0; y < image.height(); y += 3) {
        for (int x = 0; x < image.width(); x += 3) {
            ++total;
            const QRgb px = image.pixel(x, y);
            if (qRed(px) < 45 && qGreen(px) < 45 && qBlue(px) < 45)
                ++dark;
        }
    }
    return total > 0 ? double(dark) / double(total) : 1.0;
}

} // namespace

class TestUndoRender final : public QObject
{
    Q_OBJECT

private slots:
    void undoKeepsCanvasRendered()
    {
        CommandStack history;
        CanvasView view;
        view.resize(800, 600);
        view.show();

        auto doc = std::make_unique<Document>(200, 100);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->name = QStringLiteral("Background");
        bg->fill = QColor(Qt::white);
        QVERIFY(doc->addLayer(std::move(bg)));

        QImage img(200, 100, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(200, 30, 30));
        auto layer = std::make_unique<ImageLayer>();
        layer->name = QStringLiteral("Photo");
        layer->assetId = doc->assets().addImage(img);
        layer->naturalWidth = 200;
        layer->naturalHeight = 100;
        layer->transform.position = QPointF(100, 50);
        const LayerId id = layer->id();
        QVERIFY(doc->addLayer(std::move(layer)));

        view.setDocument(doc.get());
        view.setHistory(&history);
        view.setSelectedLayer(id);
        QVERIFY(QTest::qWaitFor([&] { return view.width() > 0; }));

        // Pré-undo: o canvas tem o documento (fundo branco) renderizado.
        const QImage before = view.grab().toImage();
        QVERIFY2(darkRatio(before) < 0.95,
                 "canvas ja esta cinza ANTES do undo (setup quebrado)");

        // Ação comandada: move a camada de imagem.
        const AffineTransform t = doc->findLayer(id)->transform;
        AffineTransform nt = t;
        nt.position = QPointF(130, 60);
        history.execute(std::make_unique<SetLayerTransformCommand>(
            *doc, id, t, nt));
        QVERIFY(history.undoCount() == 1);

        // UNDO — o momento do bug reportado.
        history.undo();
        QTest::qWait(50);

        const QImage after = view.grab().toImage();
        const double ratio = darkRatio(after);
        QVERIFY2(ratio < 0.95,
                 qPrintable(QStringLiteral("canvas %.0f%% cinza-escuro apos undo "
                                           "(esperado < 95%%)").arg(ratio * 100)));
    }

    void undoOfVisibilityTogglesRepaints()
    {
        CommandStack history;
        CanvasView view;
        view.resize(640, 480);
        view.show();

        auto doc = std::make_unique<Document>(100, 80);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(Qt::white);
        QVERIFY(doc->addLayer(std::move(bg)));

        view.setDocument(doc.get());
        view.setHistory(&history);
        QVERIFY(QTest::qWaitFor([&] { return view.width() > 0; }));

        const LayerId bgId = doc->rootGroup()->children.front()->id();

        // Esconde o fundo via comando, depois desfaz: o branco deve voltar.
        history.execute(std::make_unique<LayerPropertyCommand<bool>>(
            *doc, bgId, QStringLiteral("layer.visible"),
            &Document::setLayerVisible, true, false));
        QTest::qWait(30);
        history.undo();
        QTest::qWait(30);

        const QImage after = view.grab().toImage();
        QVERIFY2(darkRatio(after) < 0.95,
                 "desfazer 'esconder fundo' nao repintou o canvas");
    }
};

QTEST_MAIN(TestUndoRender)
#include "test_undo_render.moc"
