#include "tools/BucketFillTool.hpp"

#include "tools/ToolContext.hpp"

#include <SFML/Graphics/Image.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace minipaint
{
namespace
{
bool colorsMatch(const sf::Color& left, const sf::Color& right, const unsigned int tolerance)
{
    const auto within = [tolerance](const std::uint8_t first, const std::uint8_t second) {
        const int difference = static_cast<int>(first) - static_cast<int>(second);
        return std::abs(difference) <= static_cast<int>(tolerance);
    };

    return within(left.r, right.r) && within(left.g, right.g) && within(left.b, right.b) &&
           within(left.a, right.a);
}
} // namespace

void BucketFillTool::onMousePressed(const sf::Vector2f position, ToolContext& context)
{
    m_modified = false;

    sf::Image* targetImage = context.targetImage;
    const sf::Image* sourceImage = context.targetImage;

    if (targetImage == nullptr || sourceImage == nullptr)
    {
        return;
    }

    const sf::Vector2u size = sourceImage->getSize();
    if (size.x == 0U || size.y == 0U)
    {
        return;
    }

    const auto pixelX = static_cast<unsigned int>(std::clamp(
        static_cast<int>(std::floor(position.x)), 0, static_cast<int>(size.x) - 1));
    const auto pixelY = static_cast<unsigned int>(std::clamp(
        static_cast<int>(std::floor(position.y)), 0, static_cast<int>(size.y) - 1));
    const sf::Color targetColor = sourceImage->getPixel({pixelX, pixelY});
    const sf::Color replacementColor = context.settings.primaryColor;

    if (targetColor == replacementColor)
    {
        return;
    }

    sf::Image filled = *sourceImage;
    const unsigned int tolerance = std::min(255U, context.settings.fillTolerance);

    std::vector<std::uint8_t> visited(static_cast<std::size_t>(size.x) *
                                      static_cast<std::size_t>(size.y), 0U);
    std::vector<std::size_t> pending;
    pending.reserve(64U);

    const std::size_t start = static_cast<std::size_t>(pixelY) *
                              static_cast<std::size_t>(size.x) +
                              static_cast<std::size_t>(pixelX);
    pending.push_back(start);
    visited[start] = 1U;
    filled.setPixel({pixelX, pixelY}, replacementColor);

    const auto tryPixel = [&](const std::size_t x, const std::size_t y) {
        const std::size_t index = y * static_cast<std::size_t>(size.x) + x;
        if (visited[index] != 0U)
        {
            return;
        }

        if (!colorsMatch(filled.getPixel({static_cast<unsigned int>(x),
                                          static_cast<unsigned int>(y)}),
                         targetColor,
                         tolerance))
        {
            visited[index] = 1U;
            return;
        }

        visited[index] = 1U;
        filled.setPixel({static_cast<unsigned int>(x), static_cast<unsigned int>(y)},
                        replacementColor);
        pending.push_back(index);
    };

    for (std::size_t head = 0; head < pending.size(); ++head)
    {
        const std::size_t index = pending[head];
        const std::size_t x = index % static_cast<std::size_t>(size.x);
        const std::size_t y = index / static_cast<std::size_t>(size.x);

        if (x > 0U)
        {
            tryPixel(x - 1U, y);
        }
        if (x + 1U < static_cast<std::size_t>(size.x))
        {
            tryPixel(x + 1U, y);
        }
        if (y > 0U)
        {
            tryPixel(x, y - 1U);
        }
        if (y + 1U < static_cast<std::size_t>(size.y))
        {
            tryPixel(x, y + 1U);
        }
    }

    *targetImage = filled;

    context.imageReplaced(*targetImage);
    m_modified = true;
}

void BucketFillTool::onMouseMoved(const sf::Vector2f, ToolContext&)
{
}

bool BucketFillTool::onMouseReleased(const sf::Vector2f, ToolContext&)
{
    const bool modified = m_modified;
    m_modified = false;
    return modified;
}

void BucketFillTool::renderPreview(sf::RenderTarget&, ToolContext&)
{
}

void BucketFillTool::cancel()
{
    m_modified = false;
}
} // namespace minipaint
