#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

#include "ColorSpace.h"

namespace cc {

/// Resolves the raw ICC profile bytes for |space| from the system's installed
/// profiles (empty when the platform has no matching profile). The lookup is
/// pure: no profiles ship inside this repository.
[[nodiscard]] QByteArray iccProfileFor(ColorSpace space);

/// Path that provided iccProfileFor() (empty when unresolved) — for logs.
[[nodiscard]] QString iccProfilePathFor(ColorSpace space);

/// Identifies a profile by comparing it with the installed profiles.
/// Unknown/custom profiles return nullopt but remain preservable byte-for-byte.
[[nodiscard]] std::optional<ColorSpace> colorSpaceForProfile(const QByteArray& profile);

/// Extracts the embedded iCCP profile from PNG bytes (empty when absent).
/// Exposed so tests and the importer side can read back what we wrote.
[[nodiscard]] QByteArray extractIccProfileFromPng(const QByteArray& png);

/// Extracts the embedded ICC_PROFILE payload from JPEG bytes (empty when
/// absent), reassembled from the APP2 marker sequence.
[[nodiscard]] QByteArray extractIccProfileFromJpeg(const QByteArray& jpeg);

/// Embeds |profile| into a PNG payload as an iCCP chunk placed before IEND
/// (replacing any existing iCCP). Returns false on malformed payloads.
[[nodiscard]] bool embedIccProfileInPng(QByteArray& png, const QByteArray& profile);

/// Embeds |profile| into a JPEG payload as APP2 "ICC_PROFILE" markers right
/// after SOI (replacing any existing ICC markers).
[[nodiscard]] bool embedIccProfileInJpeg(QByteArray& jpeg, const QByteArray& profile);

/// WebP RIFF ICCP metadata (empty when absent).
[[nodiscard]] QByteArray extractIccProfileFromWebp(const QByteArray& webp);
[[nodiscard]] bool embedIccProfileInWebp(QByteArray& webp, const QByteArray& profile);

/// Embeds explicit source |profile| bytes when available, otherwise resolves
/// the installed profile matching |space|. Used by the exporters to preserve
/// an imported document's original profile instead of silently replacing it.
[[nodiscard]] bool tagImageFileWithProfile(const QString& path,
                                           const QString& format,
                                           const QByteArray& profile,
                                           ColorSpace space);

/// File-level helper: reads |path|, embeds the profile matching |space| when
/// the payload is a PNG or JPEG and a profile is available, and writes the
/// result back. Returns false (and leaves the file untouched) when nothing
/// was embedded — absence of a profile is not an error.
[[nodiscard]] bool tagImageFileWithColorSpace(const QString& path,
                                              const QString& format,
                                              ColorSpace space);

} // namespace cc