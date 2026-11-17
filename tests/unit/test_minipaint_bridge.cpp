#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <algorithm>
#include <memory>

#include "services/minipaintbridge.h"
#include "core/Document.h"
#include "core/layers/Layer.h"

using namespace cc;

/// Verifies the MiniPaintBridge contract without the real editor: a shell
/// stub named minipaint-standalone records the argv it receives, copies a
/// known PNG into --output and exits with a code taken from the environment.
class TestMiniPaintBridge : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testRoundTrip();
    void testCancelled();
    void cleanupTestCase();

private:
    QByteArray m_oldPath;
    QTemporaryDir m_dir;
    QString m_argsFile;
    QString m_sourcePng;

    bool makeStub();
    void setEnv(const QByteArray& name, const QByteArray& value);
    void unsetEnv(const QByteArray& name);
};

void TestMiniPaintBridge::setEnv(const QByteArray& name, const QByteArray& value)
{
    qputenv(name.constData(), value);
}

void TestMiniPaintBridge::unsetEnv(const QByteArray& name)
{
    qunsetenv(name.constData());
}

bool TestMiniPaintBridge::makeStub()
{
    const QString stubPath = QDir(m_dir.path()).filePath(QStringLiteral("minipaint-standalone"));
    QFile stub(stubPath);
    if (!stub.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    const QString script = QStringLiteral(
        "#!/bin/sh\n"
        "out=''\n"
        "prev=''\n"
        "for a in \"$@\"; do\n"
        "  if [ \"$prev\" = '--output' ]; then out=\"$a\"; fi\n"
        "  prev=\"$a\"\n"
        "done\n"
        "printf '%s\\n' \"$@\" > \"$MINIPAINT_STUB_ARGS\"\n"
        "if [ -n \"$out\" ] && [ -f \"$MINIPAINT_STUB_PNG\" ]; then\n"
        "  cp \"$MINIPAINT_STUB_PNG\" \"$out\"\n"
        "fi\n"
        "exit ${MINIPAINT_STUB_EXIT:-0}\n");
    stub.write(script.toUtf8());
    stub.close();
    return stub.setPermissions(QFile::ReadUser | QFile::WriteUser | QFile::ExeUser |
                               QFile::ReadOwner | QFile::ExeOwner |
                               QFile::ReadGroup | QFile::ExeGroup |
                               QFile::ReadOther | QFile::ExeOther);
}

void TestMiniPaintBridge::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_argsFile = QDir(m_dir.path()).filePath(QStringLiteral("args.txt"));
    m_sourcePng = QDir(m_dir.path()).filePath(QStringLiteral("edited.png"));

    // The image the stub "saves back": recognisably not the layer colour.
    QImage edited(4, 4, QImage::Format_ARGB32);
    edited.fill(QColor(0, 200, 0, 255));
    QVERIFY(edited.save(m_sourcePng, "PNG"));

    QVERIFY(makeStub());

    m_oldPath = qgetenv("PATH");
    setEnv("PATH", m_dir.path().toUtf8() + ":" + m_oldPath);
    setEnv("MINIPAINT_STUB_ARGS", m_argsFile.toUtf8());
    setEnv("MINIPAINT_STUB_PNG", m_sourcePng.toUtf8());
    setEnv("MINIPAINT_STUB_EXIT", QByteArrayLiteral("0"));
}

void TestMiniPaintBridge::cleanupTestCase()
{
    setEnv("PATH", m_oldPath);
    unsetEnv("MINIPAINT_STUB_ARGS");
    unsetEnv("MINIPAINT_STUB_PNG");
    unsetEnv("MINIPAINT_STUB_EXIT");
}

void TestMiniPaintBridge::testRoundTrip()
{
    Document doc(128, 96, 96);
    QImage source(8, 8, QImage::Format_ARGB32);
    source.fill(QColor(255, 0, 0, 255));
    const LayerId assetId = doc.assets().addImage(source);

    auto layer = std::make_unique<ImageLayer>();
    layer->name = QStringLiteral("teste");
    layer->assetId = assetId;
    layer->naturalWidth = 8;
    layer->naturalHeight = 8;
    const LayerId layerId = layer->id();
    doc.addLayer(std::move(layer));

    MiniPaintBridge bridge;
    QVERIFY2(bridge.isAvailable(), "the stub must be found on PATH");
    QCOMPARE(bridge.executablePath(), QDir(m_dir.path()).filePath(QStringLiteral("minipaint-standalone")));

    QFile::remove(m_argsFile);
    auto* imageLayer = static_cast<ImageLayer*>(doc.findLayer(layerId));
    const QImage result = bridge.editLayer(imageLayer, &doc,
                                           QStringLiteral("pencil"), 3.0F);

    QVERIFY2(!result.isNull(), "exit 0 with an output PNG must yield an image");
    QCOMPARE(result.size(), QSize(4, 4));
    QCOMPARE(result.pixelColor(1, 1), QColor(0, 200, 0, 255)); // the stub's file

    // The bridge must have launched MiniPaint with the documented contract.
    // The stub writes one argument per line.
    QFile args(m_argsFile);
    QVERIFY(args.open(QIODevice::ReadOnly | QIODevice::Text));
    const QStringList argv = QString::fromUtf8(args.readAll()).split(
        QChar::fromLatin1('\n'), Qt::SkipEmptyParts);
    args.close();
    QVERIFY2(argv.contains(QStringLiteral("--input")), "input handoff");
    QVERIFY2(argv.contains(QStringLiteral("--output")), "output handoff");
    QVERIFY2(argv.contains(QStringLiteral("--tool")) && argv.contains(QStringLiteral("pencil")),
             "initial tool forwarded");
    QVERIFY2(argv.contains(QStringLiteral("--brush-size")) && argv.contains(QStringLiteral("3")),
             "brush size forwarded");
    QVERIFY2(argv.contains(QStringLiteral("--primary-color")) &&
             argv.contains(QStringLiteral("000000FF")), "primary colour forwarded");
    // The title is a single argument containing the layer name.
    const bool hasTitle = std::any_of(argv.cbegin(), argv.cend(),
                                      [](const QString& arg) {
                                          return arg.startsWith(QStringLiteral("MiniPaint - Editing "));
                                      });
    QVERIFY2(argv.contains(QStringLiteral("--title")) && hasTitle,
             "window title forwarded");
}

void TestMiniPaintBridge::testCancelled()
{
    Document doc(128, 96, 96);
    QImage source(8, 8, QImage::Format_ARGB32);
    source.fill(QColor(255, 0, 0, 255));
    const LayerId assetId = doc.assets().addImage(source);

    auto layer = std::make_unique<ImageLayer>();
    layer->name = QStringLiteral("teste");
    layer->assetId = assetId;
    layer->naturalWidth = 8;
    layer->naturalHeight = 8;
    const LayerId layerId = layer->id();
    doc.addLayer(std::move(layer));

    setEnv("MINIPAINT_STUB_EXIT", QByteArrayLiteral("2")); // user cancelled
    MiniPaintBridge bridge;
    auto* imageLayer = static_cast<ImageLayer*>(doc.findLayer(layerId));
    const QImage result = bridge.editLayer(imageLayer, &doc,
                                           QStringLiteral("pencil"), 3.0F);
    setEnv("MINIPAINT_STUB_EXIT", QByteArrayLiteral("0"));

    QVERIFY2(result.isNull(), "exit code 2 means cancel: no image");
}

QTEST_MAIN(TestMiniPaintBridge)

#include "test_minipaint_bridge.moc"