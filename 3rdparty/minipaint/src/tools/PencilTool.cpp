#include "tools/PencilTool.hpp"

#include "tools/Drawing.hpp"
#include "tools/ToolContext.hpp"

namespace minipaint
{
void PencilTool::onMousePressed(const sf::Vector2f position, ToolContext& context)
{
    m_drawing = true;
    m_lastPoint = position;

    if (context.targetImage != nullptr)
    {
        detail::drawBrushImage(*context.targetImage, position, context.settings.brushSize,
                               context.settings.primaryColor);
        detail::markStampDirty(context, position, context.settings.brushSize);
        return;
    }

    if (context.target == nullptr)
    {
        return;
    }

    detail::drawBrush(*context.target, position, context.settings.brushSize,
                      context.settings.primaryColor);
}

void PencilTool::onMouseMoved(const sf::Vector2f position, ToolContext& context)
{
    if (!m_drawing)
    {
        return;
    }

    if (context.targetImage != nullptr)
    {
        detail::drawBrushSegmentImage(*context.targetImage, m_lastPoint, position,
                                      context.settings.brushSize,
                                      context.settings.primaryColor);
        detail::markSegmentDirty(context, m_lastPoint, position,
                                 context.settings.brushSize);
    }
    else if (context.target != nullptr)
    {
        detail::drawBrushSegment(*context.target, m_lastPoint, position,
                                 context.settings.brushSize,
                                 context.settings.primaryColor);
    }

    m_lastPoint = position;
}

bool PencilTool::onMouseReleased(const sf::Vector2f position, ToolContext& context)
{
    if (!m_drawing)
    {
        return false;
    }

    if (context.targetImage != nullptr)
    {
        detail::drawBrushSegmentImage(*context.targetImage, m_lastPoint, position,
                                      context.settings.brushSize,
                                      context.settings.primaryColor);
        detail::markSegmentDirty(context, m_lastPoint, position,
                                 context.settings.brushSize);
    }
    else if (context.target != nullptr)
    {
        detail::drawBrushSegment(*context.target, m_lastPoint, position,
                                 context.settings.brushSize,
                                 context.settings.primaryColor);
    }

    m_drawing = false;
    return true;
}

void PencilTool::renderPreview(sf::RenderTarget&, ToolContext&)
{
}

void PencilTool::cancel()
{
    m_drawing = false;
    m_lastPoint = {};
}
} // namespace minipaint
