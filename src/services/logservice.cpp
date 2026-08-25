#include "logservice.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QTextStream>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace cc {
namespace {

std::atomic<bool> g_installed{false};
QMutex g_mutex;
std::unique_ptr<QFile> g_logFile;
QString g_directory;
qint64 g_maxBytes = 0;
int g_backupCount = 0;

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return QStringLiteral("DEBUG");
    case QtInfoMsg:     return QStringLiteral("INFO ");
    case QtWarningMsg:  return QStringLiteral("WARN ");
    case QtCriticalMsg: return QStringLiteral("ERROR");
    case QtFatalMsg:    return QStringLiteral("FATAL");
    }
    return QStringLiteral("?????");
}

void rotateIfNeededLocked()
{
    const QString basePath = g_directory + QStringLiteral("/creatorcanvas.log");
    QFile existing(basePath);
    if (!existing.exists() || existing.size() <= g_maxBytes)
        return;

    for (int i = g_backupCount - 1; i >= 1; --i) {
        const QString from = QStringLiteral("%1.%2").arg(basePath).arg(i);
        const QString to   = QStringLiteral("%1.%2").arg(basePath).arg(i + 1);
        QFile target(to);
        if (target.exists())
            target.remove();
        QFile source(from);
        if (source.exists())
            source.rename(to);
    }

    QFile oldest(QStringLiteral("%1.%2").arg(basePath).arg(g_backupCount));
    if (oldest.exists())
        oldest.remove();

    QFile::rename(basePath, QStringLiteral("%1.1").arg(basePath));
}

void emitLine(const QString &line)
{
    if (g_logFile && g_logFile->isOpen()) {
        g_logFile->write(line.toUtf8());
        g_logFile->write("\n");
        g_logFile->flush();
    }
    std::fprintf(stderr, "%s\n", qUtf8Printable(line));
    std::fflush(stderr);
}

} // namespace

void LogService::init(const QString &directory, qint64 maxFileBytes, int backupCount)
{
    QMutexLocker locker(&g_mutex);
    if (g_installed.exchange(true))
        return;

    g_directory = directory;
    g_maxBytes = qMax<qint64>(1, maxFileBytes);
    g_backupCount = qMax(1, backupCount);

    QDir().mkpath(g_directory);
    rotateIfNeededLocked();

    g_logFile = std::make_unique<QFile>(
        g_directory + QStringLiteral("/creatorcanvas.log"));
    if (!g_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        g_logFile.reset(); // stderr-only fallback

    qInstallMessageHandler(&LogService::messageHandler);
}

void LogService::shutdown()
{
    QMutexLocker locker(&g_mutex);
    if (!g_installed.exchange(false))
        return;

    qInstallMessageHandler(nullptr);
    if (g_logFile) {
        g_logFile->flush();
        g_logFile->close();
        g_logFile.reset();
    }
}

void LogService::messageHandler(QtMsgType type,
                                const QMessageLogContext &context,
                                const QString &message)
{
    QMutexLocker locker(&g_mutex);

    const QString stamp = QDateTime::currentDateTime()
                              .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));

    QString category = context.category ? QString::fromLatin1(context.category)
                                        : QString();
    if (category.isEmpty() || category == QLatin1String("default"))
        category = QStringLiteral("app");

    const QString fileBase = context.file
        ? QFileInfo(QString::fromLatin1(context.file)).fileName()
        : QString();

    emitLine(QStringLiteral("%1 [%2] [%3] %4:%5 - %6")
                 .arg(stamp,
                      levelName(type),
                      category,
                      fileBase,
                      QString::number(context.line),
                      message));

    if (type == QtFatalMsg) {
        if (g_logFile && g_logFile->isOpen())
            g_logFile->flush();
        std::abort(); // preserve qFatal contract
    }
}

} // namespace cc
