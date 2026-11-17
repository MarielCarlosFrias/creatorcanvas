#include "tools/TextTool.hpp"
#include "tools/ToolContext.hpp"

namespace minipaint
{
void TextTool::onMousePressed(const sf::Vector2f position, ToolContext& context)
{
    m_placed = true;
    m_cursorPosition = position;

    // Notify the host that a text cursor was placed at this position.
    // The host should open a text entry dialog and then draw the text.
    if (context.colorPicked)
    {
        // Use colorPicked as a signal to place text cursor (text color = current primary)
        context.colorPicked(context.settings.primaryColor);
    }
}

void TextTool::onMouseMoved(const sf::Vector2f, ToolContext&)
{
}

bool TextTool::onMouseReleased(const sf::Vector2f, ToolContext&)
{
    const bool placed = m_placed;
    m_placed = false;
    return placed;
}

void TextTool::renderPreview(sf::RenderTarget&, ToolContext&)
{
}

void TextTool::cancel()
{
    m_placed = false;
}
} // namespace minipaint
