#include "tools/EyedropperTool.hpp"

#include "tools/ToolContext.hpp"

#include <SFML/Graphics/Image.hpp>

#include <algorithm>
#include <cmath>

namespace minipaint
{
void EyedropperTool::onMousePressed(const sf::Vector2f position, ToolContext& context)
{
    if (context.image == nullptr || context.colorPicked == nullptr)
    {
        return;
    }

    const sf::Vector2u size = context.image->getSize();
    if (size.x == 0U || size.y == 0U)
    {
        return;
    }

    const auto pixelX = static_cast<unsigned int>(
        std::clamp(static_cast<int>(std::floor(position.x)), 0,
                   static_cast<int>(size.x) - 1));
    const auto pixelY = static_cast<unsigned int>(
        std::clamp(static_cast<int>(std::floor(position.y)), 0,
                   static_cast<int>(size.y) - 1));

    context.colorPicked(context.image->getPixel({pixelX, pixelY}));
}

void EyedropperTool::onMouseMoved(const sf::Vector2f, ToolContext&)
{
}

bool EyedropperTool::onMouseReleased(const sf::Vector2f, ToolContext&)
{
    return false;
}

void EyedropperTool::renderPreview(sf::RenderTarget&, ToolContext&)
{
}

void EyedropperTool::cancel()
{
}
} // namespace minipaint
