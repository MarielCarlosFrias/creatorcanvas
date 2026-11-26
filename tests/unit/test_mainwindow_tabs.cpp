/// Testa o comportamento do painel direito (abas Camadas/Texto/Shape/Image):
/// seleccionar uma camada NÃO deve trocar a aba activa — o utilizador
/// permanece onde está até carregar ele próprio noutra aba.
#include <QtTest>
#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QDockWidget>
#include <QMouseEvent>
#include <QSpinBox>
#include <QSlider>
#include <QTabBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QLayout>
#include <QPainter>
#include <QImage>
#include <QTemporaryDir>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QStatusBar>
#include <QToolButton>

#include "core/Document.h"
#include "core/document/NewDocumentSpec.h"
#include "core/serialization/ProjectFile.h"
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
#include "ui/onboardingpanel.h"
#include "ui/packbrowser.h"
#include <QComboBox>
#include <QListWidget>
#include "ui/designtokens.h"
#include "ui/collapsiblesection.h"
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

    void imageInspectorAdvancedAdjustmentsAreInteractive()
    {
        Document doc(64, 48);
        QImage image(64, 48, QImage::Format_ARGB32);
        image.fill(qRgba(120, 140, 160, 255));
        const LayerId assetId = doc.assets().addImage(image, QStringLiteral("png"));
        auto layer = std::make_unique<ImageLayer>();
        layer->assetId = assetId;
        layer->naturalWidth = 64;
        layer->naturalHeight = 48;
        const LayerId imageId = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));

        CommandStack history;
        ImageInspector inspector(nullptr, &doc, &history);
        inspector.setAdvancedMode(true);
        inspector.setSelectedLayer(imageId);
        QCoreApplication::processEvents();

        auto* section = inspector.findChild<AdjustmentsSection*>();
        QVERIFY(section != nullptr);
        const auto sliders = section->findChildren<QSlider*>();
        QCOMPARE(sliders.size(), 4);
        sliders.at(0)->setValue(20);
        QCoreApplication::processEvents();

        const auto* selected = dynamic_cast<const ImageLayer*>(doc.findLayer(imageId));
        QVERIFY(selected != nullptr);
        QVERIFY(selected->adjustmentLayerId.has_value());
        QVERIFY(doc.findAdjustmentLayer(*selected->adjustmentLayerId) != nullptr);
    }

    void adjustmentSliderThenCanvasRepaint()
    {
        // Fluxo suspeito do crash em release: aplicar ajuste na aba Imagem e
        // depois repintar o canvas de verdade (paint do widget, não só
        // renderDocument directo).
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        i18n.loadAvailableLanguages();

        MainWindow window(&settings, &i18n, nullptr, /*suppressStartupInteractions=*/true);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        Document* doc = window.document();
        QVERIFY(doc != nullptr);
        QImage img(320, 240, QImage::Format_ARGB32);
        img.fill(QColor(90, 120, 160, 255));
        auto layer = std::make_unique<ImageLayer>();
        layer->assetId = doc->assets().addImage(img);
        layer->naturalWidth = 320;
        layer->naturalHeight = 240;
        layer->transform.position = QPointF(doc->width() / 2.0, doc->height() / 2.0);
        const LayerId imageId = layer->id();
        QVERIFY(doc->addLayer(std::move(layer)));
        window.selectLayer(imageId);
        QCoreApplication::processEvents();

        auto* inspector = window.findChild<ImageInspector*>();
        QVERIFY(inspector != nullptr);
        inspector->setAdvancedMode(true);
        inspector->setSelectedLayer(imageId);
        QCoreApplication::processEvents();

        auto* section = inspector->findChild<AdjustmentsSection*>();
        QVERIFY(section != nullptr);
        const auto sliders = section->findChildren<QSlider*>();
        QVERIFY(sliders.size() >= 4);

        auto* canvas = window.findChild<CanvasView*>();
        QVERIFY(canvas != nullptr);

        // Simula arrastar o slider de brilho várias vezes com repaint real.
        for (int v = -100; v <= 100; v += 25) {
            sliders.at(0)->setValue(v);
            QCoreApplication::processEvents();
            canvas->update();
            QCoreApplication::processEvents();
            window.grab(); // força paintEvent completo
        }

        // E um undo/redo em cima disso.
        for (int i = 0; i < 9; ++i) {
            QKeyEvent undo(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
            QApplication::sendEvent(&window, &undo);
            QCoreApplication::processEvents();
            window.grab();
        }
        for (int i = 0; i < 9; ++i) {
            QKeyEvent redo(QEvent::KeyPress, Qt::Key_Y, Qt::ControlModifier);
            QApplication::sendEvent(&window, &redo);
            QCoreApplication::processEvents();
            window.grab();
        }
    }

    void inspectorDockAdaptsToTextMetrics()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        QVERIFY(i18n.loadAvailableLanguages());
        QVERIFY(i18n.setLanguage(QStringLiteral("sv")));

        const QFont originalFont = QApplication::font();
        QFont largeFont = originalFont;
        largeFont.setPointSize(originalFont.pointSize() + 4);
        QApplication::setFont(largeFont);
        struct FontRestorer {
            QFont font;
            ~FontRestorer() { QApplication::setFont(font); }
        } fontRestorer{originalFont};

        MainWindow window(&settings, &i18n, nullptr,
                          /*suppressStartupInteractions=*/true);
        window.createDemoDocumentForSnapshot();
        window.resize(900, 700);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto* textDock = window.findChild<QDockWidget*>(QStringLiteral("TextDock"));
        QVERIFY(textDock != nullptr);
        textDock->show();
        textDock->raise();
        bool textTabActivated = false;
        for (auto* tabs : window.findChildren<QTabBar*>()) {
            if (!tabs->isVisibleTo(&window))
                continue;
            for (int i = 0; i < tabs->count(); ++i) {
                if (tabs->tabText(i) == textDock->windowTitle()) {
                    tabs->setCurrentIndex(i);
                    textTabActivated = true;
                }
            }
        }
        QVERIFY(textTabActivated);
        QTest::qWait(20);

        auto* scroll = textDock->widget()->findChild<QScrollArea*>();
        QVERIFY(scroll != nullptr);
        for (auto* section : textDock->widget()->findChildren<CollapsibleSection*>())
            section->setExpanded(true);
        scroll->widget()->layout()->activate();
        QCoreApplication::processEvents();

        const int contentWidth = scroll->widget()->minimumSizeHint().width();
        const int requiredWidth = textDock->widget()->minimumSizeHint().width();
        QVERIFY(requiredWidth > Design::DockMin);
        QVERIFY(requiredWidth >= contentWidth
                                  + scroll->verticalScrollBar()->sizeHint().width());
        QCOMPARE(textDock->minimumWidth(),
                 qMax(Design::dockWidth(window.width()), requiredWidth));
        QVERIFY(textDock->width() >= requiredWidth);

        // The dock layout reserves the complete content and vertical bar width.
        QVERIFY(textDock->minimumSizeHint().width() >= requiredWidth);
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

    void webSlicesDialogRoundTrip()
    {
        // Ponta a ponta: criar fatias (numérica e a partir da selecção),
        // verificar o rect em espaço de documento, exportar uma e remover.
        Document doc(200, 160);
        QImage photo(100, 80, QImage::Format_ARGB32);
        photo.fill(qRgba(50, 200, 50, 255));
        const LayerId asset = doc.assets().addImage(photo, QStringLiteral("png"));
        auto layer = std::make_unique<ImageLayer>();
        layer->assetId = asset;
        layer->naturalWidth = 100;
        layer->naturalHeight = 80;
        layer->transform.position = QPointF(100, 80);   // centro do documento
        const LayerId id = layer->id();
        QVERIFY(doc.addLayer(std::move(layer)));
        CommandStack history;

        WebSlicesDialog dlg(doc, &history, id);

        // 1) Numérica.
        const QString s1 = dlg.addSliceNumeric(10, 20, 60, 40, QStringLiteral("png"),
                                               QStringLiteral("thumb"));
        QVERIFY(!s1.isEmpty());
        QCOMPARE(doc.webSlices().size(), 1u);
        QCOMPARE(doc.webSlices().front().rect, QRectF(10, 20, 60, 40));

        // 2) A partir da selecção: quadrado 20x20 no bitmap (centro),
        // que deve cair no centro do documento (100,80).
        QImage sel(100, 80, QImage::Format_Alpha8);
        sel.fill(0);
        for (int y = 30; y < 50; ++y) {
            uchar* row = sel.scanLine(y);
            for (int x = 40; x < 60; ++x)
                row[x] = 255;
        }
        doc.setPixelSelection(sel);
        const QString s2 = dlg.addSliceFromSelection();
        QVERIFY(!s2.isEmpty());
        QCOMPARE(doc.webSlices().size(), 2u);
        const QRectF selSlice = doc.webSlices().back().rect;
        // A camada está centrada em (100,80) e o bitmap é 100x80: o centro
        // da selecção (pixel 50,40) cai exactamente no centro do documento.
        QVERIFY2(qAbs(selSlice.center().x() - 100.0) < 2.0
                 && qAbs(selSlice.center().y() - 80.0) < 2.0,
                 qPrintable(QStringLiteral("fatia da selecção no sítio errado: (%1,%2 %3x%4)")
                            .arg(selSlice.x()).arg(selSlice.y())
                            .arg(selSlice.width()).arg(selSlice.height())));

        // 3) Undo/redo da criação.
        history.undo();
        QCOMPARE(doc.webSlices().size(), 1u);
        history.redo();
        QCOMPARE(doc.webSlices().size(), 2u);

        // 4) Exportar uma fatia para ficheiro real.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString out = tmp.filePath(QStringLiteral("thumb.png"));
        QString error;
        QVERIFY2(cc::exportSliceToImage(doc, doc.webSlices().front().rect, out,
                                        QStringLiteral("png"), -1, 1.0, &error),
                 qPrintable(error));
        QVERIFY(QFile::exists(out));

        // 5) Remover.
        QVERIFY(dlg.removeSliceById(s1));
        QCOMPARE(doc.webSlices().size(), 1u);
        history.undo();
        QCOMPARE(doc.webSlices().size(), 2u);
    }

    void pasteIntoWithEmptyClipboardDoesNotTouchSelectedLayer()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        i18n.loadAvailableLanguages();
        MainWindow window(&settings, &i18n, nullptr, true);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        Document* doc = window.document();
        QVERIFY(doc != nullptr);
        QImage pixels(32, 32, QImage::Format_ARGB32);
        pixels.fill(QColor(100, 120, 140, 255));
        auto layer = std::make_unique<ImageLayer>();
        layer->assetId = doc->assets().addImage(pixels);
        layer->naturalWidth = 32;
        layer->naturalHeight = 32;
        layer->transform.position = QPointF(50, 50);
        const LayerId id = layer->id();
        QVERIFY(doc->addLayer(std::move(layer)));
        window.selectLayer(id);

        QImage selection(doc->width(), doc->height(), QImage::Format_Alpha8);
        selection.fill(255);
        doc->setPixelSelection(selection);
        QGuiApplication::clipboard()->clear();
        const size_t beforeCount = doc->rootGroup()->children.size();

        QAction* pasteInto = nullptr;
        for (QAction* action : window.findChildren<QAction*>())
            if (action->shortcut() == QKeySequence(QStringLiteral("Ctrl+Shift+V"))) {
                pasteInto = action;
                break;
            }
        QVERIFY(pasteInto != nullptr);
        pasteInto->trigger();
        QCoreApplication::processEvents();

        // No-op total: não cola e, sobretudo, não aplica máscara à camada atual.
        QCOMPARE(doc->rootGroup()->children.size(), beforeCount);
        const auto* current = dynamic_cast<const ImageLayer*>(doc->findLayer(id));
        QVERIFY(current != nullptr);
        QVERIFY(current->alphaMask.isNull());
    }

    void groupedLayerMoveFollowsPointer()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const QString project = tmpDir.filePath(QStringLiteral("legacy_group.creatorcanvas"));
        {
            Document doc(800, 600);
            QImage pixels(100, 80, QImage::Format_ARGB32);
            pixels.fill(QColor(200, 40, 40));
            auto image = std::make_unique<ImageLayer>();
            image->name = QStringLiteral("Mockup test"); // exclui snap neste teste
            image->assetId = doc.assets().addImage(pixels);
            image->naturalWidth = 100;
            image->naturalHeight = 80;
            image->transform.position = QPointF(300, 250);
            auto group = std::make_unique<GroupLayer>();
            group->name = QStringLiteral("Legacy");
            group->children.push_back(std::move(image));
            // Assinatura antiga: matriz do grupo desloca o conteúdo para o canto.
            group->transform.position = QPointF(0, 0);
            QVERIFY(doc.addLayer(std::move(group)));
            QString error;
            QImage thumb(32, 32, QImage::Format_ARGB32);
            thumb.fill(Qt::white);
            QVERIFY2(cc::saveDocument(doc, project, &error, &thumb), qPrintable(error));
        }

        SettingsService settings(tmpDir.filePath("settings.json"));
        I18nService i18n(&settings, {":/locales"});
        i18n.loadAvailableLanguages();
        MainWindow window(&settings, &i18n, nullptr, true);
        window.openFromPath(project);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        Document* doc = window.document();
        QVERIFY(doc != nullptr);
        GroupLayer* group = nullptr;
        ImageLayer* image = nullptr;
        for (const auto& child : doc->rootGroup()->children) {
            if (child->type() == LayerType::Group) {
                group = static_cast<GroupLayer*>(child.get());
                if (!group->children.empty())
                    image = dynamic_cast<ImageLayer*>(group->children.front().get());
                break;
            }
        }
        QVERIFY(group != nullptr && image != nullptr);
        // O loader migra o grupo legado: matriz identidade.
        QVERIFY((group->transform.position - group->contentBounds().center()).manhattanLength() < 0.01);

        auto* canvas = window.findChild<CanvasView*>();
        QVERIFY(canvas != nullptr);
        window.selectLayer(image->id());
        QCoreApplication::processEvents();

        const QRectF bounds = image->contentBounds();
        const QTransform imageToDoc = group->transform.matrix(group->contentBounds())
                                      * image->transform.matrix(bounds);
        const QPoint docCenter = imageToDoc.map(bounds.center()).toPoint();
        const QPoint widgetCenter = canvas->docToDevice().map(docCenter);
        const QPointF oldPosition = image->transform.position;

        // Desativa o snap para testar apenas o espaço do grupo.
        canvas->setSnapToGrid(false);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, widgetCenter);
        QTest::mouseMove(canvas, widgetCenter + QPoint(60, 30));
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier,
                            widgetCenter + QPoint(60, 30));

        const QPointF moved = image->transform.position - oldPosition;
        const QPointF expected = canvas->deviceToDoc().map(widgetCenter + QPoint(60, 30))
                                 - canvas->deviceToDoc().map(widgetCenter);
        QVERIFY2(qAbs(moved.x() - expected.x()) < 1.0
                 && qAbs(moved.y() - expected.y()) < 1.0,
                 qPrintable(QStringLiteral("delta inesperado: (%1,%2), esperado: (%3,%4)")
                                .arg(moved.x()).arg(moved.y())
                                .arg(expected.x()).arg(expected.y())));
    }

    void legacyGroupMoveKeepsLayerAndHandlesAligned()
    {
        Document doc(800, 600);
        CommandStack history;
        QImage pixels(100, 80, QImage::Format_ARGB32);
        pixels.fill(QColor(200, 40, 40));
        auto image = std::make_unique<ImageLayer>();
        image->name = QStringLiteral("Mockup test");
        image->assetId = doc.assets().addImage(pixels);
        image->naturalWidth = 100;
        image->naturalHeight = 80;
        image->transform.position = QPointF(300, 250);
        const LayerId imageId = image->id();
        auto group = std::make_unique<GroupLayer>();
        group->name = QStringLiteral("Legacy");
        group->children.push_back(std::move(image));
        group->transform.position = QPointF(0, 0);
        GroupLayer* groupPtr = group.get();
        QVERIFY(doc.addLayer(std::move(group)));

        CanvasView canvas;
        canvas.setDocument(&doc);
        canvas.setHistory(&history);
        canvas.resize(900, 650);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        canvas.setSelectedLayer(imageId);
        QCoreApplication::processEvents();

        auto* imageLayer = dynamic_cast<ImageLayer*>(doc.findLayer(imageId));
        QVERIFY(imageLayer != nullptr);
        const QRectF bounds = imageLayer->contentBounds();
        const QTransform before = imageLayer->transform.matrix(bounds)
                                  * groupPtr->transform.matrix(groupPtr->contentBounds());
        const QPointF oldFullCenter = before.map(bounds.center());
        const QPointF oldPosition = imageLayer->transform.position;
        const QPoint widgetCenter = canvas.docToDevice().map(oldFullCenter).toPoint();

        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, widgetCenter);
        QTest::mouseMove(&canvas, widgetCenter + QPoint(60, 30));
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier,
                            widgetCenter + QPoint(60, 30));

        const QTransform after = imageLayer->transform.matrix(bounds)
                                 * groupPtr->transform.matrix(groupPtr->contentBounds());
        const QPointF newFullCenter = after.map(bounds.center());
        const QPointF localDelta = imageLayer->transform.position - oldPosition;
        const QPointF fullDelta = newFullCenter - oldFullCenter;
        QVERIFY2(qAbs(fullDelta.x() - localDelta.x()) < 1.0
                 && qAbs(fullDelta.y() - localDelta.y()) < 1.0,
                 qPrintable(QStringLiteral("local=(%1,%2), composto=(%3,%4)")
                                .arg(localDelta.x()).arg(localDelta.y())
                                .arg(fullDelta.x()).arg(fullDelta.y())));
    }

    void draggingFromRulersCreatesGuides()
    {
        Document doc(800, 600);
        CommandStack history;
        CanvasView canvas;
        canvas.setDocument(&doc);
        canvas.setHistory(&history);
        canvas.resize(900, 650);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        canvas.setShowRulers(true);
        canvas.setShowGuides(true);

        // Régua superior → guia horizontal na posição de release.
        const QPoint start(200, 10);
        const QPoint end(260, 220);
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(&canvas, end);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, end);
        QCOMPARE(doc.guides().size(), size_t(1));
        QVERIFY(doc.guides()[0].horizontal);
        const double expectedY = canvas.deviceToDoc().map(end).y();
        QVERIFY(qAbs(doc.guides()[0].position - expectedY) < 1.0);

        // Régua esquerda → guia vertical.
        const QPoint startV(10, 240);
        const QPoint endV(340, 260);
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, startV);
        QTest::mouseMove(&canvas, endV);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, endV);
        QCOMPARE(doc.guides().size(), size_t(2));
        QVERIFY(!doc.guides()[1].horizontal);
        const double expectedX = canvas.deviceToDoc().map(endV).x();
        QVERIFY(qAbs(doc.guides()[1].position - expectedX) < 1.0);
    }

    void sidePanelsParticipateInCompactLayoutCycle()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        i18n.loadAvailableLanguages();
        const QString project = tmpDir.filePath("compact.creatorcanvas");
        {
            Document doc(800, 600);
            QString error;
            QImage thumb(16, 16, QImage::Format_ARGB32);
            thumb.fill(Qt::white);
            QVERIFY(cc::saveDocument(doc, project, &error, &thumb));
        }
        MainWindow window(&settings, &i18n, nullptr, true);
        window.openFromPath(project); // entra no editor, não no ecrã inicial
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto* navigator = window.findChild<QDockWidget*>(QStringLiteral("NavigatorDock"));
        auto* swatches = window.findChild<QDockWidget*>(QStringLiteral("SwatchesDock"));
        QVERIFY(navigator && swatches);
        navigator->setVisible(true);
        swatches->setVisible(true);

        window.resize(800, 520);
        QCoreApplication::processEvents();
        QVERIFY(!navigator->isVisible());
        QVERIFY(!swatches->isVisible());

        window.resize(1440, 900);
        QCoreApplication::processEvents();
        QVERIFY(navigator->isVisible());
        QVERIFY(swatches->isVisible());
    }

    void sidePanelsShowTranslateAndDestroySafely()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        i18n.loadAvailableLanguages();
        MainWindow window(&settings, &i18n, nullptr, true);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto* navigator = window.findChild<QDockWidget*>(QStringLiteral("NavigatorDock"));
        auto* swatches = window.findChild<QDockWidget*>(QStringLiteral("SwatchesDock"));
        QVERIFY(navigator != nullptr);
        QVERIFY(swatches != nullptr);
        navigator->setVisible(true);
        swatches->setVisible(true);
        QCoreApplication::processEvents();
        QVERIFY(!navigator->windowTitle().isEmpty());
        QVERIFY(!swatches->windowTitle().isEmpty());
        window.close();
        QCoreApplication::processEvents();
        // A destruição no fim do slot não pode chamar lambdas com `this` morto.
    }

    // ---- Ajuda a iniciantes: dicas vivas ----
    void beginnerHintsFollowTheDocumentState()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        QVERIFY(i18n.loadAvailableLanguages());

        MainWindow window(&settings, &i18n, nullptr,
                          /*suppressStartupInteractions=*/true);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto* tipsBar = window.findChild<QFrame*>("TipsBar");
        auto* emptyHint = window.findChild<QLabel*>("CanvasEmptyHint");
        QVERIFY(tipsBar);
        QVERIFY(emptyHint);

        // 1) Barra de dicas visível de início e dispensável (persistida).
        QVERIFY(tipsBar->isVisible());
        QVERIFY(!settings.hintsBarDismissed());
        auto* closeBtn = tipsBar->findChild<QToolButton*>();
        QVERIFY(closeBtn);
        QTest::mouseClick(static_cast<QWidget*>(closeBtn), Qt::LeftButton);
        QVERIFY(!tipsBar->isVisible());
        QVERIFY(settings.hintsBarDismissed());

        // 2) Dica do canvas vazio Some quando o documento ganha camadas.
        window.createDemoDocumentForSnapshot();
        QCoreApplication::processEvents();
        QVERIFY(window.document());
        if (window.document()->rootGroup()->children.empty()) {
            // Documento de demonstração pode vir vazio: cria uma camada.
            auto layer = std::make_unique<cc::TextLayer>();
            window.document()->addLayer(std::move(layer));
        }
        QCoreApplication::processEvents();
        QVERIFY(!window.document()->rootGroup()->children.empty());
        QVERIFY(!emptyHint->isVisible());

        // 3) Trocar de ferramenta publica uma dica na barra de estado.
        I18nService i18n2(&settings, {":/locales"});
        const QString before = window.statusBar()->currentMessage();
        Q_UNUSED(before);
        window.selectToolByName("paint");
        QCoreApplication::processEvents();
        QVERIFY(!window.statusBar()->currentMessage().isEmpty());
        window.close();
        QCoreApplication::processEvents();
    }

    // ---- Painel "Primeiros passos": fechar tem de ser definitivo ----
    void onboardingPanelDismissPersists()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        QVERIFY(i18n.loadAvailableLanguages());
        QVERIFY(!settings.onboardingDismissed());

        MainWindow window(&settings, &i18n, nullptr,
                          /*suppressStartupInteractions=*/true);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto* dock = window.findChild<QDockWidget*>("OnboardingDock");
        auto* panel = window.findChild<cc::OnboardingPanel*>();
        QVERIFY(dock && panel);
        // Entrar no editor (criar um documento) e deixar assentar: o painel
        // so aparece no editor, nunca no ecra inicial.
        window.createDemoDocumentForSnapshot();
        QCoreApplication::processEvents();
        QTest::qWait(50);
        QVERIFY(dock->isVisible());
        QVERIFY(!dock->windowTitle().isEmpty());

        auto* button = panel->findChild<QPushButton*>();
        QVERIFY(button);
        QTest::mouseClick(static_cast<QWidget*>(button), Qt::LeftButton);
        QVERIFY(!dock->isVisible());
        QVERIFY(settings.onboardingDismissed());

        // Actividade que dispara updateOnboarding() nao pode faze-lo voltar.
        window.createDemoDocumentForSnapshot();
        QCoreApplication::processEvents();
        window.selectToolByName("paint");
        QCoreApplication::processEvents();
        QVERIFY(!dock->isVisible());

        window.close();
        QCoreApplication::processEvents();
    }

    // ---- Integracao: aplicar sobreposicao cria UMA camada em Screen ----
    void packOverlayAppliesOnceWithScreenBlend()
    {
        QTemporaryDir tmpDir;
        SettingsService settings(tmpDir.path() + "/settings.json");
        I18nService i18n(&settings, {":/locales"});
        QVERIFY(i18n.loadAvailableLanguages());

        MainWindow window(&settings, &i18n, nullptr,
                          /*suppressStartupInteractions=*/true);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.createDemoDocumentForSnapshot();
        QCoreApplication::processEvents();

        Document* doc = window.document();
        QVERIFY(doc && doc->rootGroup());
        const int before = static_cast<int>(doc->rootGroup()->children.size());
        QVERIFY(before > 0);

        // Abre o navegador pelo atalho (caminho real do utilizador).
        QTest::keyClick(&window, Qt::Key_B, Qt::ControlModifier | Qt::ShiftModifier);
        QCoreApplication::processEvents();
        auto* browser = window.findChild<cc::PackBrowser*>();
        QVERIFY(browser);

        auto* combo = browser->findChild<QComboBox*>();
        auto* grid = browser->findChild<QListWidget*>();
        QVERIFY(combo && grid);
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toString() == QLatin1String("overlay")) {
                combo->setCurrentIndex(i);
                break;
            }
        }
        QVERIFY(grid->count() > 0);
        QCoreApplication::processEvents();
        QTest::qWait(50);

        // Activa pelo teclado (Enter): o caminho completo
        // navegador -> MainWindow -> importImage fica coberto.
        grid->setCurrentRow(0);
        QTest::keyClick(grid, Qt::Key_Return);
        QCoreApplication::processEvents();

        QCOMPARE(static_cast<int>(doc->rootGroup()->children.size()), before + 1);
        Layer* added = doc->rootGroup()->children.back().get();
        QCOMPARE(added->blendMode, cc::BlendMode::Screen);

        window.close();
        QCoreApplication::processEvents();
    }
};


QTEST_MAIN(TestMainWindowTabs)
#include "test_mainwindow_tabs.moc"

