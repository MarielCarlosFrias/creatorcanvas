#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/VertexArray.hpp>

#include "tools/ToolContext.hpp"

#include <algorithm>
#include <cmath>

namespace minipaint::detail
{
/// Half a brush diameter plus a safety margin for the anti-aliased edge.
inline float dirtyMargin(const float diameter) noexcept
{
    return diameter * 0.5F + 1.0F;
}

/// Marks the canvas region covered by a brush stamp centered at |center|.
inline void markStampDirty(ToolContext& context,
                           const sf::Vector2f center,
                           const float diameter)
{
    const float margin = dirtyMargin(diameter);
    const int minX = static_cast<int>(std::floor(center.x)) - static_cast<int>(std::ceil(margin));
    const int minY = static_cast<int>(std::floor(center.y)) - static_cast<int>(std::ceil(margin));
    const int maxX = static_cast<int>(std::ceil(center.x)) + static_cast<int>(std::ceil(margin));
    const int maxY = static_cast<int>(std::ceil(center.y)) + static_cast<int>(std::ceil(margin));
    context.markDirty(minX, minY, maxX, maxY);
}

/// Marks the canvas region covered by a brush segment |a|-|b|.
inline void markSegmentDirty(ToolContext& context,
                             const sf::Vector2f a,
                             const sf::Vector2f b,
                             const float diameter)
{
    const float margin = dirtyMargin(diameter);
    const int minX = static_cast<int>(std::floor(std::min(a.x, b.x))) - static_cast<int>(std::ceil(margin));
    const int minY = static_cast<int>(std::floor(std::min(a.y, b.y))) - static_cast<int>(std::ceil(margin));
    const int maxX = static_cast<int>(std::ceil(std::max(a.x, b.x))) + static_cast<int>(std::ceil(margin));
    const int maxY = static_cast<int>(std::ceil(std::max(a.y, b.y))) + static_cast<int>(std::ceil(margin));
    context.markDirty(minX, minY, maxX, maxY);
}
void drawBrush(sf::RenderTarget& target,
               sf::Vector2f center,
               float diameter,
               const sf::Color& color);

void drawBrushSegment(sf::RenderTarget& target,
                      sf::Vector2f start,
                      sf::Vector2f end,
                      float diameter,
                      const sf::Color& color);

void drawRectangle(sf::RenderTarget& target,
                   sf::Vector2f start,
                   sf::Vector2f end,
                   float outlineThickness,
                   const sf::Color& color,
                   bool filled);

void drawEllipse(sf::RenderTarget& target,
                 sf::Vector2f start,
                 sf::Vector2f end,
                 float outlineThickness,
                 const sf::Color& color,
                 bool filled);

// --- Image-based drawing (CPU pixel manipulation for layers) ---

void drawBrushImage(sf::Image& image,
                    sf::Vector2f center,
                    float diameter,
                    const sf::Color& color);

void drawBrushSegmentImage(sf::Image& image,
                           sf::Vector2f start,
                           sf::Vector2f end,
                           float diameter,
                           const sf::Color& color);

void drawRectangleImage(sf::Image& image,
                        sf::Vector2f start,
                        sf::Vector2f end,
                        float outlineThickness,
                        const sf::Color& color,
                        bool filled);

void drawEllipseImage(sf::Image& image,
                      sf::Vector2f start,
                      sf::Vector2f end,
                      float outlineThickness,
                      const sf::Color& color,
                      bool filled);
} // namespace minipaint::detail
