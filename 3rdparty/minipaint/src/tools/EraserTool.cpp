#include "tools/EraserTool.hpp"

#include "tools/Drawing.hpp"
#include "tools/ToolContext.hpp"

namespace minipaint
{
void EraserTool::onMousePressed(const sf::Vector2f position, ToolContext& context)
{
    m_drawing = true;
    m_path.reset(position);

    const sf::Color eraseColor = context.settings.backgroundColor;

    if (context.targetImage != nullptr)
    {
        detail::drawBrushImage(*context.targetImage, position, context.settings.brushSize,
                               eraseColor);
        detail::markStampDirty(context, position, context.settings.brushSize);
        return;
    }

    if (context.target == nullptr)
    {
        return;
    }

    detail::drawBrush(*context.target, position, context.settings.brushSize, eraseColor);
}

void EraserTool::onMouseMoved(const sf::Vector2f position, ToolContext& context)
{
    if (!m_drawing)
    {
        return;
    }

    const sf::Color eraseColor = context.settings.backgroundColor;
    const auto stamp = [&](const sf::Vector2f& point) {
        if (context.targetImage != nullptr)
        {
            detail::drawBrushImage(*context.targetImage, point, context.settings.brushSize,
                                   eraseColor);
            detail::markStampDirty(context, point, context.settings.brushSize);
        }
        else if (context.target != nullptr)
        {
            detail::drawBrush(*context.target, point, context.settings.brushSize, eraseColor);
        }
    };

    m_path.advance(position, stamp);
}

bool EraserTool::onMouseReleased(const sf::Vector2f position, ToolContext& context)
{
    if (!m_drawing)
    {
        return false;
    }

    const sf::Color eraseColor = context.settings.backgroundColor;
    const auto stamp = [&](const sf::Vector2f& point) {
        if (context.targetImage != nullptr)
        {
            detail::drawBrushImage(*context.targetImage, point, context.settings.brushSize,
                                   eraseColor);
            detail::markStampDirty(context, point, context.settings.brushSize);
        }
        else if (context.target != nullptr)
        {
            detail::drawBrush(*context.target, point, context.settings.brushSize, eraseColor);
        }
    };

    m_path.finish(position, stamp);
    m_drawing = false;
    return true;
}

void EraserTool::renderPreview(sf::RenderTarget&, ToolContext&)
{
}

void EraserTool::cancel()
{
    m_drawing = false;
    m_path.reset({});
}
} // namespace minipaint
