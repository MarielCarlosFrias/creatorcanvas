#include "IccProfiles.h"

#include <QFile>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace cc {

namespace {

// ---------------------------------------------------------------- PNG bits --
// PNG chunk: [length:4 BE][type:4][data:length][crc:4]. The signature is the
// 8-byte document header; IEND is always the last chunk.
constexpr std::array<std::uint8_t, 8> kPngSignature{
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

std::uint32_t readBE32(const std::uint8_t* p)
{
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}

void appendBE32(QByteArray& out, std::uint32_t value)
{
    out.append(static_cast<char>((value >> 24) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>(value & 0xFF));
}

std::uint32_t crc32Update(std::uint32_t crc, const std::uint8_t* data,
                          std::size_t length)
{
    static std::array<std::uint32_t, 256> table{};
    static bool built = false;
    if (!built)
    {
        for (std::uint32_t n = 0; n < 256; ++n)
        {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k)
                c = (c & 1U) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        built = true;
    }

    crc ^= 0xFFFFFFFFU;
    for (std::size_t i = 0; i < length; ++i)
        crc = table[(crc ^ data[i]) & 0xFFU] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFU;
}

QByteArray pngChunk(const char type[4], const QByteArray& data)
{
    QByteArray chunk;
    appendBE32(chunk, static_cast<std::uint32_t>(data.size()));
    chunk.append(type, 4);
    chunk.append(data);
    appendBE32(chunk, crc32Update(0,
                                  reinterpret_cast<const std::uint8_t*>(chunk.constData() + 4),
                                  static_cast<std::size_t>(chunk.size() - 4)));
    return chunk;
}

bool looksLikePng(const QByteArray& bytes)
{
    return bytes.size() > static_cast<int>(sizeof(kPngSignature)) &&
           std::equal(kPngSignature.begin(), kPngSignature.end(),
                      reinterpret_cast<const std::uint8_t*>(bytes.constData()));
}

bool looksLikeJpeg(const QByteArray& bytes)
{
    return bytes.size() > 4 &&
           static_cast<std::uint8_t>(bytes[0]) == 0xFF &&
           static_cast<std::uint8_t>(bytes[1]) == 0xD8;
}

bool looksLikeWebp(const QByteArray& bytes)
{
    return bytes.size() >= 12 && bytes.left(4) == QByteArray("RIFF") &&
           bytes.mid(8, 4) == QByteArray("WEBP");
}

std::uint32_t readLE32(const std::uint8_t* p)
{
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

void appendLE32(QByteArray& out, std::uint32_t value)
{
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 24) & 0xFF));
}

QByteArray webpChunk(const char type[4], const QByteArray& data)
{
    QByteArray chunk(type, 4);
    appendLE32(chunk, static_cast<std::uint32_t>(data.size()));
    chunk.append(data);
    if ((data.size() & 1) != 0)
        chunk.append('\0'); // RIFF chunks are padded to an even byte count
    return chunk;
}

bool looksLikeIccProfile(const QByteArray& profile)
{
    if (profile.size() < 128)
        return false;
    const auto* raw = reinterpret_cast<const std::uint8_t*>(profile.constData());
    if (readBE32(raw) != static_cast<std::uint32_t>(profile.size()))
        return false; // the header size field must match the payload
    return raw[36] == 'a' && raw[37] == 'c' && raw[38] == 's' && raw[39] == 'p';
}

// ----------------------------------------------------------- profile lookup --
struct Candidate
{
    ColorSpace space;
    QLatin1StringView fileName;
};

const std::array<Candidate, 22>& profileCandidates()
{
    static const std::array<Candidate, 22> candidates{{
        {ColorSpace::sRGB, QLatin1StringView("srgb.icc")},
        {ColorSpace::sRGB, QLatin1StringView("sRGB.icc")},
        {ColorSpace::sRGB, QLatin1StringView("esrgb.icc")},
        {ColorSpace::sRGB, QLatin1StringView("sRGB Color Space Profile.icm")},
        {ColorSpace::sRGB, QLatin1StringView("sRGB IEC61966-2.1.icm")},
        {ColorSpace::AdobeRGB, QLatin1StringView("a98.icc")},
        {ColorSpace::AdobeRGB, QLatin1StringView("AdobeRGB1998.icc")},
        {ColorSpace::AdobeRGB, QLatin1StringView("compatibleWithAdobeRGB1998.icc")},
        {ColorSpace::AdobeRGB, QLatin1StringView("compat-a98.icc")},
        {ColorSpace::AdobeRGB, QLatin1StringView("AdobeRGB1998.icm")},
        {ColorSpace::DisplayP3, QLatin1StringView("P3.icc")},
        {ColorSpace::DisplayP3, QLatin1StringView("displayp3.icc")},
        {ColorSpace::DisplayP3, QLatin1StringView("DisplayP3.icc")},
        {ColorSpace::DisplayP3, QLatin1StringView("Display P3.icm")},
        {ColorSpace::CMYK, QLatin1StringView("default_cmyk.icc")},
        {ColorSpace::CMYK, QLatin1StringView("USWebCoatedSWOP.icc")},
        {ColorSpace::CMYK, QLatin1StringView("USWebCoatedSWOP.icm")},
        {ColorSpace::Grayscale, QLatin1StringView("default_gray.icc")},
        {ColorSpace::Grayscale, QLatin1StringView("ps_gray.icc")},
        {ColorSpace::Grayscale, QLatin1StringView("Gray.icm")},
        {ColorSpace::Lab, QLatin1StringView("lab.icc")},
        {ColorSpace::Lab, QLatin1StringView("Lab.icm")},
    }};
    return candidates;
}

QStringList profileDirectories()
{
    QStringList dirs;
    const QString overrideDir = QProcessEnvironment::systemEnvironment().value(QStringLiteral("CC_ICC_DIR"));
    if (!overrideDir.isEmpty())
        dirs << overrideDir;

    dirs << QStringLiteral("/usr/share/color/icc")
         << QStringLiteral("/usr/local/share/color/icc")
         << QStringLiteral("/usr/share/color/icc/colord")
         << QStringLiteral("/usr/share/color/icc/ghostscript")
#if defined(Q_OS_MACOS)
         << QStringLiteral("/Library/ColorSync/Profiles")
         << QStringLiteral("/System/Library/ColorSync/Profiles")
#elif defined(Q_OS_WIN)
         << QStringLiteral("C:/Windows/System32/spool/drivers/color")
#endif
        ;
    for (const QString& searchPath : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        dirs << searchPath + QStringLiteral("/color/icc");
    return dirs;
}

struct ResolvedProfile
{
    QByteArray bytes;
    QString path;
};

ResolvedProfile resolveProfile(ColorSpace space)
{
    for (const QString& dir : profileDirectories())
    {
        for (const Candidate& candidate : profileCandidates())
        {
            if (candidate.space != space)
                continue;
            const QString path = dir + QStringLiteral("/") + candidate.fileName;
            if (!QFile::exists(path))
                continue;
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                continue;
            const QByteArray bytes = file.readAll();
            if (looksLikeIccProfile(bytes))
                return {bytes, path};
        }
    }
    return {};
}

} // namespace

QByteArray iccProfileFor(ColorSpace space)
{
    return resolveProfile(space).bytes;
}

QString iccProfilePathFor(ColorSpace space)
{
    return resolveProfile(space).path;
}

std::optional<ColorSpace> colorSpaceForProfile(const QByteArray& profile)
{
    if (profile.isEmpty())
        return std::nullopt;
    constexpr ColorSpace spaces[] = {
        ColorSpace::sRGB, ColorSpace::AdobeRGB, ColorSpace::DisplayP3,
        ColorSpace::CMYK, ColorSpace::Grayscale, ColorSpace::Lab};
    for (const ColorSpace space : spaces) {
        if (iccProfileFor(space) == profile)
            return space;
    }
    return std::nullopt;
}

QByteArray extractIccProfileFromPng(const QByteArray& png)
{
    if (!looksLikePng(png))
        return {};

    const auto* raw = reinterpret_cast<const std::uint8_t*>(png.constData());
    std::size_t offset = sizeof(kPngSignature);
    while (offset + 8 <= static_cast<std::size_t>(png.size()))
    {
        const std::uint32_t length = readBE32(raw + offset);
        const char* type = png.constData() + offset + 4;
        const std::size_t dataStart = offset + 8;
        if (dataStart + static_cast<std::size_t>(length) + 4 >
            static_cast<std::size_t>(png.size()))
            return {}; // truncated

        if (std::strncmp(type, "iCCP", 4) == 0)
        {
            const char* data = png.constData() + dataStart;
            // keyword (NUL-terminated), compression method, zlib stream.
            const auto keywordEnd = static_cast<std::size_t>(
                std::find(data, data + length, '\0') - data);
            if (keywordEnd + 2 > static_cast<std::size_t>(length))
                return {};
            const QByteArray compressed(data + keywordEnd + 2,
                                        static_cast<int>(length - keywordEnd - 2));
            // qUncompress expects the 4-byte uncompressed-size header that
            // qCompress writes; iCCP carries only the raw zlib stream, so a
            // placeholder header is prepended (Qt grows past the hint).
            QByteArray withHeader;
            withHeader.resize(4);
            withHeader.append(compressed);
            return qUncompress(withHeader);
        }
        offset = dataStart + length + 4;
    }
    return {};
}

bool embedIccProfileInPng(QByteArray& png, const QByteArray& profile)
{
    if (!looksLikePng(png) || !looksLikeIccProfile(profile))
        return false;

    // Drop any existing iCCP first, so the payload stays canonical.
    {
        QByteArray rebuilt;
        rebuilt.reserve(png.size());
        const auto* raw = reinterpret_cast<const std::uint8_t*>(png.constData());
        rebuilt.append(png.constData(), static_cast<int>(sizeof(kPngSignature)));
        std::size_t offset = sizeof(kPngSignature);
        bool inserted = false;
        while (offset + 8 <= static_cast<std::size_t>(png.size()))
        {
            const std::uint32_t length = readBE32(raw + offset);
            const std::size_t dataStart = offset + 8;
            if (dataStart + static_cast<std::size_t>(length) + 4 >
                static_cast<std::size_t>(png.size()))
                return false; // truncated
            const char* type = png.constData() + offset + 4;
            const bool isIcc = std::strncmp(type, "iCCP", 4) == 0;
            const bool insertBefore = std::strncmp(type, "IDAT", 4) == 0 ||
                                      std::strncmp(type, "IEND", 4) == 0;
            if (!isIcc && !inserted && insertBefore)
            {
                // iCCP must precede IDAT: many readers stop inspecting
                // ancillary chunks once image data starts. IEND is only a
                // fallback for degenerate payloads.
                QByteArray payload;
                payload.append("ICC Profile");
                payload.append('\0');
                payload.append('\0'); // zlib compression method
                payload.append(qCompress(profile).mid(4)); // strip zlib size header
                rebuilt.append(pngChunk("iCCP", payload));
                inserted = true;
            }
            if (!isIcc)
                rebuilt.append(png.constData() + offset,
                               static_cast<int>(dataStart + length + 4 - offset));
            offset = dataStart + length + 4;
        }
        if (!inserted)
            return false;
        png = rebuilt;
    }
    return true;
}

QByteArray extractIccProfileFromJpeg(const QByteArray& jpeg)
{
    if (!looksLikeJpeg(jpeg))
        return {};

    QByteArray profile;
    const auto* raw = reinterpret_cast<const std::uint8_t*>(jpeg.constData());
    std::size_t offset = 2; // after SOI
    int expected = -1;
    int seen = 0;
    while (offset + 4 <= static_cast<std::size_t>(jpeg.size()))
    {
        if (raw[offset] != 0xFF)
            return {};
        const std::uint8_t marker = raw[offset + 1];
        if (marker == 0xD9) // EOI
            break;
        if (marker == 0xDA) // SOS: image data starts, headers are over
            break;
        const std::uint32_t length = (static_cast<std::uint32_t>(raw[offset + 2]) << 8) |
                                     static_cast<std::uint32_t>(raw[offset + 3]);
        const std::size_t dataStart = offset + 4;
        if (length < 2 || dataStart + length - 2 > static_cast<std::size_t>(jpeg.size()))
            return {};
        const char* data = jpeg.constData() + dataStart;
        const std::size_t dataLength = length - 2;
        if (marker == 0xE2 && dataLength > 14 &&
            std::strncmp(data, "ICC_PROFILE\0", 12) == 0)
        {
            const auto total = static_cast<int>(static_cast<std::uint8_t>(data[13]));
            if (seen == 0)
            {
                expected = total;
                profile.reserve(static_cast<int>(static_cast<std::size_t>(total) * dataLength));
            }
            profile.append(data + 14, static_cast<int>(dataLength - 14));
            ++seen;
            if (expected > 0 && seen >= expected)
                break;
        }
        offset = dataStart + dataLength;
    }
    return (expected > 0 && seen == expected) ? profile : QByteArray();
}

bool embedIccProfileInJpeg(QByteArray& jpeg, const QByteArray& profile)
{
    if (!looksLikeJpeg(jpeg) || !looksLikeIccProfile(profile))
        return false;

    // Drop existing ICC APP2 markers; insert ours right after SOI (readers
    // skip unknown APPn markers and the ICC profile is order-independent
    // among them). Everything else is copied verbatim.
    QByteArray markers;
    // A JPEG marker's length field is 16 bits and includes the field itself:
    // the payload per APP2 marker may not exceed 65535 - 2 (length) - 14
    // ("ICC_PROFILE\0" + sequence + total) = 65519, or the field wraps and
    // corrupts the file.
    constexpr int chunkMax = 65535 - 2 - 14;
    const int parts = (static_cast<int>(profile.size()) + chunkMax - 1) / chunkMax;
    for (int part = 0; part < parts; ++part)
    {
        const int chunkOffset = part * chunkMax;
        const int chunkSize = std::min<int>(chunkMax, static_cast<int>(profile.size()) - chunkOffset);
        const int length = 2 + 12 + 2 + chunkSize;
        QByteArray segment;
        segment.append('\xFF').append('\xE2'); // APP2
        segment.append(static_cast<char>((length >> 8) & 0xFF));
        segment.append(static_cast<char>(length & 0xFF));
        segment.append("ICC_PROFILE");
        segment.append('\0');
        segment.append(static_cast<char>(part + 1));
        segment.append(static_cast<char>(parts));
        segment.append(profile.constData() + chunkOffset, chunkSize);
        markers.append(segment);
    }

    QByteArray body;
    body.append(jpeg.constData(), 2); // SOI
    body.append(markers);

    const auto* raw = reinterpret_cast<const std::uint8_t*>(jpeg.constData());
    std::size_t offset = 2;
    bool sawImageData = false;
    while (offset + 4 <= static_cast<std::size_t>(jpeg.size()))
    {
        if (raw[offset] != 0xFF)
            return false;
        const std::uint8_t marker = raw[offset + 1];
        if (marker == 0xDA || marker == 0xD9) {
            sawImageData = true;
            break;
        }
        const std::uint32_t length = (static_cast<std::uint32_t>(raw[offset + 2]) << 8) |
                                     static_cast<std::uint32_t>(raw[offset + 3]);
        const std::size_t segmentEnd = offset + 2 + length;
        if (length < 2 || segmentEnd > static_cast<std::size_t>(jpeg.size()))
            return false;
        const char* data = jpeg.constData() + offset + 4;
        const bool isIcc = marker == 0xE2 && length >= 16 &&
                           std::strncmp(data, "ICC_PROFILE", 12) == 0;
        if (!isIcc)
            body.append(jpeg.constData() + offset, static_cast<int>(segmentEnd - offset));
        offset = segmentEnd;
    }
    if (!sawImageData)
        return false;
    body.append(jpeg.constData() + offset, static_cast<int>(jpeg.size() - offset));

    jpeg = body;
    return true;
}

QByteArray extractIccProfileFromWebp(const QByteArray& webp)
{
    if (!looksLikeWebp(webp))
        return {};
    const auto* raw = reinterpret_cast<const std::uint8_t*>(webp.constData());
    std::size_t offset = 12;
    while (offset + 8 <= static_cast<std::size_t>(webp.size()))
    {
        const char* type = webp.constData() + offset;
        const std::uint32_t length = readLE32(raw + offset + 4);
        const std::size_t dataStart = offset + 8;
        if (dataStart + length > static_cast<std::size_t>(webp.size()))
            return {};
        if (std::strncmp(type, "ICCP", 4) == 0)
            return webp.mid(static_cast<int>(dataStart), static_cast<int>(length));
        offset = dataStart + length + (length & 1U);
    }
    return {};
}

bool embedIccProfileInWebp(QByteArray& webp, const QByteArray& profile)
{
    if (!looksLikeWebp(webp) || !looksLikeIccProfile(profile))
        return false;

    const auto* raw = reinterpret_cast<const std::uint8_t*>(webp.constData());
    if (readLE32(raw + 4) + 8 != static_cast<std::uint32_t>(webp.size()))
        return false; // truncated or inconsistent RIFF payload

    QByteArray rebuilt = webp.left(12); // RIFF + size + WEBP
    std::size_t offset = 12;
    bool inserted = false;
    while (offset + 8 <= static_cast<std::size_t>(webp.size()))
    {
        const char* type = webp.constData() + offset;
        const std::uint32_t length = readLE32(raw + offset + 4);
        const std::size_t dataStart = offset + 8;
        if (dataStart + length > static_cast<std::size_t>(webp.size()))
            return false;
        const std::size_t chunkEnd = dataStart + length + (length & 1U);
        const bool isIcc = std::strncmp(type, "ICCP", 4) == 0;
        const bool insertBefore = std::strncmp(type, "VP8 ", 4) == 0 ||
                                  std::strncmp(type, "VP8L", 4) == 0 ||
                                  std::strncmp(type, "ANIM", 4) == 0;
        if (!inserted && insertBefore)
        {
            rebuilt.append(webpChunk("ICCP", profile));
            inserted = true;
        }
        if (!isIcc)
        {
            const std::size_t begin = rebuilt.size();
            rebuilt.append(webp.constData() + offset,
                           static_cast<int>(chunkEnd - offset));
            if (std::strncmp(type, "VP8X", 4) == 0 && length >= 1)
                rebuilt[static_cast<int>(begin + 8)] |= 0x20; // ICC present flag
        }
        offset = chunkEnd;
    }
    if (!inserted)
        rebuilt.append(webpChunk("ICCP", profile));

    const std::uint32_t riffSize = static_cast<std::uint32_t>(rebuilt.size() - 8);
    for (int i = 0; i < 4; ++i)
        rebuilt[4 + i] = static_cast<char>((riffSize >> (8 * i)) & 0xFF);
    webp = rebuilt;
    return true;
}

bool tagImageFileWithProfile(const QString& path, const QString& format,
                             const QByteArray& profile, ColorSpace space)
{
    const QByteArray resolved = profile.isEmpty() ? iccProfileFor(space) : profile;
    if (resolved.isEmpty())
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QByteArray bytes = file.readAll();
    file.close();

    bool embedded = false;
    if (format == QLatin1String("png") && looksLikePng(bytes))
        embedded = embedIccProfileInPng(bytes, resolved);
    else if (format == QLatin1String("jpeg") && looksLikeJpeg(bytes))
        embedded = embedIccProfileInJpeg(bytes, resolved);
    else if (format == QLatin1String("webp") && looksLikeWebp(bytes))
        embedded = embedIccProfileInWebp(bytes, resolved);
    else
        return false;

    if (!embedded)
        return false;

    // Commit atomically so a short write or a crash cannot destroy the
    // successfully exported image while tagging its metadata.
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly))
        return false;
    if (output.write(bytes) != bytes.size())
        return false;
    return output.commit();
}

bool tagImageFileWithColorSpace(const QString& path, const QString& format,
                                ColorSpace space)
{
    return tagImageFileWithProfile(path, format, {}, space);
}

} // namespace cc
