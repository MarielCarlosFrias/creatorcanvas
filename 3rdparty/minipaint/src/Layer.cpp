#include "Layer.hpp"

#include <SFML/Graphics/Image.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace minipaint
{
namespace
{
enum class OpKind : std::uint8_t
{
    None,     // overwrite (BlendNone)
    Alpha,    // Porter-Duff over (BlendAlpha)
    Add,      // BlendAdd
    Multiply, // BlendMultiply
    Min,      // BlendMin
    Max       // BlendMax
};

// Resolved once per layer so the inner loop never calls sf::BlendMode::operator==.
OpKind resolveOp(const sf::BlendMode& mode)
{
    if (mode == sf::BlendAlpha) return OpKind::Alpha;
    if (mode == sf::BlendAdd) return OpKind::Add;
    if (mode == sf::BlendMultiply) return OpKind::Multiply;
    if (mode == sf::BlendMin) return OpKind::Min;
    if (mode == sf::BlendMax) return OpKind::Max;
    if (mode == sf::BlendNone) return OpKind::None;
    return OpKind::Alpha;
}

inline std::uint8_t clamp255(const float value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0.0F, 255.0F));
}

// Blends one source pixel onto one destination pixel (raw RGBA bytes).
template <OpKind Kind>
void blendOne(const std::uint8_t* src, std::uint8_t* dst, const float opacity)
{
    if constexpr (Kind == OpKind::None)
    {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(src[3]) * opacity, 0.0F, 255.0F));
        return;
    }

    // Fast path: an opaque source pixel overwrites the destination exactly
    // (outA == 1), so no blending math is needed at all.
    if (opacity >= 1.0F && src[3] == 255)
    {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = 255;
        return;
    }

    const float inv255 = 1.0F / 255.0F;
    const float sa = static_cast<float>(src[3]) * inv255 * opacity;
    const float da = static_cast<float>(dst[3]) * inv255;
    const float outA = sa + da * (1.0F - sa);

    if (outA <= 0.0F)
    {
        dst[0] = 0;
        dst[1] = 0;
        dst[2] = 0;
        dst[3] = 0;
        return;
    }

    const float invOutA = 1.0F / outA;
    const float oneMinusSa = 1.0F - sa;

    const auto blend = [&](const std::uint8_t srcC, const std::uint8_t dstC) -> std::uint8_t {
        float blended = static_cast<float>(srcC);
        if constexpr (Kind == OpKind::Add)
        {
            blended = std::min(255.0F, static_cast<float>(srcC) + static_cast<float>(dstC));
        }
        else if constexpr (Kind == OpKind::Multiply)
        {
            blended = static_cast<float>(srcC) * static_cast<float>(dstC) * inv255;
        }
        else if constexpr (Kind == OpKind::Min)
        {
            blended = std::min(static_cast<float>(srcC), static_cast<float>(dstC));
        }
        else if constexpr (Kind == OpKind::Max)
        {
            blended = std::max(static_cast<float>(srcC), static_cast<float>(dstC));
        }

        return clamp255((static_cast<float>(dstC) * da * oneMinusSa + blended * sa) * invOutA);
    };

    dst[0] = blend(src[0], dst[0]);
    dst[1] = blend(src[1], dst[1]);
    dst[2] = blend(src[2], dst[2]);
    dst[3] = clamp255(outA * 255.0F);
}

template <OpKind Kind>
void blendLayerInto(const sf::Image& result,
                    const std::uint8_t* src,
                    const sf::Vector2u layerSize,
                    const sf::Vector2u dstSize,
                    const float opacity)
{
    std::uint8_t* dst = const_cast<std::uint8_t*>(result.getPixelsPtr());
    const std::size_t srcStride = static_cast<std::size_t>(layerSize.x) * 4U;
    const std::size_t dstStride = static_cast<std::size_t>(dstSize.x) * 4U;
    const std::size_t copyWidth = std::min<std::size_t>(layerSize.x, dstSize.x);
    const std::size_t copyHeight = std::min<std::size_t>(layerSize.y, dstSize.y);

    for (std::size_t y = 0; y < copyHeight; ++y)
    {
        const std::uint8_t* srcRow = src + y * srcStride;
        std::uint8_t* dstRow = dst + y * dstStride;

        for (std::size_t x = 0; x < copyWidth; ++x)
        {
            blendOne<Kind>(srcRow + x * 4U, dstRow + x * 4U, opacity);
        }
    }
}
} // namespace

