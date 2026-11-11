/// Testa o comportamento do painel direito (abas Camadas/Texto/Shape/Image):
/// seleccionar uma camada NÃO deve trocar a aba activa — o utilizador
/// permanece onde está até carregar ele próprio noutra aba.
#include <QtTest>
#include <QDockWidget>
#include <QTabBar>
#include <QPainter>
#include <QImage>

#include "core/Document.h"
#include "ui/adjustmentsdialog.h"
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
};

QTEST_MAIN(TestMainWindowTabs)
#include "test_mainwindow_tabs.moc"

