/// Testa o comportamento do painel direito (abas Camadas/Texto/Shape/Image):
/// seleccionar uma camada NÃO deve trocar a aba activa — o utilizador
/// permanece onde está até carregar ele próprio noutra aba.
#include <QtTest>
#include <QDockWidget>
#include <QMouseEvent>
#include <QSpinBox>
#include <QSlider>
#include <QTabBar>
#include <QPainter>
#include <QImage>
#include <QTemporaryDir>

#include "core/Document.h"
#include "core/document/NewDocumentSpec.h"
#include "ui/imageinspector.h"
#include "ui/adjustmentsection.h"
#include "ui/layercompsdialog.h"
#include "ui/adjustmentsdialog.h"
#include "ui/webslicesdialog.h"
#include "rendering/CanvasRenderer.h"
#include "rendering/IRenderer.h"
#include "ui/canvasview.h"
#include "ui/slicepreviewwidget.h"
#include "core/document/WebSlice.h"
#include "imageio/DocumentExporter.h"
#include "core/layers/Layer.h"
#include "ui/mainwindow.h"
#include "localization/i18nservice.h"
#include "services/settingsservice.h"

using namespace cc;

class TestMainWindowTabs final : public QObject
{
    Q_OBJECT

private slots:
    void selectingLayerKeepsCurrentInspectorTab()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        i18n.loadAvailableLanguages();

        MainWindow window(&settings, &i18n, nullptr,
                          /*suppressStartupInteractions=*/true);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto findDock = [&window](const char* name) {
            return window.findChild<QDockWidget*>(QString::fromLatin1(name));
        };
        QDockWidget* layersDock = findDock("LayersDock");
        QDockWidget* textDock = findDock("TextDock");
        QVERIFY(layersDock != nullptr);
        QVERIFY(textDock != nullptr);

        // Camadas de vários tipos.
        Document* doc = window.document();
        QVERIFY(doc != nullptr);

        auto text = std::make_unique<TextLayer>();
        text->content = QStringLiteral("Texto");
        const LayerId textId = text->id();
        QVERIFY(doc->addLayer(std::move(text)));

        auto shape = std::make_unique<ShapeLayer>();
        shape->fill = QColor(Qt::cyan);
        const LayerId shapeId = shape->id();
        QVERIFY(doc->addLayer(std::move(shape)));

        auto image = std::make_unique<ImageLayer>();
        image->naturalWidth = 100;
        image->naturalHeight = 100;
        const LayerId imageId = image->id();
        QVERIFY(doc->addLayer(std::move(image)));

        // Os docks começam ocultos em uma janela nova. Tornamo-los visíveis
        // como na área de edição e deixamos "Camadas" como aba actual.
        layersDock->show();
        textDock->show();
        layersDock->raise();
        QCoreApplication::processEvents();
        QVERIFY(layersDock->isVisible());

        QTabBar* inspectorTabs = nullptr;
        for (QTabBar* tabs : window.findChildren<QTabBar*>()) {
            bool hasLayers = false;
            bool hasText = false;
            for (int i = 0; i < tabs->count(); ++i) {
                hasLayers = hasLayers || tabs->tabText(i) == layersDock->windowTitle();
                hasText = hasText || tabs->tabText(i) == textDock->windowTitle();
            }
            if (hasLayers && hasText) {
                inspectorTabs = tabs;
                break;
            }
        }
        QVERIFY(inspectorTabs != nullptr);
        QCOMPARE(inspectorTabs->tabText(inspectorTabs->currentIndex()),
                 layersDock->windowTitle());

        // Seleccionar uma camada de TEXTO não pode trocar a aba.
        window.selectLayer(textId);
        QCoreApplication::processEvents();
        QCOMPARE(inspectorTabs->tabText(inspectorTabs->currentIndex()),
                 layersDock->windowTitle());

        // Idem para forma e imagem.
        window.selectLayer(shapeId);
        QCoreApplication::processEvents();
        QCOMPARE(inspectorTabs->tabText(inspectorTabs->currentIndex()),
                 layersDock->windowTitle());

