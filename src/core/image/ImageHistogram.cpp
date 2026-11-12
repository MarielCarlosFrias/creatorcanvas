#include "ImageHistogram.h"

#include <QColor>

namespace cc {

quint32 ImageHistogram::maxCount() const
{
    quint32 max = 0;
    for (int i = 0; i < 256; ++i) {
        max = std::max(max, red[static_cast<size_t>(i)]);
        max = std::max(max, green[static_cast<size_t>(i)]);
        max = std::max(max, blue[static_cast<size_t>(i)]);
        max = std::max(max, luminance[static_cast<size_t>(i)]);
    }
    return max;
}

ImageHistogram computeHistogram(const QImage& image)
{
    ImageHistogram h;
    if (image.isNull())
        return h;

    const QImage img = image.format() == QImage::Format_ARGB32
                           ? image
                           : image.convertToFormat(QImage::Format_ARGB32);

    const int w = img.width();
    const int hgt = img.height();
    for (int y = 0; y < hgt; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb px = line[x];
            if (qAlpha(px) == 0)
                continue;   // transparente não conta como nível 0
            const int r = qRed(px);
            const int g = qGreen(px);
            const int b = qBlue(px);
            ++h.red[static_cast<size_t>(r)];
            ++h.green[static_cast<size_t>(g)];
            ++h.blue[static_cast<size_t>(b)];
            // Luminância Rec.601 — mesma base que os pesos de sombras/realces.
            const int lum = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b + 0.5);
            ++h.luminance[static_cast<size_t>(qBound(0, lum, 255))];
        }
    }
    return h;
}

} // namespace cc
