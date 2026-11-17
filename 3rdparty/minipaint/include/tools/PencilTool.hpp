#pragma once

#include "Tool.hpp"
#include "tools/SmoothPath.hpp"

namespace minipaint
{
/// Pencil: free-hand drawing with a smoothed, even-width stroke.
class PencilTool final : public Tool
{
public:
    void onMousePressed(sf::Vector2f position, ToolContext& context) override;
    void onMouseMoved(sf::Vector2f position, ToolContext& context) override;
    bool onMouseReleased(sf::Vector2f position, ToolContext& context) override;
    void renderPreview(sf::RenderTarget& target, ToolContext& context) override;
    void cancel() override;
    [[nodiscard]] ToolType getToolType() const override { return ToolType::Pencil; }

private:
    bool m_drawing{false};
    sf::Vector2f m_lastPoint{};
};
} // namespace minipaint
