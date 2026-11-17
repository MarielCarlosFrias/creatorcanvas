#pragma once

#include <SFML/Graphics/BlendMode.hpp>
#include <SFML/Graphics/Image.hpp>

#include <string>
#include <vector>

namespace minipaint
{
/// A single paint layer with RGBA pixels, opacity, visibility, and blend mode.
struct Layer
{
    std::string name;
    sf::Image image;
    float opacity{1.0F};          // 0.0 (transparent) to 1.0 (opaque)
    bool visible{true};
    bool locked{false};
    sf::BlendMode blendMode{sf::BlendAlpha};

    Layer() = default;

    Layer(std::string layerName, sf::Vector2u size, sf::Color fillColor = sf::Color::Transparent)
        : name(std::move(layerName))
        , image()
    {
        image.resize(size, fillColor);
    }

    [[nodiscard]] sf::Vector2u size() const noexcept
    {
        return image.getSize();
    }

    [[nodiscard]] bool isEmpty() const noexcept
    {
        return image.getSize().x == 0U || image.getSize().y == 0U;
    }
};

/// Composite a stack of layers into a single image.
/// Layers are composited bottom-to-top using each layer's opacity and blend mode.
[[nodiscard]] sf::Image compositeLayers(const std::vector<Layer>& layers);

/// Composites only the rectangle [origin, origin + regionSize) of the layer
/// stack into |result| (which must already have regionSize pixels), starting
/// from transparent. Blend math is pixel-local, so the produced pixels are
/// identical to compositeLayers() cropped to the region — this is what makes
/// incremental canvas sync possible without recompositing every layer per
/// mouse event.
void compositeLayersIntoRegion(sf::Image& result,
                               const std::vector<Layer>& layers,
                               sf::Vector2u regionSize,
                               sf::Vector2u origin);

} // namespace minipaint
