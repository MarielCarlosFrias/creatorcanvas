#include "PsdImporter.h"

#include <cstring>
#include <vector>

namespace cc {
namespace {

/// Leitor posicional com verificação de limites — qualquer leitura fora do
/// buffer devolve false e invalida o ficheiro inteiro (nada parcial).
class PsdReader
{
public:
    explicit PsdReader(const QByteArray& d)
        : m_data(d)
    {
    }

    qint64 pos() const { return m_pos; }
    qint64 remaining() const { return m_data.size() - m_pos; }

    bool readU16(quint16& v)
    {
        if (remaining() < 2)
            return false;
        v = (quint8(m_data[m_pos]) << 8) | quint8(m_data[m_pos + 1]);
        m_pos += 2;
        return true;
    }

    bool readU32(quint32& v)
    {
        if (remaining() < 4)
            return false;
        v = (quint32(quint8(m_data[m_pos])) << 24)
            | (quint32(quint8(m_data[m_pos + 1])) << 16)
            | (quint32(quint8(m_data[m_pos + 2])) << 8)
            | quint32(quint8(m_data[m_pos + 3]));
        m_pos += 4;
        return true;
    }

    bool readBytes(char* dst, qint64 n)
    {
        if (n < 0 || remaining() < n)
            return false;
        std::memcpy(dst, m_data.constData() + m_pos, size_t(n));
        m_pos += n;
        return true;
    }

