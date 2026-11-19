#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

#include <optional>

#include "core/color/ColorSpace.h"

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
    /// Embedded source profile and its recognised working space, if any.
    QByteArray iccProfile;
    std::optional<ColorSpace> colorSpace;
};

ImportResult importImageFromFile(const QString& filePath,
                                 qint64 maxFileBytes = 64LL * 1024 * 1024,
                                 qint64 maxPixels = 64LL * 1024 * 1024);

/// Converts an imported image into an existing document's working profile,
/// then re-encodes the asset as PNG so later decoding sees converted pixels.
/// Returns false without changing |result| if either profile is unsupported.
[[nodiscard]] bool convertImportToColorSpace(ImportResult& result,
                                              const QByteArray& targetProfile);

} // namespace cc
