#include "tools/LineTool.hpp"

#include "tools/Drawing.hpp"
#include "tools/ToolContext.hpp"

#include <SFML/Window/Keyboard.hpp>

#include <cmath>

namespace minipaint
{
namespace
{
sf::Vector2f applyConstraint(const sf::Vector2f start, const sf::Vector2f target)
{
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift))
    {
        const sf::Vector2f delta = target - start;
        const float length = std::hypot(delta.x, delta.y);
        if (length > 0.001F)
        {
            constexpr float Pi = 3.14159265358979323846F;
            constexpr float Step = Pi / 4.0F; // Snap to 45 degree increments
            const float angle = std::atan2(delta.y, delta.x);
            const float snapped = std::round(angle / Step) * Step;
            return start + sf::Vector2f(std::cos(snapped), std::sin(snapped)) * length;
        }
    }
    return target;
}
} // namespace

void LineTool::onMousePressed(const sf::Vector2f position, ToolContext&)
{
    m_active = true;
    m_start = position;
    m_current = position;
}

void LineTool::onMouseMoved(const sf::Vector2f position, ToolContext&)
{
    if (m_active)
    {
        m_current = applyConstraint(m_start, position);
    }
}

bool LineTool::onMouseReleased(const sf::Vector2f position, ToolContext& context)
{
    if (m_active)
    {
        m_current = applyConstraint(m_start, position);
    }

    const bool modified = m_active;
    m_active = false;

    if (context.targetImage != nullptr)
    {
        detail::drawBrushSegmentImage(*context.targetImage, m_start, m_current,
                                      context.settings.brushSize,
                                      context.settings.primaryColor);
        detail::markSegmentDirty(context, m_start, m_current,
                                 context.settings.brushSize);
        return modified;
    }

    if (context.target != nullptr)
    {
        detail::drawBrushSegment(*context.target, m_start, m_current,
                                 context.settings.brushSize,
                                 context.settings.primaryColor);
    }

    return modified;
}

void LineTool::renderPreview(sf::RenderTarget& target, ToolContext& context)
{
    if (!m_active || context.target == nullptr)
    {
        return;
    }

    detail::drawBrushSegment(target, m_start, m_current,
                             context.settings.brushSize,
                             context.settings.primaryColor);
}

bool LineTool::supportsPreview() const
{
    return true;
}

void LineTool::cancel()
{
    m_active = false;
}
} // namespace minipaint
