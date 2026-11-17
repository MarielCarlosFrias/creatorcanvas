#include "tools/EllipseTool.hpp"

#include "tools/Drawing.hpp"
#include "tools/ToolContext.hpp"

namespace minipaint
{
void EllipseTool::onMousePressed(const sf::Vector2f position, ToolContext&)
{
    m_active = true;
    m_start = position;
    m_current = position;
}

void EllipseTool::onMouseMoved(const sf::Vector2f position, ToolContext&)
{
    if (m_active)
    {
        m_current = position;
    }
}

bool EllipseTool::onMouseReleased(const sf::Vector2f position, ToolContext& context)
{
    if (m_active)
    {
        m_current = position;
    }

    const bool modified = m_active;
    m_active = false;

    if (context.targetImage != nullptr)
    {
        detail::drawEllipseImage(*context.targetImage, m_start, m_current,
                                 context.settings.brushSize,
                                 context.settings.primaryColor,
                                 context.settings.fillShapes);
        detail::markSegmentDirty(context, m_start, m_current,
                                 context.settings.brushSize);
        return modified;
    }

    if (context.target != nullptr)
    {
        detail::drawEllipse(*context.target, m_start, m_current,
                            context.settings.brushSize,
                            context.settings.primaryColor,
                            context.settings.fillShapes);
    }

    return modified;
}

void EllipseTool::renderPreview(sf::RenderTarget& target, ToolContext& context)
{
    if (!m_active || context.target == nullptr)
    {
        return;
    }

    detail::drawEllipse(target, m_start, m_current,
                        context.settings.brushSize,
                        context.settings.primaryColor,
                        context.settings.fillShapes);
}

bool EllipseTool::supportsPreview() const
{
    return true;
}

void EllipseTool::cancel()
{
    m_active = false;
}
} // namespace minipaint
