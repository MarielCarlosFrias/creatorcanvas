#pragma once

#include <QString>
#include <QColor>
#include <QImage>
#include <optional>
#include <vector>

namespace cc {

/// Espacos de cor suportados pelo CreatorCanvas.
enum class ColorSpace {
    sRGB,          ///< sRGB IEC61966-2.1 (padrao web)
    AdobeRGB,      ///< Adobe RGB (1998) - wide gamut
    DisplayP3,     ///< Display P3 - wide gamut (Apple)
    CMYK,          ///< CMYK (impressao)
    Grayscale,     ///< Escala de cinza
    Lab            ///< CIE L*a*b* (espaco de conexao)
};

/// Retorna o nome legivel de um espaco de cor.
QString colorSpaceName(ColorSpace space);

/// Retorna se o espaco de cor e wide gamut (maior que sRGB).
bool isWideGamut(ColorSpace space);

/// Retorna se o espaco de cor suporta canais alfa.
bool supportsAlpha(ColorSpace space);

/// Numero de canais do espaco de cor.
int colorSpaceChannelCount(ColorSpace space);

/// Nome dos canais para um espaco de cor.
QStringList colorSpaceChannelNames(ColorSpace space);

/// Converte uma QImage de um espaco de cor para outro.
/// Retorna imagem vazia se a conversao nao for suportada.
QImage convertColorSpace(const QImage& source, ColorSpace from, ColorSpace to);

/// Converte uma cor QColor de um espaco para outro.
QColor convertColor(const QColor& color, ColorSpace from, ColorSpace to);

/// Verifica se uma conversao entre dois espacos de cor e suportada.
bool isColorConversionSupported(ColorSpace from, ColorSpace to);

} // namespace cc
