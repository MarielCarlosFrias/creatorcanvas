#include "ImageImporter.h"

#include <QFile>

namespace cc {
namespace {

bool startsWith(const QByteArray& bytes, const char* magic, int len)
{
    return bytes.left(len) == QByteArray(magic, len);
}

QString sniffFormat(const QByteArray& bytes)
{
    if (startsWith(bytes, "\x89PNG\r\n\x1a\n", 8))
        return QStringLiteral("png");
    if (startsWith(bytes, "\xFF\xD8\xFF", 3))
        return QStringLiteral("jpeg");
    if (bytes.size() > 12 && bytes.left(4) == QByteArray("RIFF")
        && bytes.mid(8, 4) == QByteArray("WEBP"))
        return QStringLiteral("webp");
    if (startsWith(bytes, "BM", 2))
        return QStringLiteral("bmp");
    return {};
}

} // namespace

ImportResult importImageFromFile(const QString& filePath,
                                 qint64 maxFileBytes, qint64 maxPixels)
{
    ImportResult result;

    QFile file(filePath);
    if (!file.exists()) {
        result.status = ImportStatus::FileNotFound;
        result.errorMessage = QStringLiteral("File does not exist: %1").arg(filePath);
        return result;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        result.status = ImportStatus::CannotRead;
        result.errorMessage =
            QStringLiteral("File cannot be opened for reading: %1").arg(filePath);
        return result;
    }
    if (file.size() > maxFileBytes) {
        result.status = ImportStatus::TooLarge;
        result.errorMessage =
            QStringLiteral("File exceeds the size limit (%1 bytes).").arg(file.size());
        return result;
    }

    result.encoded = file.readAll();
    file.close();

    result.format = sniffFormat(result.encoded);
    if (result.format.isEmpty()) {
        result.status = ImportStatus::UnsupportedFormat;
        result.errorMessage =
            QStringLiteral("Unrecognized image format (magic bytes).");
        return result;
    }

    if (!result.image.loadFromData(result.encoded,
                                   result.format.toLatin1().constData())) {
        result.status = ImportStatus::CorruptImage;
        result.errorMessage =
            QStringLiteral("Image data is corrupted or truncated.");
        return result;
    }

    const qint64 pixels = qint64(result.image.width()) * result.image.height();
    if (pixels > maxPixels) {
        result.status = ImportStatus::TooLarge;
        result.errorMessage =
            QStringLiteral("Image exceeds the pixel limit (%1 MP).")
                .arg(pixels / (1024.0 * 1024.0), 0, 'f', 1);
        return result;
    }

    result.status = ImportStatus::Ok;
    return result;
}

} // namespace cc
