#include "minipaintbridge.h"

#include "core/Document.h"
#include "core/assets/AssetStore.h"
#include "core/layers/Layer.h"
#include "imageio/DocumentExporter.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QDebug>

namespace cc {

namespace
{
/// Tool names accepted by MiniPaint CLI.
const QStringList kValidTools = {
    QStringLiteral("pencil"),
    QStringLiteral("eraser"),
    QStringLiteral("line"),
    QStringLiteral("rectangle"),
    QStringLiteral("ellipse"),
    QStringLiteral("bucket"),
    QStringLiteral("eyedropper")
};
} // namespace

MiniPaintBridge::MiniPaintBridge(QObject* parent)
    : QObject(parent)
{
    m_executablePath = resolveExecutable();
    m_available = !m_executablePath.isEmpty();
    if (m_available)
    {
        qInfo() << "[MiniPaintBridge] Found executable at:" << m_executablePath;
    }
    else
    {
        qInfo() << "[MiniPaintBridge] MiniPaint standalone executable not found. "
                   "Install it beside CreatorCanvas or add it to PATH.";
    }
    emit availabilityChanged(m_available);
}

QString MiniPaintBridge::resolveExecutable() const
{
    const QString appDir = QCoreApplication::applicationDirPath();

    // Candidate names per platform
#if defined(Q_OS_WIN)
    const QStringList names = {
        QStringLiteral("minipaint-standalone.exe"),
        QStringLiteral("minipaint-standalone.cmd"),
        QStringLiteral("minipaint-standalone")
    };
#else
    const QStringList names = {
        QStringLiteral("minipaint-standalone")
    };
#endif

    // 1. Beside the application executable (bundled with CreatorCanvas)
    for (const QString& name : names)
    {
        const QString path = appDir + QDir::separator() + name;
        if (QFile::exists(path))
        {
            return path;
        }
    }

    // 2. In a sibling minipaint/ directory (development layout)
    for (const QString& name : names)
    {
        const QString path = appDir + QStringLiteral("/../minipaint/build/") + name;
        if (QFile::exists(path))
        {
            return path;
        }
    }

    // 3. System PATH
    for (const QString& name : names)
    {
        const QString found = QStandardPaths::findExecutable(name);
        if (!found.isEmpty())
        {
            return found;
        }
#if defined(Q_OS_WIN)
        // QStandardPaths may only consider PATHEXT executables. Tests and
        // developer setups can expose a command script, so inspect PATH
        // explicitly as a fallback for .cmd/.bat launchers.
        const QStringList pathEntries =
            qEnvironmentVariable("PATH").split(u';', Qt::SkipEmptyParts);
        for (const QString& entry : pathEntries)
        {
            const QString candidate = QDir(entry).filePath(name);
            if (QFileInfo::isFile(candidate))
                return candidate;
        }
#endif
    }

    return QString();
}

bool MiniPaintBridge::isAvailable() const
{
    return m_available;
}

QString MiniPaintBridge::executablePath() const
{
    return m_executablePath;
}

QImage MiniPaintBridge::editLayer(ImageLayer* layer,
                                   Document* document,
                                   QString initialTool,
                                   float brushSize)
{
    if (!m_available || !layer || !document)
    {
        return QImage();
    }

    if (m_sessionRunning)
    {
        qWarning() << "[MiniPaintBridge] An editing session is already running";
        return QImage();
    }
    m_sessionRunning = true;
    // RAII: no matter which path leaves this function, the flag is cleared.
    struct SessionGuard
    {
        bool& flag;
        ~SessionGuard() { flag = false; }
    } sessionGuard{m_sessionRunning};

    if (layer->type() != LayerType::Image)
    {
        qWarning() << "[MiniPaintBridge] Layer is not an image layer";
        return QImage();
    }

    if (layer->locked)
    {
        qWarning() << "[MiniPaintBridge] Layer is locked";
        return QImage();
    }

    AssetStore& assets = document->assets();
    QImage source = assets.decodedImage(layer->assetId);
    if (source.isNull())
    {
        qWarning() << "[MiniPaintBridge] Could not decode layer asset";
        return QImage();
    }

    // Normalize tool name
    initialTool = initialTool.toLower();
    if (!kValidTools.contains(initialTool))
    {
        initialTool = QStringLiteral("pencil");
    }

    // Clamp brush size
    if (brushSize < 1.0F)
        brushSize = 1.0F;
    if (brushSize > 128.0F)
        brushSize = 128.0F;

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        qWarning() << "[MiniPaintBridge] Could not create temporary directory";
        return QImage();
    }

