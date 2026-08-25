#include "services/logservice.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using cc::LogService;

namespace {
QString readAll(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll());
}
} // namespace

class TestLogging final : public QObject
{
    Q_OBJECT

private slots:
    void writesTimestampedEntriesToFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString logPath = dir.filePath("logs/creatorcanvas.log");

        LogService::init(dir.filePath("logs"));
        qInfo().noquote() << "marker-alpha-12345";
        qInfo().noquote() << "second-line";
        LogService::shutdown();

        const QString content = readAll(logPath);
        QVERIFY(content.contains("marker-alpha-12345"));
        QVERIFY(content.contains("[INFO ]"));
        QVERIFY(content.contains("[app]"));
    }

    void rotatesOversizedLogOnNextInit()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString logs = dir.filePath("logs");

        // Session 1: exceed the tiny limit.
        LogService::init(logs, /*maxFileBytes=*/256, /*backups=*/2);
        for (int i = 0; i < 8; ++i)
            qWarning().noquote()
                << QString(120, QLatin1Char('x')) + QString::number(i);
        LogService::shutdown();

        QVERIFY(readAll(logs + "/creatorcanvas.log").size() > 256);

        // Session 2: init must rotate the oversized log to .1.
        LogService::init(logs, 256, 2);
        LogService::shutdown();

        QVERIFY(QFile::exists(logs + "/creatorcanvas.log.1"));
    }
};

QTEST_GUILESS_MAIN(TestLogging)
#include "test_logging.moc"
