#include "localization/i18nservice.h"
#include "services/logservice.h"
#include "services/settingsservice.h"
#include "ui/darktheme.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QDir>
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

    cc::MainWindow window(&settings, &i18n);
    window.show();

    const int exitCode = QApplication::exec();
    qInfo() << "Exiting with code" << exitCode;
    cc::LogService::shutdown();
    return exitCode;
}
