#include <QtTest>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include "core/color/IccProfiles.h"
#include "core/Document.h"
#include "imageio/DocumentExporter.h"

using namespace cc;

/// ICC profile embedding in exported files: profiles come from the system
/// (none ship in the repository), so assertions degrade gracefully when the
/// platform has no profile for a space.
class TestIcc : public QObject
{
    Q_OBJECT

private slots:
    void testProfileResolution();
    void testPngRoundTrip();
    void testJpegRoundTrip();
    void testExportEmbedsProfile();
    void testProfileFallback();
    void testSliceEmbedsProfile();
};

void TestIcc::testProfileResolution()
{
    // sRGB and AdobeRGB are expected on the usual Linux colour setups.
    const QByteArray srgb = iccProfileFor(ColorSpace::sRGB);
    QVERIFY2(!srgb.isEmpty(), "an sRGB profile must be resolvable on this machine");
    QVERIFY2(srgb.size() >= 128, "profile must carry a full header");
    QVERIFY2(!iccProfilePathFor(ColorSpace::sRGB).isEmpty(), "path reported");

    // Lab is not exported as an image space, but resolution must not crash
    // and must stay consistent between the two accessors.
    const QByteArray lab = iccProfileFor(ColorSpace::Lab);
    QCOMPARE(lab.isEmpty(), iccProfilePathFor(ColorSpace::Lab).isEmpty());
}

void TestIcc::testPngRoundTrip()
{
    const QByteArray profile = iccProfileFor(ColorSpace::sRGB);
    if (profile.isEmpty())
        QSKIP("no sRGB profile installed");

    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(Qt::red);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    QVERIFY(image.save(&buffer, "PNG"));

    QVERIFY(extractIccProfileFromPng(png).isEmpty()); // none to begin with
    QVERIFY2(embedIccProfileInPng(png, profile), "embed must succeed");
    QCOMPARE(extractIccProfileFromPng(png), profile); // byte-identical

    // iCCP must precede IDAT, not be appended just before IEND.
    const int iccpOffset = png.indexOf("iCCP");
    const int idatOffset = png.indexOf("IDAT");
    QVERIFY(iccpOffset > 0 && idatOffset > 0);
    QVERIFY(iccpOffset < idatOffset);

    // The payload is still a valid PNG that Qt can read back.
    QImage reloaded;
    QVERIFY(reloaded.loadFromData(png, "PNG"));
    QCOMPARE(reloaded.size(), QSize(8, 8));
    QCOMPARE(reloaded.pixelColor(0, 0), QColor(Qt::red));
}

void TestIcc::testJpegRoundTrip()
{
    const QByteArray profile = iccProfileFor(ColorSpace::sRGB);
    if (profile.isEmpty())
        QSKIP("no sRGB profile installed");

    QImage image(16, 16, QImage::Format_RGB32);
    image.fill(Qt::blue);
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly);
    QVERIFY(image.save(&buffer, "JPG", 90));

    QVERIFY2(embedIccProfileInJpeg(jpeg, profile), "embed must succeed");
    QCOMPARE(extractIccProfileFromJpeg(jpeg), profile);

    // A payload over the 64 KB APP2 limit must split into several markers:
    // patch a copy of the profile to 3x its size so the header stays valid
    // and the payload crosses the chunk threshold.
    const QByteArray base = iccProfileFor(ColorSpace::sRGB);
    const int baseSize = base.size();
    QByteArray doubled;
    doubled.resize(4);
    const std::uint32_t tripledSize = static_cast<std::uint32_t>(baseSize * 3);
    doubled[0] = static_cast<char>((tripledSize >> 24) & 0xFF);
    doubled[1] = static_cast<char>((tripledSize >> 16) & 0xFF);
    doubled[2] = static_cast<char>((tripledSize >> 8) & 0xFF);
    doubled[3] = static_cast<char>(tripledSize & 0xFF);
    doubled.append(base.constData() + 4, baseSize - 4);
    doubled.append(base.constData(), baseSize);
    doubled.append(base.constData(), baseSize);
    QCOMPARE(doubled.size(), baseSize * 3);

    QByteArray jpegChunked;
    {
        QBuffer out(&jpegChunked);
        out.open(QIODevice::WriteOnly);
        QVERIFY(image.save(&out, "JPG", 90));
    }
    QVERIFY(extractIccProfileFromJpeg(jpegChunked).isEmpty()); // fresh payload

    QVERIFY2(embedIccProfileInJpeg(jpegChunked, doubled), "multi-marker embed");
    QCOMPARE(extractIccProfileFromJpeg(jpegChunked), doubled);
    (void)jpegChunked;

    QImage reloaded;
    QVERIFY(reloaded.loadFromData(jpeg, "JPG"));
    QCOMPARE(reloaded.size(), QSize(16, 16));
    const QColor pixel = reloaded.pixelColor(0, 0); // JPEG is lossy
    QVERIFY(qAbs(pixel.red() - 0) <= 6 && qAbs(pixel.green() - 0) <= 6 &&
            qAbs(pixel.blue() - 255) <= 6);
}

void TestIcc::testExportEmbedsProfile()
{
    const QByteArray profile = iccProfileFor(ColorSpace::AdobeRGB);
    if (profile.isEmpty())
        QSKIP("no AdobeRGB profile installed");

    QTemporaryDir dir;
    Document doc(64, 64, 96);
    doc.setColorSpace(ColorSpace::AdobeRGB);

    const QString path = dir.filePath(QStringLiteral("out.png"));
    QString error;
    QVERIFY2(exportDocumentToImage(doc, path, QStringLiteral("png"), 100, 1.0, &error),
              qPrintable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray extracted = extractIccProfileFromPng(file.readAll());
    file.close();
    QCOMPARE(extracted, profile);
}

void TestIcc::testProfileFallback()
{
    QTemporaryDir dir;
    Document doc(64, 64, 96);
    doc.setColorSpace(ColorSpace::DisplayP3);

    // With no Display P3 profile installed, tagging is a no-op; with one,
    // it must be embedded. Either way the PNG stays valid and readable.
    const QString path = dir.filePath(QStringLiteral("out.png"));
    QString error;
    QVERIFY(exportDocumentToImage(doc, path, QStringLiteral("png"), 100, 1.0, &error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    file.close();

    const QByteArray profile = iccProfileFor(ColorSpace::DisplayP3);
    QCOMPARE(extractIccProfileFromPng(bytes), profile);
    QImage reloaded;
    QVERIFY(reloaded.loadFromData(bytes, "PNG"));
}

void TestIcc::testSliceEmbedsProfile()
{
    const QByteArray profile = iccProfileFor(ColorSpace::AdobeRGB);
    if (profile.isEmpty())
        QSKIP("no AdobeRGB profile installed");

    QTemporaryDir dir;
    Document doc(64, 64, 96);
    doc.setColorSpace(ColorSpace::AdobeRGB);
    cc::WebSlice slice;
    slice.name = QStringLiteral("thumb");
    slice.rect = QRectF(0, 0, 32, 32);
    doc.addWebSlice(slice);

    const QString path = dir.filePath(QStringLiteral("slice.png"));
    QString error;
    QVERIFY2(exportSliceToImage(doc, doc.webSlices().front().rect, path,
                                QStringLiteral("png"), 100, 1.0, &error),
              qPrintable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(extractIccProfileFromPng(file.readAll()), profile);
    file.close();
}

QTEST_MAIN(TestIcc)

#include "test_icc.moc"
