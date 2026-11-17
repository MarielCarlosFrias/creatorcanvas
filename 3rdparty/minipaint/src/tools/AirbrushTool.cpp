#include "tools/AirbrushTool.hpp"

#include "tools/Drawing.hpp"
#include "tools/ToolContext.hpp"

#include <cmath>

namespace minipaint
{
void AirbrushTool::spray(const sf::Vector2f& position, ToolContext& context, const int count)
{
    for (int index = 0; index < count; ++index)
    {
        const float angle = unit() * 6.2831853F;
        const float dist = unit() * context.settings.brushSize * 0.5F;
        const sf::Vector2f point{position.x + std::cos(angle) * dist,
                                  position.y + std::sin(angle) * dist};

        if (context.targetImage != nullptr)
        {
            detail::drawBrushImage(*context.targetImage, point,
                                   context.settings.brushSize * 0.3F,
                                   context.settings.primaryColor);
        }
        else if (context.target != nullptr)
        {
            detail::drawBrush(*context.target, point, context.settings.brushSize * 0.3F,
                              context.settings.primaryColor);
        }
    }

    // Dots land within brushSize*0.5 of the center; a 1.4x stamp margin covers
    // the dot extent with room to spare.
    if (context.targetImage != nullptr)
    {
        detail::markStampDirty(context, position, context.settings.brushSize * 1.4F);
    }
}

void AirbrushTool::onMousePressed(const sf::Vector2f position, ToolContext& context)
{
    m_active = true;
    m_path.reset(position);
    spray(position, context, static_cast<int>(context.settings.brushSize) * 2);
}

void AirbrushTool::onMouseMoved(const sf::Vector2f position, ToolContext& context)
{
    if (!m_active)
    {
        return;
    }

    const int count = static_cast<int>(context.settings.brushSize);
    m_path.advance(position, [&](const sf::Vector2f& point) { spray(point, context, count); });
}

bool AirbrushTool::onMouseReleased(const sf::Vector2f position, ToolContext& context)
{
    if (!m_active)
    {
        return false;
    }

    const int count = static_cast<int>(context.settings.brushSize);
    m_path.finish(position, [&](const sf::Vector2f& point) { spray(point, context, count); });
    m_active = false;
    return true;
}

void AirbrushTool::renderPreview(sf::RenderTarget&, ToolContext&)
{
}

void AirbrushTool::cancel()
{
    m_active = false;
    m_path.reset({});
}
} // namespace minipaint