    const QString inputPath = tempDir.filePath(QStringLiteral("input.png"));
    const QString outputPath = tempDir.filePath(QStringLiteral("output.png"));

    // Export the layer bitmap as PNG (preserves alpha)
    QElapsedTimer timer;
    timer.start();
    if (!source.save(inputPath, "PNG"))
    {
        qWarning() << "[MiniPaintBridge] Could not write input PNG to" << inputPath;
        return QImage();
    }

    // Build command line arguments
    QStringList args;
    args << QStringLiteral("--input") << inputPath;
    args << QStringLiteral("--output") << outputPath;
    args << QStringLiteral("--tool") << initialTool;
    args << QStringLiteral("--brush-size") << QString::number(brushSize);
    args << QStringLiteral("--primary-color") << QStringLiteral("000000FF");
    args << QStringLiteral("--secondary-color") << QStringLiteral("FFFFFFFF");
    args << QStringLiteral("--title")
         << QStringLiteral("MiniPaint - Editing ") + layer->name;

    qInfo() << "[MiniPaintBridge] Layer exported in" << timer.elapsed() << "ms"
            << "(" << source.width() << "x" << source.height() << ")";
    qInfo() << "[MiniPaintBridge] Launching:" << m_executablePath << args;

    timer.start();

    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    QString program = m_executablePath;
    QStringList processArgs = args;
#if defined(Q_OS_WIN)
    // The Windows bridge test uses a .cmd stub. Batch files must be launched
    // through cmd.exe; real .exe installations keep the direct fast path.
    if (m_executablePath.endsWith(QStringLiteral(".cmd"), Qt::CaseInsensitive) ||
        m_executablePath.endsWith(QStringLiteral(".bat"), Qt::CaseInsensitive))
    {
        program = QStringLiteral("cmd.exe");
        processArgs = {QStringLiteral("/C"), m_executablePath};
        processArgs += args;
    }
#endif
    process.start(program, processArgs);

    if (!process.waitForStarted(5000))
    {
        qWarning() << "[MiniPaintBridge] Could not start MiniPaint process";
        return QImage();
    }

    // Wait for the editor WITHOUT freezing the interface: a local event loop
    // keeps CreatorCanvas responsive (repaints, move, other menus) while the
    // external editor owns the screen. A keep-alive timer pumps events.
    QEventLoop waitLoop;
    QTimer keepAlive;
    keepAlive.setInterval(50);
    connect(&keepAlive, &QTimer::timeout,
            []() { if (qApp) qApp->processEvents(); });
    connect(&process, &QProcess::finished, &waitLoop, &QEventLoop::quit);
    connect(&process, &QProcess::errorOccurred, &waitLoop, &QEventLoop::quit);
    keepAlive.start();
    waitLoop.exec();
    keepAlive.stop();

    if (process.state() != QProcess::NotRunning)
    {
        qWarning() << "[MiniPaintBridge] MiniPaint process timed out";
        process.kill();
        process.waitForFinished();
        return QImage();
    }

    const int exitCode = process.exitCode();
    qInfo() << "[MiniPaintBridge] MiniPaint exited with code" << exitCode
            << "after" << timer.elapsed() << "ms";

    // Exit code 2 = user cancelled
    if (exitCode == 2)
    {
        qInfo() << "[MiniPaintBridge] User cancelled";
        return QImage();
    }

    if (exitCode != 0)
    {
        qWarning() << "[MiniPaintBridge] MiniPaint failed with exit code" << exitCode;
        QByteArray err = process.readAllStandardError();
        if (!err.isEmpty())
            qWarning() << "[MiniPaintBridge] stderr:" << err;
        return QImage();
    }

    // Import the result PNG
    QFileInfo outInfo(outputPath);
    if (!outInfo.exists() || outInfo.size() == 0)
    {
        qWarning() << "[MiniPaintBridge] Output file missing or empty";
        return QImage();
    }

    QImage result(outputPath);
    if (result.isNull())
    {
        qWarning() << "[MiniPaintBridge] Could not read output PNG";
        return QImage();
    }

    qInfo() << "[MiniPaintBridge] Imported result:" << result.size();
    return result;
}

} // namespace cc
