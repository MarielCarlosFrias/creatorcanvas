#pragma once

#include <QColor>
#include <QUuid>
#include <QtGlobal>

namespace cc {

struct TextOutline
{
    bool enabled = false;
    QColor color{0, 0, 0};
    double width = 4.0;

    bool operator==(const TextOutline& other) const
    {
        return enabled == other.enabled && color == other.color
               && width == other.width;
    }
};

struct TextShadow
{
    bool enabled = false;
    QColor color{0, 0, 0, 170};
    double offsetX = 4.0;
    double offsetY = 4.0;
    double blur = 6.0;

    bool operator==(const TextShadow& other) const
    {
        return enabled == other.enabled && color == other.color
               && offsetX == other.offsetX && offsetY == other.offsetY
               && blur == other.blur;
    }
};

struct TextGradient
{
    bool enabled = false;
    int type = 0; // 0 = Linear, 1 = Radial
    QColor startColor{255, 107, 107};
    QColor endColor{78, 205, 196};
    double angleDeg = 0.0;

    bool operator==(const TextGradient& other) const
    {
        return enabled == other.enabled && type == other.type
               && startColor == other.startColor && endColor == other.endColor
               && angleDeg == other.angleDeg;
    }
};

struct TextTextureFill
{
    bool enabled = false;
    QUuid assetId; // ID do asset de imagem registrado no AssetStore
    double opacity = 1.0;

    bool operator==(const TextTextureFill& other) const
    {
        return enabled == other.enabled && assetId == other.assetId
               && qFuzzyCompare(opacity, other.opacity);
    }
};

struct TextEffects
{
    TextOutline outline;
    TextShadow shadow;
    TextGradient gradient;
    TextTextureFill texture;
    bool hollow = false;   // Interior transparente (apenas contorno)
    bool knockout = false; // Efeito chroma cut / perfuração transparente

    bool operator==(const TextEffects& other) const
    {
        return outline == other.outline && shadow == other.shadow
               && gradient == other.gradient && texture == other.texture
               && hollow == other.hollow && knockout == other.knockout;
    }
    bool operator!=(const TextEffects& other) const { return !(*this == other); }
};

} // namespace cc
