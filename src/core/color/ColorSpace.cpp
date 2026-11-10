#include "ColorSpace.h"

#include <cmath>
#include <algorithm>

namespace cc {

QString colorSpaceName(ColorSpace space) {
    switch (space) {
    case ColorSpace::sRGB:      return QStringLiteral("sRGB");
    case ColorSpace::AdobeRGB:  return QStringLiteral("Adobe RGB (1998)");
    case ColorSpace::DisplayP3: return QStringLiteral("Display P3");
    case ColorSpace::CMYK:      return QStringLiteral("CMYK");
    case ColorSpace::Grayscale: return QStringLiteral("Grayscale");
    case ColorSpace::Lab:       return QStringLiteral("CIE Lab");
    }
    return QStringLiteral("Unknown");
}

bool isWideGamut(ColorSpace space) {
    switch (space) {
    case ColorSpace::AdobeRGB:
    case ColorSpace::DisplayP3:
        return true;
    default:
        return false;
    }
}

bool supportsAlpha(ColorSpace space) {
    switch (space) {
    case ColorSpace::CMYK:
    case ColorSpace::Grayscale:
        return false;
    default:
        return true;
    }
}

int colorSpaceChannelCount(ColorSpace space) {
    switch (space) {
    case ColorSpace::sRGB:
    case ColorSpace::AdobeRGB:
    case ColorSpace::DisplayP3:
    case ColorSpace::Lab:
        return 3;
    case ColorSpace::CMYK:
        return 4;
    case ColorSpace::Grayscale:
        return 1;
    }
    return 3;
}

QStringList colorSpaceChannelNames(ColorSpace space) {
    switch (space) {
    case ColorSpace::sRGB:
    case ColorSpace::AdobeRGB:
    case ColorSpace::DisplayP3:
        return QStringList({ "R", "G", "B" });
    case ColorSpace::CMYK:
        return QStringList({ "C", "M", "Y", "K" });
    case ColorSpace::Grayscale:
        return QStringList({ "Gray" });
    case ColorSpace::Lab:
        return QStringList({ "L", "a", "b" });
    }
    return QStringList({ "R", "G", "B" });
}

bool isColorConversionSupported(ColorSpace from, ColorSpace to) {
    if (from == ColorSpace::sRGB || to == ColorSpace::sRGB)
        return true;
    if (isWideGamut(from) && (to == ColorSpace::sRGB || to == ColorSpace::Lab))
        return true;
    if (from == ColorSpace::CMYK && (to == ColorSpace::sRGB || to == ColorSpace::Lab))
        return true;
    if (from == ColorSpace::Lab || to == ColorSpace::Lab)
        return true;
    if (from == ColorSpace::Grayscale && to == ColorSpace::sRGB)
        return true;
    if (from == to)
        return true;
    return false;
}

/// Converte sRGB linear para XYZ (D65)
QColor sRGBtoXYZ(const QColor& color) {
    auto sRGBtoLinear = [](double c) {
        if (c <= 0.04045)
            return c / 12.92;
        return std::pow((c + 0.055) / 1.055, 2.4);
    };
    double r = sRGBtoLinear(color.redF());
    double g = sRGBtoLinear(color.greenF());
    double b = sRGBtoLinear(color.blueF());
    double x = r * 0.4124 + g * 0.3576 + b * 0.1805;
    double y = r * 0.2126 + g * 0.7152 + b * 0.0722;
    double z = r * 0.0193 + g * 0.1192 + b * 0.9505;
    return QColor::fromRgbF(qBound(0.0, x, 1.0), qBound(0.0, y, 1.0), qBound(0.0, z, 1.0));
}

/// Converte XYZ (D65) para sRGB linear
QColor XYZtosRGB(const QColor& color) {
    auto linearTosRGB = [](double c) {
        if (c <= 0.0031308)
            return c * 12.92;
        return 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
    };
    double x = color.redF();
    double y = color.greenF();
    double z = color.blueF();
    double r = x * 3.2406 + y * -1.5372 + z * -0.4986;
    double g = x * -0.9689 + y * 1.8758 + z * 0.0415;
    double b = x * 0.0557 + y * -0.2040 + z * 1.0570;
    return QColor::fromRgbF(qBound(0.0, linearTosRGB(r), 1.0),
                            qBound(0.0, linearTosRGB(g), 1.0),
                            qBound(0.0, linearTosRGB(b), 1.0));
}

/// Converte sRGB para CMYK (Fogra39 aproximado)
QColor sRGBtoCMYK(const QColor& color) {
    double r = color.redF();
    double g = color.greenF();
    double b = color.blueF();
    double k = 1.0 - qMax(qMax(r, g), b);
    if (k >= 1.0) {
        return QColor::fromRgbF(0, 0, 0, 1.0);
    }
    double c = (1.0 - r - k) / (1.0 - k);
    double m = (1.0 - g - k) / (1.0 - k);
    double y = (1.0 - b - k) / (1.0 - k);
    return QColor::fromRgbF(qBound(0.0, c, 1.0),
                            qBound(0.0, m, 1.0),
                            qBound(0.0, y, 1.0),
                            qBound(0.0, k, 1.0));
}

/// Converte CMYK para sRGB (Fogra39 aproximado)
QColor CMYKtosRGB(const QColor& color) {
    double c = color.redF();
    double m = color.greenF();
    double y = color.blueF();
    double k = color.alphaF();
    double r = (1.0 - c) * (1.0 - k);
    double g = (1.0 - m) * (1.0 - k);
    double b = (1.0 - y) * (1.0 - k);
    return QColor::fromRgbF(qBound(0.0, r, 1.0),
                            qBound(0.0, g, 1.0),
                            qBound(0.0, b, 1.0));
}

/// Converte sRGB para Lab (D65)
QColor sRGBtoLab(const QColor& color) {
    QColor xyz = sRGBtoXYZ(color);
    double x = xyz.redF() / 0.95047;
    double y = xyz.greenF() / 1.00000;
    double z = xyz.blueF() / 1.08883;
    auto f = [](double t) {
        if (t > 0.008856)
            return std::cbrt(t);
        return (7.787 * t) + (16.0 / 116.0);
    };
    double L = (116.0 * f(y)) - 16.0;
    double a = 500.0 * (f(x) - f(y));
    double b = 200.0 * (f(y) - f(z));
    return QColor::fromRgbF(qBound(0.0, L / 100.0, 1.0),
                            qBound(0.0, (a + 128.0) / 255.0, 1.0),
                            qBound(0.0, (b + 128.0) / 255.0, 1.0));
}

/// Converte Lab para sRGB (D65)
QColor LabtosRGB(const QColor& color) {
    double L = color.redF() * 100.0;
    double a = (color.greenF() * 255.0) - 128.0;
    double b = (color.blueF() * 255.0) - 128.0;
    auto fInv = [](double t) {
        if (std::pow(t, 3) > 0.008856)
            return std::pow(t, 3);
        return (t - (16.0 / 116.0)) / 7.787;
    };
    double fy = (L + 16.0) / 116.0;
    double fx = fy + a / 500.0;
    double fz = fy - b / 200.0;
    double x = 0.95047 * fInv(fx);
    double y = 1.00000 * fInv(fy);
    double z = 1.08883 * fInv(fz);
    return XYZtosRGB(QColor::fromRgbF(qBound(0.0, x, 1.0),
                                      qBound(0.0, y, 1.0),
                                      qBound(0.0, z, 1.0)));
}

QColor convertColor(const QColor& color, ColorSpace from, ColorSpace to) {
    if (from == to)
        return color;
    
    QColor srgb = color;
    if (from != ColorSpace::sRGB) {
        switch (from) {
        case ColorSpace::CMYK:
            srgb = CMYKtosRGB(color);
            break;
        case ColorSpace::Lab:
            srgb = LabtosRGB(color);
            break;
        case ColorSpace::AdobeRGB:
        case ColorSpace::DisplayP3:
            srgb = color;
            break;
        case ColorSpace::Grayscale:
            srgb = QColor::fromRgbF(color.redF(), color.redF(), color.redF());
            break;
        default:
            return QColor();
        }
    }
    
    if (to == ColorSpace::sRGB)
        return srgb;
    
    switch (to) {
    case ColorSpace::CMYK:
        return sRGBtoCMYK(srgb);
    case ColorSpace::Lab:
        return sRGBtoLab(srgb);
    case ColorSpace::Grayscale:
        return QColor::fromRgbF(srgb.redF(), srgb.redF(), srgb.redF());
    case ColorSpace::AdobeRGB:
    case ColorSpace::DisplayP3:
        return srgb;
    default:
        return QColor();
    }
}

QImage convertColorSpace(const QImage& source, ColorSpace from, ColorSpace to) {
    if (from == to || !isColorConversionSupported(from, to))
        return source;
    
    QImage result = source.convertToFormat(QImage::Format_RGBA8888);
    
    for (int y = 0; y < result.height(); ++y) {
        for (int x = 0; x < result.width(); ++x) {
            QColor pixel = result.pixelColor(x, y);
            QColor converted = convertColor(pixel, from, to);
            if (converted.isValid()) {
                result.setPixelColor(x, y, converted);
            }
        }
    }
    
    return result;
}

} // namespace cc
