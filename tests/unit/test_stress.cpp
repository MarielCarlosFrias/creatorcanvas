#include <QtTest>
#include <QTemporaryDir>
#include <QImage>

#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "core/layers/Layer.h"
#include "core/image/AdjustmentEngine.h"
#include "core/serialization/ProjectFile.h"
#include "services/packstore.h"
#include "ui/mainwindow.h"
#include "localization/i18nservice.h"
#include "services/settingsservice.h"

using namespace cc;

/// Suíte de stress: percorre caminhos reais (documento, camadas, undo/redo,
/// ajustes, save/reload, pacotes, janela) para expor erros de memória e
/// lógica. Correr sob ASan.
class TestStress final : public QObject
{
    Q_OBJECT

private slots:
    static void addText(Document& doc, const QString& name)
    {
        auto t = std::make_unique<TextLayer>();
        t->name = name;
        t->content = QStringLiteral("Stress");
        t->sizePt = 40;
        doc.addLayer(std::move(t));
    }

    void layerLifecycle()
    {
        Document doc(1200, 800);
        CommandStack history;
        addText(doc, QStringLiteral("A"));
        addText(doc, QStringLiteral("B"));
        QCOMPARE(doc.rootGroup()->children.size(), std::size_t(2));

        for (int i = 0; i < 20; ++i) {
            auto t = std::make_unique<TextLayer>();
            t->name = QStringLiteral("H%1").arg(i);
            history.execute(std::make_unique<AddLayerCommand>(doc, std::move(t)));
        }
        history.undo();
        history.undo();
        history.redo();
        QVERIFY(doc.rootGroup() != nullptr);
        QVERIFY(!doc.rootGroup()->children.empty());
    }

    void selectionRoundTrip()
    {
        Document doc(800, 600);
        QImage mask(800, 600, QImage::Format_Alpha8);
        mask.fill(0);
        for (int y = 100; y < 500; ++y)
            for (int x = 100; x < 700; ++x)
                mask.scanLine(y)[x] = 255;
        doc.setPixelSelection(mask);
        QVERIFY(!doc.pixelSelection().isNull());
        doc.clearPixelSelection();
        QVERIFY(doc.pixelSelection().isNull());
    }

    void adjustmentRoundTrip()
    {
        QImage src(400, 300, QImage::Format_ARGB32_Premultiplied);
        src.fill(Qt::blue);
        const AdjustmentType types[] = {AdjustmentType::BrightnessContrast,
                                        AdjustmentType::HueSaturation,
                                        AdjustmentType::Levels,
                                        AdjustmentType::Curves,
                                        AdjustmentType::Threshold,
                                        AdjustmentType::Invert};
        for (AdjustmentType t : types) {
            AdjustmentParams params;
            params.emplace_back(QStringLiteral("strength"), 1.0);
            const QImage out = applyAdjustmentParameters(src, t, params);
            Q_UNUSED(out);
        }
    }

    void saveOpenRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Document doc(1000, 700);
        addText(doc, QStringLiteral("Salvo"));

        const QString path = dir.filePath(QStringLiteral("stress.creatorcanvas"));
        QImage thumb(320, 180, QImage::Format_ARGB32);
        thumb.fill(Qt::red);
        QString err;
        QVERIFY(saveDocument(doc, path, &err, &thumb));
        auto loaded = loadDocument(path, &err);
        QVERIFY(loaded != nullptr);
        QVERIFY(!loadProjectThumbnail(path).isNull());
    }

    void packItemsAreUsable()
    {
        const QVector<PackItem> items = PackStore::allItems();
        QVERIFY(!items.isEmpty());
        for (const PackItem& item : items) {
            const QSize natural = item.naturalSize();
            if (natural.isValid())
                QVERIFY(natural.width() > 0 && natural.height() > 0);
            QVERIFY(!item.resource.isEmpty());
            QVERIFY(!item.labelKey.isEmpty());
        }
    }

    void windowLifecycle()
    {
        QTemporaryDir dir;
        SettingsService settings(dir.filePath("s.ini"));
        I18nService i18n(&settings, {QStringLiteral(":/locales")});
        QVERIFY(i18n.loadAvailableLanguages());

        MainWindow window(&settings, &i18n, nullptr, true);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(50);
        window.close();
        QCoreApplication::processEvents();
        QTest::qWait(30);
    }
};

QTEST_MAIN(TestStress)
#include "test_stress.moc"
