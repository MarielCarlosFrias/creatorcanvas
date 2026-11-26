#include <QtTest>
#include <QTemporaryDir>
#include <QImage>
#include <QFile>

#include "services/BatchProcessor.h"

using namespace cc;

namespace {

void appendU16(QByteArray& d, quint16 v)
{
    d.append(char(v >> 8));
    d.append(char(v & 0xff));
}

void appendU32(QByteArray& d, quint32 v)
{
    d.append(char(v >> 24));
    d.append(char((v >> 16) & 0xff));
    d.append(char((v >> 8) & 0xff));
    d.append(char(v & 0xff));
}

QByteArray minimalRgbPsd(int w = 2, int h = 1)
{
    QByteArray d("8BPS", 4);
    appendU16(d, 1);
    d.append(QByteArray(6, '\0'));
    appendU16(d, 3);
    appendU32(d, quint32(h));
    appendU32(d, quint32(w));
    appendU16(d, 8);
    appendU16(d, 3);
    appendU32(d, 0);
    appendU32(d, 0);
    appendU32(d, 0);
    appendU16(d, 0);
    for (int c = 0; c < 3; ++c)
        for (int i = 0; i < w * h; ++i)
            d.append(char(20 + c * 60 + i));
    return d;
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(bytes) == bytes.size();
}

} // namespace

class TestBatchProcessor final : public QObject
{
    Q_OBJECT

private slots:
    void resizeAndPng();
    void psdToPdf();
    void psdToSvg();
    void invalidInputFails();
    void overwriteRule();
};

void TestBatchProcessor::resizeAndPng()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString input = dir.filePath(QStringLiteral("in.png"));
    QImage source(40, 20, QImage::Format_ARGB32);
    source.fill(QColor(10, 120, 200));
    QVERIFY(source.save(input));

    BatchImageOptions options;
    options.operations = {QStringLiteral("resize"), QStringLiteral("convert")};
    options.resizeSize = QSize(20, 20);
    options.outputFormat = QByteArray("png");
    const QString output = dir.filePath(QStringLiteral("out"));
    QString error;
    QVERIFY2(processBatchImageFile(input, output, options, &error), qPrintable(error));

    const QImage actual(output + QStringLiteral(".png"));
    QVERIFY(!actual.isNull());
    // KeepAspectRatio 40x20 -> 20x20 resulta em 20x10.
    QCOMPARE(actual.size(), QSize(20, 10));
}

void TestBatchProcessor::psdToPdf()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString input = dir.filePath(QStringLiteral("in.psd"));
    QVERIFY(writeFile(input, minimalRgbPsd()));

    BatchImageOptions options;
    options.operations = {QStringLiteral("convert")};
    options.outputFormat = QByteArray("pdf");
    const QString output = dir.filePath(QStringLiteral("out"));
    QString error;
    QVERIFY2(processBatchImageFile(input, output, options, &error), qPrintable(error));

    QFile file(output + QStringLiteral(".pdf"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.read(5), QByteArray("%PDF-"));
}

void TestBatchProcessor::psdToSvg()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString input = dir.filePath(QStringLiteral("in.psd"));
    QVERIFY(writeFile(input, minimalRgbPsd()));

    BatchImageOptions options;
    options.operations = {QStringLiteral("convert")};
    options.outputFormat = QByteArray("svg");
    const QString output = dir.filePath(QStringLiteral("out"));
    QString error;
    QVERIFY2(processBatchImageFile(input, output, options, &error), qPrintable(error));

    QFile file(output + QStringLiteral(".svg"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString svg = QString::fromUtf8(file.readAll());
    QVERIFY(svg.contains(QStringLiteral("<svg")));
    QVERIFY(svg.contains(QStringLiteral("<image")));
}

void TestBatchProcessor::invalidInputFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString input = dir.filePath(QStringLiteral("bad.psd"));
    QVERIFY(writeFile(input, QByteArray("8BPS") + QByteArray(3, '\0')));
    BatchImageOptions options;
    options.outputFormat = QByteArray("pdf");
    QString error;
    QVERIFY(!processBatchImageFile(input, dir.filePath(QStringLiteral("out")), options, &error));
    QVERIFY(!error.isEmpty());
}

void TestBatchProcessor::overwriteRule()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString input = dir.filePath(QStringLiteral("in.png"));
    QImage source(4, 4, QImage::Format_ARGB32);
    source.fill(Qt::red);
    QVERIFY(source.save(input));
    const QString output = dir.filePath(QStringLiteral("out.png"));
    QVERIFY(writeFile(output, QByteArray("existing")));

    BatchImageOptions options;
    options.outputFormat = QByteArray("png");
    options.overwrite = false;
    QVERIFY(!processBatchImageFile(input, output, options));
    options.overwrite = true;
    QVERIFY(processBatchImageFile(input, output, options));
}

QTEST_GUILESS_MAIN(TestBatchProcessor)
#include "test_batch.moc"
