#include "core/Document.h"
#include "core/document/NewDocumentSpec.h"
#include "services/presetstore.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace cc;

class TestNewDocument final : public QObject
{
    Q_OBJECT

private slots:
    void createSolidBackgroundDocument()
    {
        NewDocumentSpec spec;
        spec.width = 1280;
        spec.height = 720;
        spec.dpi = 96;
        spec.backgroundColor = QColor(255, 0, 0);

        auto doc = createDocument(spec);
        QVERIFY(doc);
        QCOMPARE(doc->width(), 1280);
        QCOMPARE(doc->height(), 720);
        QCOMPARE(doc->dpi(), 96);
        QCOMPARE(doc->rootGroup()->children.size(), std::size_t(1));

        auto* bg = static_cast<BackgroundLayer*>(
            doc->rootGroup()->children[0].get());
        QCOMPARE(bg->type(), LayerType::Background);
        QCOMPARE(bg->name, QStringLiteral("Background"));
        QCOMPARE(bg->fill, QColor(255, 0, 0));
    }

    void createTransparentDocumentHasNoLayers()
    {
        NewDocumentSpec spec;
        spec.transparentBackground = true;
        auto doc = createDocument(spec);
        QVERIFY(doc);
        QCOMPARE(doc->width(), 1280);
        QCOMPARE(doc->rootGroup()->children.size(), std::size_t(0));
    }

    void builtInPresetsAreLoaded()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PresetStore store(dir.filePath("presets.json"));
        QVERIFY(store.all().size() >= 10);

        bool found = false;
        for (const DocumentPreset& preset : store.all()) {
            if (preset.name == QLatin1String("YouTube Thumbnail")) {
                found = true;
                QCOMPARE(preset.spec.width, 1280);
                QCOMPARE(preset.spec.height, 720);
            }
        }
        QVERIFY(found);
    }

    void addUserPresetPersists()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("presets.json");
        {
            PresetStore store(path);
            DocumentPreset preset;
            preset.name = QStringLiteral("My Size");
            preset.spec.width = 800;
            preset.spec.height = 600;
            preset.spec.dpi = 72;
            QVERIFY(store.addUserPreset(preset));
            QVERIFY(!store.addUserPreset(preset)); // duplicate rejected
        }

        PresetStore reloaded(path);
        bool found = false;
        for (const DocumentPreset& preset : reloaded.all()) {
            if (preset.name == QLatin1String("My Size")) {
                found = true;
                QCOMPARE(preset.spec.dpi, 72);
            }
        }
        QVERIFY(found);
    }

    void cannotRemoveBuiltIn()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PresetStore store(dir.filePath("presets.json"));
        const int before = store.all().size();
        QVERIFY(!store.removeUserPreset(QStringLiteral("YouTube Thumbnail")));
        QCOMPARE(store.all().size(), before);
    }

    void removeUserPreset()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PresetStore store(dir.filePath("presets.json"));

        DocumentPreset preset;
        preset.name = QStringLiteral("Temp");
        preset.spec.width = 10;
        preset.spec.height = 10;
        QVERIFY(store.addUserPreset(preset));

        QVERIFY(store.removeUserPreset(QStringLiteral("Temp")));
        QVERIFY(!store.removeUserPreset(QStringLiteral("Temp")));

        for (const DocumentPreset& existing : store.all())
            QVERIFY(existing.name != QLatin1String("Temp"));
    }
};

QTEST_GUILESS_MAIN(TestNewDocument)
#include "test_newdocument.moc"
