#include "tools/RectangleTool.hpp"

#include "tools/Drawing.hpp"
#include "tools/ToolContext.hpp"

namespace minipaint
{
void RectangleTool::onMousePressed(const sf::Vector2f position, ToolContext&)
{
    m_active = true;
    m_start = position;
    m_current = position;
}

void RectangleTool::onMouseMoved(const sf::Vector2f position, ToolContext&)
{
    if (m_active)
    {
        m_current = position;
    }
}

bool RectangleTool::onMouseReleased(const sf::Vector2f position, ToolContext& context)
{
    if (m_active)
    {
        m_current = position;
    }

    const bool modified = m_active;
    m_active = false;

    if (context.targetImage != nullptr)
    {
        detail::drawRectangleImage(*context.targetImage, m_start, m_current,
                                   context.settings.brushSize,
                                   context.settings.primaryColor,
                                   context.settings.fillShapes);
        detail::markSegmentDirty(context, m_start, m_current,
                                 context.settings.brushSize);
        return modified;
    }

    if (context.target != nullptr)
    {
        detail::drawRectangle(*context.target, m_start, m_current,
                              context.settings.brushSize,
                              context.settings.primaryColor,
                              context.settings.fillShapes);
    }

    return modified;
}

void RectangleTool::renderPreview(sf::RenderTarget& target, ToolContext& context)
{
    if (!m_active || context.target == nullptr)
    {
        return;
    }

    detail::drawRectangle(target, m_start, m_current,
                          context.settings.brushSize,
                          context.settings.primaryColor,
                          context.settings.fillShapes);
}

bool RectangleTool::supportsPreview() const
{
    return true;
}

void RectangleTool::cancel()
{
    m_active = false;
}
} // namespace minipaint