        window.selectLayer(imageId);
        QCoreApplication::processEvents();
        QCOMPARE(inspectorTabs->tabText(inspectorTabs->currentIndex()),
                 layersDock->windowTitle());

        // Se o utilizador mudar de aba manualmente, aí sim muda.
        textDock->raise();
        QCoreApplication::processEvents();
        QCOMPARE(inspectorTabs->tabText(inspectorTabs->currentIndex()),
                 textDock->windowTitle());
    }
    void adjustmentsDialogProvidesRequest()
    {
        // Diálogo em modo headless: sem inferência nem interacção, o pedido
        // sai válido e não-destrutivo por omissão (contrato usado pelo
        // MainWindow::openAdjustments).
        QImage src(32, 24, QImage::Format_ARGB32);
        src.fill(qRgba(110, 130, 150, 255));

        AdjustmentsDialog dlg(src, AdjustmentType::Curves);
        const AdjustmentRequest req = dlg.request();
        QVERIFY(req.valid);
        QVERIFY(req.nonDestructive);
        QCOMPARE(req.type, AdjustmentType::Curves);
        // Curvas identidade: os 4 canais estão codificados (count + pontos).
        QVERIFY(!req.params.empty());
    }
    void adjustmentsDialogConstructionNeverCrashes()
    {
        // Regressão real: abrir o diálogo directamente em "Níveis" crashava —
        // os sliders da página emitiam valueChanged durante a construção,
        // antes de m_gamma existir. Todos os tipos iniciais passam por aqui.
        QImage src(64, 48, QImage::Format_ARGB32);
        src.fill(qRgba(110, 130, 150, 255));

        for (cc::AdjustmentType type : {cc::AdjustmentType::BrightnessContrast,
                                       cc::AdjustmentType::HueSaturation,
                                       cc::AdjustmentType::ShadowsHighlights,
                                       cc::AdjustmentType::Vignette,
                                       cc::AdjustmentType::Invert,
                                       cc::AdjustmentType::Levels,
                                       cc::AdjustmentType::Curves,
                                       cc::AdjustmentType::ChannelMixer,
                                       cc::AdjustmentType::GradientMap}) {
            AdjustmentsDialog dlg(src, type, nullptr, nullptr, /*advancedMode=*/true);
            const AdjustmentRequest req = dlg.request();
            QVERIFY(req.valid);
            QCOMPARE(req.type, type);
        }

        // Sem modo avançado não há histograma nem crash.
        AdjustmentsDialog basic(src, cc::AdjustmentType::Levels);
        QVERIFY(basic.request().valid);
    }

    void slicePreviewDragWritesBackToFields()
    {
        // Modo avançado: arrastar o pendente na prévia tem de escrever as
        // coordenadas de volta nos campos — é a razão de ser da prévia.
        Document doc(480, 360);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(35, 65, 115);
        QVERIFY(doc.addLayer(std::move(bg)));

        WebSlicesDialog dlg(doc, nullptr, LayerId(), nullptr, nullptr, /*advancedMode=*/true);
        dlg.show();
        QTest::qWait(120);

        // Identificar os campos pelos valores iniciais (X=Y=0, L=640, A=360).
        QList<QSpinBox*> spins = dlg.findChildren<QSpinBox*>();
        QVERIFY(spins.size() >= 4);
        QSpinBox* x = nullptr; QSpinBox* y = nullptr;
        QSpinBox* w = nullptr; QSpinBox* h = nullptr;
        for (QSpinBox* s : spins) {
            const int v = s->value();
            if (v == 640 && !w) w = s;
            else if (v == 360 && !h) h = s;
            else if (v == 0 && !x) x = s;
            else if (v == 0 && !y) y = s;
        }
        QVERIFY2(w && h && x && y, "campos X/Y/L/A não encontrados");

        // Arrastar o pendente pelo centro da prévia.
        auto* preview = dlg.findChild<SlicePreviewWidget*>();
        QVERIFY(preview);

        // Pendente sensato (cabe no documento) antes de arrastar: o default
        // 640x360 num documento 480x360 gera coordenadas negativas ao
        // arrastar, que o range do spin clampava a zero.
        x->setValue(40);
        y->setValue(30);
        w->setValue(200);
        h->setValue(100);
        QTest::qWait(30);

        // O centro tem de ser lido DEPOIS de mexer nos campos — o rect do
        // pendente mudou de tamanho.
        const QRectF before = preview->pendingWidgetRect();
        QVERIFY(!before.isNull());
        const QPoint center = before.center().toPoint();

        const int xBefore = x->value();
        const int yBefore = y->value();
        const int wBefore = w->value();
        const int hBefore = h->value();

        QTest::mousePress(preview, Qt::LeftButton, Qt::NoModifier, center);
        QTest::mouseMove(preview, center + QPoint(40, 25));
        QTest::mouseRelease(preview, Qt::LeftButton, Qt::NoModifier, center + QPoint(40, 25));

        // Os campos passaram a reflectir o arraste: posição mudou, tamanho
        // ficou igual (arrastar pelo centro move, não redimensiona).
        QVERIFY2(x->value() != xBefore || y->value() != yBefore,
                 qPrintable(QStringLiteral("arraste devia mudar X/Y: %1,%2 -> %3,%4")
                            .arg(xBefore).arg(yBefore).arg(x->value()).arg(y->value())));
        QCOMPARE(w->value(), wBefore);
        QCOMPARE(h->value(), hBefore);
    }









    void imageInspectorLiveAdjustmentsCreateLinkedLayer()
    {
        // A secção de ajustes do inspector (modo avançado) tem de criar uma
        // camada de ajuste LIGADA à imagem — nunca cozer pixéis.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        SettingsService settings(tmp.filePath("settings.json"));
        I18nService i18n(&settings);
        QVERIFY(i18n.loadAvailableLanguages());

        Document doc(64, 48);
        QImage img(64, 48, QImage::Format_ARGB32);
        img.fill(qRgba(120, 130, 140, 255));
        const LayerId asset = doc.assets().addImage(img, QStringLiteral("png"));
        auto layer = std::make_unique<ImageLayer>();
        layer->assetId = asset;
        layer->naturalWidth = 64;
        layer->naturalHeight = 48;
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));
        CommandStack history;

        ImageInspector inspector(&i18n, &doc, &history);
        inspector.setAdvancedMode(true);
        inspector.setSelectedLayer(id);

        auto* section = inspector.findChild<AdjustmentsSection*>();
        QVERIFY(section != nullptr);
        QVERIFY(section->isVisibleTo(&inspector));

        auto* slider = section->findChild<QSlider*>();
        QVERIFY(slider != nullptr);
        slider->setValue(30);
        QTest::qWait(20);

        const auto* imgLayer = dynamic_cast<const ImageLayer*>(doc.findLayer(id));
        QVERIFY(imgLayer != nullptr);
        QVERIFY2(imgLayer->adjustmentLayerId.has_value(),
                 "mover o slider devia criar a camada de ajuste ligada");
        const AdjustmentLayer* adj = doc.findAdjustmentLayer(*imgLayer->adjustmentLayerId);
        QVERIFY(adj != nullptr);
        QCOMPARE(adj->kind, AdjustmentType::BrightnessContrast);
        QVERIFY(!adj->parameters.empty());
        QCOMPARE(imgLayer->assetId, asset);
        QCOMPARE(history.undoCount(), 1);
    }


    void canvasViewCachedRenderMatchesWithViewportTransform()
    {
        // Reprodução do relato: com uma CanvasView REAL (transformação de
        // viewport, dispositivo widget), o render em cache tem de ser
        // idêntico ao sem cache — se um pixmap for colado no sítio ou no
        // tamanho errado, a imagem "salta" no canvas.
        Document doc(900, 600);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(40, 50, 80);
        doc.addLayer(std::move(bg));

        // Foto grande (como uma importação real).
        QImage photo(1200, 800, QImage::Format_ARGB32);
        photo.fill(qRgba(80, 140, 200, 255));
        const LayerId asset = doc.assets().addImage(photo, QStringLiteral("png"));
        auto layer = std::make_unique<ImageLayer>();
        layer->assetId = asset;
        layer->naturalWidth = 1200;
        layer->naturalHeight = 800;
        layer->transform.position = QPointF(450, 300);
        doc.addLayer(std::move(layer));

        // Texto por cima (também cacheável) para haver 2 camadas em jogo.
        auto text = std::make_unique<TextLayer>();
        text->content = QStringLiteral("TESTE");
        text->sizePt = 96;
        text->transform.position = QPointF(450, 150);
        doc.addLayer(std::move(text));

        CanvasView view;
        view.setDocument(&doc);
        view.resize(1200, 800);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTest::qWait(200);

        // Aquecer a cache e capturar.
        view.repaint();
        QTest::qWait(50);
        const QPixmap warm = view.grab();

        // Limpar e capturar de novo (referência sem cache).
        clearRendererCaches();
        view.repaint();
        QTest::qWait(50);
        const QPixmap cold = view.grab();

        const QImage a = warm.toImage().convertToFormat(QImage::Format_ARGB32);
        const QImage b = cold.toImage().convertToFormat(QImage::Format_ARGB32);
        QCOMPARE(a.size(), b.size());
        int diff = 0;
        for (int y = 0; y < a.height(); ++y)
            for (int x = 0; x < a.width(); ++x)
                if (a.pixelColor(x, y) != b.pixelColor(x, y))
                    ++diff;
        qInfo() << "pixeis diferentes (cache vs sem cache):" << diff << "de"
                << a.width() * a.height();
        QVERIFY2(diff == 0, "o render em cache devia ser idêntico ao sem cache");
    }

    void layerCompsDialogRoundTrip()
    {
        // Ponta a ponta: guardar o estado, mudar o documento, aplicar a
        // composição (restaura) e remover — tudo pelo diálogo, com undo.
        Document doc(100, 80);
        auto shape = std::make_unique<ShapeLayer>();
        shape->kind = ShapeKind::Rectangle;
        shape->fill = QColor(40, 180, 60);
        shape->points = QPolygonF({QPointF(0, 0), QPointF(40, 30)});
        shape->transform.position = QPointF(50, 40);
        shape->visible = true;
        const LayerId id = shape->id();
        QVERIFY(doc.addLayer(std::move(shape)));
        CommandStack history;

        LayerCompsDialog dlg(doc, &history);
        const QString compId = dlg.saveCompWithName(QStringLiteral("Original"));
        QVERIFY(!compId.isEmpty());
        QCOMPARE(doc.layerComps().size(), 1u);

        // Mover e esconder a camada.
        doc.setLayerVisible(id, false);
        AffineTransform moved = doc.findLayer(id)->transform;
        moved.position = QPointF(20, 20);
        doc.setLayerTransform(id, moved);

        // Aplicar a composição tem de repor visibilidade e posição.
        dlg.applyCompById(compId);
        const Layer* restored = doc.findLayer(id);
        QVERIFY(restored != nullptr);
        QVERIFY(restored->visible);
        QCOMPARE(restored->transform.position, QPointF(50, 40));

        // Undo desfaz a aplicação (volta ao estado mexido).
        history.undo();
        QVERIFY(!doc.findLayer(id)->visible);
        QCOMPARE(doc.findLayer(id)->transform.position, QPointF(20, 20));

        // Remover a composição.
        dlg.removeCompById(compId);
        QVERIFY(doc.layerComps().empty());
        history.undo();
        QCOMPARE(doc.layerComps().size(), 1u);
    }
};


QTEST_MAIN(TestMainWindowTabs)
#include "test_mainwindow_tabs.moc"




