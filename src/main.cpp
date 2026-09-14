#include "core/image/BackgroundRemover.h"
#include "localization/i18nservice.h"
#include "services/logservice.h"
#include "services/settingsservice.h"
#include "ui/darktheme.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

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
        qInfo() << "[SmokeTest] Starting self-check...";
        qInfo() << "[SmokeTest] Languages found:" << i18n.availableLanguages();
        const QString model = cc::BackgroundRemover::findModelPath();
        qInfo() << "[SmokeTest] AI Model path:" << (!model.isEmpty() ? model : "none (optional)");
        cc::MainWindow window(&settings, &i18n);
        qInfo() << "[SmokeTest] MainWindow initialized successfully.";
        qInfo() << "[SmokeTest] PASS";
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
