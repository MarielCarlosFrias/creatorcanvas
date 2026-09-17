#include "core/Document.h"
#include "core/image/BackgroundRemover.h"
#include "core/layers/Layer.h"
#include "core/serialization/ProjectFile.h"
#include "imageio/DocumentExporter.h"
#include "localization/i18nservice.h"
#include "services/logservice.h"
#include "services/settingsservice.h"
#include "ui/darktheme.h"
#include "ui/mainwindow.h"
#include "ui/themeicons.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QImageWriter>
#include <QStandardPaths>
#include <QTemporaryDir>

#ifndef APP_VERSION
#define APP_VERSION "0.0.0-dev"
#endif

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(locales);

    QApplication app(argc, argv);

    // Must precede QSettings/QStandardPaths usage.
    QApplication::setOrganizationName(QStringLiteral("CreatorCanvas"));
    QApplication::setApplicationName(QStringLiteral("CreatorCanvas"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    cc::applyDarkTheme(app);
    app.setWindowIcon(cc::ThemeIcons::appIcon());

    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    cc::LogService::init(dataDir + QStringLiteral("/logs"));
    qInfo() << "Starting CreatorCanvas" << QApplication::applicationVersion();

    cc::SettingsService settings;
    cc::I18nService i18n(&settings);
    if (!i18n.loadAvailableLanguages())
        qWarning() << "Localization catalogs unavailable;"
                      " UI will fall back to raw keys.";

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("CreatorCanvas - Layer-based image editor for content creators"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption smokeTestOption(
        QStringLiteral("smoke-test"),
        QStringLiteral("Run quick startup smoke test headless and exit with 0 if healthy.")
    );
    parser.addOption(smokeTestOption);

    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Project file to open (.creatorcanvas)"), QStringLiteral("[file]"));

    parser.process(app);

    if (parser.isSet(smokeTestOption)) {
        qInfo() << "[SmokeTest] Starting full release self-check...";
        qInfo() << "[SmokeTest] Languages found:" << i18n.availableLanguages();
        if (i18n.availableLanguages().isEmpty()) {
            qCritical() << "[SmokeTest] FAIL: No language catalogs found!";
            return 1;
        }

        // 1. Model resolution and real inference check
        const QString model = cc::BackgroundRemover::findModelPath();
        qInfo() << "[SmokeTest] AI Model path:" << (!model.isEmpty() ? model : "none (optional)");
        if (!model.isEmpty() && cc::BackgroundRemover::isAvailable()) {
            qInfo() << "[SmokeTest] Running AI BackgroundRemover inference check...";
            cc::BackgroundRemover remover(model);
            if (remover.isLoaded()) {
                QImage testInput(64, 64, QImage::Format_ARGB32_Premultiplied);
                testInput.fill(Qt::red);
                QImage testOutput = remover.removeBackground(testInput);
                if (testOutput.isNull()) {
                    qCritical() << "[SmokeTest] FAIL: BackgroundRemover returned null image!";
                    return 2;
                }
                qInfo() << "[SmokeTest] BackgroundRemover inference OK (" << testOutput.width() << "x" << testOutput.height() << ")";
            }
        }

        // 2. Document creation, serialization and reload
        QTemporaryDir tmpDir;
        if (!tmpDir.isValid()) {
            qCritical() << "[SmokeTest] FAIL: Could not create temporary directory!";
            return 3;
        }

        cc::Document doc(1280, 720);
        auto textLayer = std::make_unique<cc::TextLayer>();
        textLayer->name = QStringLiteral("Smoke Text");
        textLayer->content = QStringLiteral("Hello Release Test");
        doc.addLayer(std::move(textLayer));

        const QString projPath = tmpDir.filePath(QStringLiteral("test_project.creatorcanvas"));
        QString saveError;
        QImage mockThumb(320, 180, QImage::Format_ARGB32);
        mockThumb.fill(Qt::blue);
        if (!cc::saveDocument(doc, projPath, &saveError, &mockThumb)) {
            qCritical() << "[SmokeTest] FAIL: saveDocument failed:" << saveError;
            return 4;
        }
        qInfo() << "[SmokeTest] Project save OK ->" << projPath;

        // Verify thumbnail extraction
        QImage extractedThumb = cc::loadProjectThumbnail(projPath);
        if (extractedThumb.isNull() || extractedThumb.width() != 320 || extractedThumb.height() != 180) {
            qCritical() << "[SmokeTest] FAIL: Thumbnail extraction failed or size mismatch!";
            return 5;
        }
        qInfo() << "[SmokeTest] Thumbnail check OK (" << extractedThumb.width() << "x" << extractedThumb.height() << ")";

        // Reload project
        QString loadError;
        auto reloaded = cc::loadDocument(projPath, &loadError);
        if (!reloaded || reloaded->rootGroup()->children.size() != 1) {
            qCritical() << "[SmokeTest] FAIL: loadDocument failed or layer count mismatch:" << loadError;
            return 6;
        }
        qInfo() << "[SmokeTest] Project reload OK (layers: " << reloaded->rootGroup()->children.size() << ")";

        // 3. Document export (PNG, JPEG, WebP)
        const QString exportPng = tmpDir.filePath(QStringLiteral("export.png"));
        const QString exportJpg = tmpDir.filePath(QStringLiteral("export.jpg"));
        const QString exportWebp = tmpDir.filePath(QStringLiteral("export.webp"));

        QString exportError;
        if (!cc::exportDocumentToImage(doc, exportPng, QStringLiteral("png"), 100, 1.0, &exportError)) {
            qCritical() << "[SmokeTest] FAIL: PNG export failed:" << exportError;
            return 7;
        }
        if (!cc::exportDocumentToImage(doc, exportJpg, QStringLiteral("jpeg"), 90, 1.0, &exportError)) {
            qCritical() << "[SmokeTest] FAIL: JPEG export failed:" << exportError;
            return 8;
        }

        const QByteArrayList supportedFormats = QImageWriter::supportedImageFormats();
        if (supportedFormats.contains("webp")) {
            if (!cc::exportDocumentToImage(doc, exportWebp, QStringLiteral("webp"), 90, 1.0, &exportError)) {
                qCritical() << "[SmokeTest] FAIL: WebP export failed:" << exportError;
                return 9;
            }
            qInfo() << "[SmokeTest] Exporters (PNG, JPEG, WebP) OK";
        } else {
            qInfo() << "[SmokeTest] Exporters (PNG, JPEG) OK [WebP plugin not present in current Qt runtime]";
        }

        // 4. MainWindow headless instantiation and project opening
        cc::MainWindow window(&settings, &i18n);
        window.openFromPath(projPath);
        qInfo() << "[SmokeTest] MainWindow initialized and project opened successfully.";

        qInfo() << "[SmokeTest] ALL CHECKS PASSED (100% HEALTHY).";
        cc::LogService::shutdown();
        return 0;
    }

    cc::MainWindow window(&settings, &i18n);

    const QStringList positionalArgs = parser.positionalArguments();
    if (!positionalArgs.isEmpty()) {
        const QString projectPath = positionalArgs.first();
        if (QFile::exists(projectPath)) {
            window.openFromPath(projectPath);
        }
    }

    window.show();

    const int exitCode = QApplication::exec();
    qInfo() << "Exiting with code" << exitCode;
    cc::LogService::shutdown();
    return exitCode;
}