namespace
{
// Region-limited twin of blendLayerInto: |result| holds exactly the region
// pixels and |origin| is the region's top-left in canvas coordinates, so a
// layer pixel only contributes when it falls inside the region.
template <OpKind Kind>
void blendLayerRegion(const std::uint8_t* src,
                      const sf::Vector2u layerSize,
                      std::uint8_t* dst,
                      const sf::Vector2u regionSize,
                      const sf::Vector2u origin,
                      const float opacity)
{
    const std::size_t srcStride = static_cast<std::size_t>(layerSize.x) * 4U;
    const std::size_t dstStride = static_cast<std::size_t>(regionSize.x) * 4U;

    for (unsigned int y = origin.y; y < origin.y + regionSize.y && y < layerSize.y; ++y)
    {
        const std::uint8_t* srcRow = src + static_cast<std::size_t>(y) * srcStride;
        std::uint8_t* dstRow = dst + static_cast<std::size_t>(y - origin.y) * dstStride;

        for (unsigned int x = origin.x; x < origin.x + regionSize.x && x < layerSize.x; ++x)
        {
            blendOne<Kind>(srcRow + static_cast<std::size_t>(x) * 4U,
                           dstRow + static_cast<std::size_t>(x - origin.x) * 4U,
                           opacity);
        }
    }
}
} // namespace

void compositeLayersIntoRegion(sf::Image& result,
                               const std::vector<Layer>& layers,
                               const sf::Vector2u regionSize,
                               const sf::Vector2u origin)
{
    if (regionSize.x == 0U || regionSize.y == 0U)
    {
        return;
    }

    result.resize(regionSize, sf::Color::Transparent);

    for (const auto& layer : layers)
    {
        if (!layer.visible || layer.isEmpty() || layer.opacity <= 0.0F)
        {
            continue;
        }

        const sf::Vector2u layerSize = layer.size();
        if (origin.x >= layerSize.x || origin.y >= layerSize.y)
        {
            continue;
        }

        const std::uint8_t* src = layer.image.getPixelsPtr();
        if (src == nullptr)
        {
            continue;
        }

        const float opacity = std::clamp(layer.opacity, 0.0F, 1.0F);
        std::uint8_t* dst = const_cast<std::uint8_t*>(result.getPixelsPtr());

        switch (resolveOp(layer.blendMode))
        {
            case OpKind::None:
                blendLayerRegion<OpKind::None>(src, layerSize, dst, regionSize, origin, opacity);
                break;
            case OpKind::Alpha:
                blendLayerRegion<OpKind::Alpha>(src, layerSize, dst, regionSize, origin, opacity);
                break;
            case OpKind::Add:
                blendLayerRegion<OpKind::Add>(src, layerSize, dst, regionSize, origin, opacity);
                break;
            case OpKind::Multiply:
                blendLayerRegion<OpKind::Multiply>(src, layerSize, dst, regionSize, origin, opacity);
                break;
            case OpKind::Min:
                blendLayerRegion<OpKind::Min>(src, layerSize, dst, regionSize, origin, opacity);
                break;
            case OpKind::Max:
                blendLayerRegion<OpKind::Max>(src, layerSize, dst, regionSize, origin, opacity);
                break;
        }
    }
}

sf::Image compositeLayers(const std::vector<Layer>& layers)
{
    sf::Image result;

    sf::Vector2u maxSize{0U, 0U};
    for (const auto& layer : layers)
    {
        if (layer.visible && !layer.isEmpty())
        {
            maxSize.x = std::max(maxSize.x, layer.size().x);
            maxSize.y = std::max(maxSize.y, layer.size().y);
        }
    }

    if (maxSize.x == 0U || maxSize.y == 0U)
    {
        return result;
    }

    result.resize(maxSize, sf::Color::Transparent);

    for (const auto& layer : layers)
    {
        if (!layer.visible || layer.isEmpty() || layer.opacity <= 0.0F)
        {
            continue;
        }

        const sf::Vector2u layerSize = layer.size();
        const std::uint8_t* src = layer.image.getPixelsPtr();
        if (src == nullptr)
        {
            continue;
        }

        const float opacity = std::clamp(layer.opacity, 0.0F, 1.0F);

        switch (resolveOp(layer.blendMode))
        {
            case OpKind::None:
                blendLayerInto<OpKind::None>(result, src, layerSize, maxSize, opacity);
                break;
            case OpKind::Alpha:
                blendLayerInto<OpKind::Alpha>(result, src, layerSize, maxSize, opacity);
                break;
            case OpKind::Add:
                blendLayerInto<OpKind::Add>(result, src, layerSize, maxSize, opacity);
                break;
            case OpKind::Multiply:
                blendLayerInto<OpKind::Multiply>(result, src, layerSize, maxSize, opacity);
                break;
            case OpKind::Min:
                blendLayerInto<OpKind::Min>(result, src, layerSize, maxSize, opacity);
                break;
            case OpKind::Max:
                blendLayerInto<OpKind::Max>(result, src, layerSize, maxSize, opacity);
                break;
        }
    }

    return result;
}
} // namespace minipaint