    bool skip(qint64 n)
    {
        if (n < 0 || remaining() < n)
            return false;
        m_pos += n;
        return true;
    }

private:
    const QByteArray& m_data;
    qint64 m_pos = 0;
};

/// Decodifica uma linha PackBits (RLE) de `expected` bytes (amostras de 8
/// bits ou pares big-endian de 16 bits). Devolve false se os dados acabarem
/// ou sobrarem bytes inválidos.
bool decodePackBitsRow(PsdReader& r, qint64 expected, std::vector<quint8>& out)
{
    out.clear();
    out.reserve(size_t(expected));
    qint64 produced = 0;
    while (produced < expected) {
        if (r.remaining() < 1)
            return false;
        char nCh = 0;
        if (!r.readBytes(&nCh, 1))
            return false;
        const qint8 n = qint8(nCh);
        if (n >= 0) { // copia os próximos n+1 bytes literais
            const qint64 count = qint64(n) + 1;
            if (r.remaining() < count || produced + count > expected)
                return false;
            for (qint64 i = 0; i < count; ++i) {
                char b = 0;
                if (!r.readBytes(&b, 1))
                    return false;
                out.push_back(quint8(b));
            }
            produced += count;
        } else if (n != -128) { // repete o próximo byte 1-n vezes
            const qint64 count = 1 - qint64(n);
            if (r.remaining() < 1 || produced + count > expected)
                return false;
            char b = 0;
            if (!r.readBytes(&b, 1))
                return false;
            for (qint64 i = 0; i < count; ++i)
                out.push_back(quint8(b));
            produced += count;
        } // n == -128: no-op
    }
    return produced == expected;
}

} // namespace

bool decodePsdComposite(const QByteArray& bytes, QImage* out, QString* error,
                        qint64 maxPixels)
{
    auto fail = [&](const QString& msg) {
        if (error)
            *error = msg;
        return false;
    };

    PsdReader r(bytes);

    // --- Header ---
    char sig[4] = {};
    if (!r.readBytes(sig, 4) || std::memcmp(sig, "8BPS", 4) != 0)
        return fail(QStringLiteral("Not a PSD file (bad signature)."));

    quint16 version = 0;
    if (!r.readU16(version))
        return fail(QStringLiteral("Truncated PSD header."));
    if (version != 1)
        return fail(QStringLiteral("PSB (version 2) files are not supported."));

    if (!r.skip(6)) // reserved
        return fail(QStringLiteral("Truncated PSD header."));

    quint16 channels = 0;
    if (!r.readU16(channels) || channels < 1 || channels > 4)
        return fail(QStringLiteral("Unsupported PSD channel count."));

    quint32 height = 0, width = 0;
    if (!r.readU32(height) || !r.readU32(width))
        return fail(QStringLiteral("Truncated PSD header."));
    if (width == 0 || height == 0)
        return fail(QStringLiteral("PSD has zero dimensions."));
    const qint64 pixels = qint64(width) * qint64(height);
    if (pixels > maxPixels)
        return fail(QStringLiteral("PSD exceeds the pixel limit."));

    quint16 depth = 0;
    if (!r.readU16(depth))
        return fail(QStringLiteral("Truncated PSD header."));
    if (depth != 8 && depth != 16)
        return fail(QStringLiteral("Only 8/16-bit PSD is supported."));
    const int bytesPerSample = depth / 8;

    quint16 mode = 0;
    if (!r.readU16(mode))
        return fail(QStringLiteral("Truncated PSD header."));
    if (mode != 1 && mode != 3) // 1=grayscale, 3=RGB (CMYK/others rejected)
        return fail(QStringLiteral("Only grayscale and RGB PSD is supported."));
    // O leitor mínimo aceita luminosidade[+alfa] ou RGB[+alfa]. Sem esta
    // validação, um PSD RGB com 1/2 canais indexava planos inexistentes.
    const bool validChannelLayout = mode == 1 ? channels <= 2 : channels >= 3;
    if (!validChannelLayout)
        return fail(QStringLiteral("PSD channel count does not match its colour mode."));

    // --- Color mode data / image resources / layer & mask: lengths + skip ---
    for (int section = 0; section < 3; ++section) {
        quint32 len = 0;
        if (!r.readU32(len))
            return fail(QStringLiteral("Truncated PSD section header."));
        if (!r.skip(qint64(len)))
            return fail(QStringLiteral("Truncated PSD section data."));
    }

    quint16 compression = 0;
    if (!r.readU16(compression))
        return fail(QStringLiteral("Truncated PSD image data header."));
    if (compression > 1)
        return fail(QStringLiteral("Unsupported PSD compression."));

    const int w = int(width);
    const int h = int(height);
    const int ch = int(channels);

    // Planos por canal: cada canal é um plano h×w (planar, não entrelaçado).
    std::vector<std::vector<quint8>> planes(size_t(ch), std::vector<quint8>(size_t(w) * size_t(h)));

    if (compression == 0) {
        // Raw: bytes crus por plano.
        for (int c = 0; c < ch; ++c) {
            const qint64 planeBytes = qint64(w) * h * bytesPerSample;
            if (r.remaining() < planeBytes)
                return fail(QStringLiteral("Truncated PSD raw image data."));
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    char buf[2] = {};
                    if (!r.readBytes(buf, bytesPerSample))
                        return fail(QStringLiteral("Truncated PSD raw image data."));
                    // 16-bit: fica só o byte alto (downsample simples e seguro).
                    planes[size_t(c)][size_t(y) * size_t(w) + size_t(x)] = quint8(buf[0]);
                }
            }
        }
    } else {
        // RLE: tabela de contagens (h*ch u16) seguida dos dados PackBits por
        // linha, plano a plano.
        std::vector<quint16> counts(size_t(h) * size_t(ch));
        for (auto& cnt : counts)
            if (!r.readU16(cnt))
                return fail(QStringLiteral("Truncated PSD RLE count table."));
        std::vector<quint8> row;
        for (int c = 0; c < ch; ++c) {
            for (int y = 0; y < h; ++y) {
                const qint64 rowBytes = counts[size_t(c) * size_t(h) + size_t(y)];
                if (r.remaining() < rowBytes)
                    return fail(QStringLiteral("Truncated PSD RLE data."));
                const qint64 before = r.pos();
                // PackBits descreve BYTES, não píxeis: em 16-bit cada linha tem
                // duas vezes mais bytes. A tabela de contagens continua a ser
                // por linha de canal, como no PSD de 8-bit.
                if (!decodePackBitsRow(r, qint64(w) * bytesPerSample, row))
                    return fail(QStringLiteral("Corrupt PSD RLE row."));
                if (r.pos() - before != rowBytes)
                    return fail(QStringLiteral("PSD RLE row size mismatch."));
                for (int x = 0; x < w; ++x) {
                    // Tal como no caminho raw, o byte alto representa a
                    // redução segura de 16 para 8 bits.
                    planes[size_t(c)][size_t(y) * size_t(w) + size_t(x)] =
                        row[size_t(x) * size_t(bytesPerSample)];
                }
            }
        }
    }

    // --- Monta a imagem final ---
    QImage img(w, h, QImage::Format_ARGB32);
    if (img.isNull())
        return fail(QStringLiteral("Could not allocate the PSD image."));

    const bool gray = (mode == 1);
    const int alphaChannel = (ch >= 2 && gray) || (ch >= 4 && !gray) ? ch - 1 : -1;

    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const size_t idx = size_t(y) * size_t(w) + size_t(x);
            quint8 r8, g8, b8, a8 = 255;
            if (gray) {
                const quint8 g = planes[0][idx];
                r8 = g8 = b8 = g;
                if (alphaChannel >= 0)
                    a8 = planes[size_t(alphaChannel)][idx];
            } else {
                r8 = planes[0][idx];
                g8 = planes[1][idx];
                b8 = planes[2][idx];
                if (alphaChannel >= 0)
                    a8 = planes[size_t(alphaChannel)][idx];
            }
            line[x] = qRgba(r8, g8, b8, a8);
        }
    }

    if (out)
        *out = std::move(img);
    return true;
}

} // namespace cc
