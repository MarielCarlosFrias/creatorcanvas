#include "core/Document.h"
#include "core/layers/Layer.h"
#include "core/serialization/ProjectFile.h"
#include "services/autosaveservice.h"
#include "services/settingsservice.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace cc;

class TestAutosave final : public QObject
{
    Q_OBJECT

private slots:
    void saveNowWritesRecoveryFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsService settings(dir.filePath("settings.ini"));
        AutosaveService service(&settings, dir.filePath("recovery.creatorcanvas"));

        Document doc(200, 200);
        service.setDocument(&doc);
        service.saveNow();

        QVERIFY(service.hasRecoveryFile());
    }

    void discardRecoveryRemovesFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsService settings(dir.filePath("settings.ini"));
        AutosaveService service(&settings, dir.filePath("recovery.creatorcanvas"));

        Document doc(200, 200);
        service.setDocument(&doc);
        service.saveNow();
        QVERIFY(service.hasRecoveryFile());

        service.discardRecovery();
        QVERIFY(!service.hasRecoveryFile());
    }

    void nullDocumentDoesNotSave()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsService settings(dir.filePath("settings.ini"));
        AutosaveService service(&settings, dir.filePath("recovery.creatorcanvas"));
        service.saveNow();
        QVERIFY(!service.hasRecoveryFile());
    }

    void recoveryFileLoadsAsProject()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("recovery.creatorcanvas");

        Document doc(300, 300);
        auto bg = std::make_unique<BackgroundLayer>();
        bg->fill = QColor(10, 200, 30);
        QVERIFY(doc.addLayer(std::move(bg)));
        QVERIFY(saveDocument(doc, path, nullptr));

        QString error;
        auto loaded = loadDocument(path, &error);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->width(), 300);
    }
};

QTEST_GUILESS_MAIN(TestAutosave)
#include "test_autosave.moc"
