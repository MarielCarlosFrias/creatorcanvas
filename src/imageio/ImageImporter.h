#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

namespace cc {

enum class ImportStatus
{
    Ok,
    FileNotFound,
    CannotRead,
    TooLarge,
    UnsupportedFormat,
    CorruptImage
};

struct ImportResult
{
    ImportStatus status = ImportStatus::FileNotFound;
    QString errorMessage;
    QByteArray encoded;
    QString format;
    QImage image;
};

ImportResult importImageFromFile(const QString& filePath,
                                 qint64 maxFileBytes = 64LL * 1024 * 1024,
                                 qint64 maxPixels = 64LL * 1024 * 1024);

} // namespace cc
