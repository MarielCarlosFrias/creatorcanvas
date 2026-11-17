#pragma once

#include "Tool.hpp"
#include "tools/SmoothPath.hpp"

namespace minipaint
{
/// Eraser: paints with the background colour using a smoothed stroke.
class EraserTool final : public Tool
{
public:
    void onMousePressed(sf::Vector2f position, ToolContext& context) override;
    void onMouseMoved(sf::Vector2f position, ToolContext& context) override;
    bool onMouseReleased(sf::Vector2f position, ToolContext& context) override;
    void renderPreview(sf::RenderTarget& target, ToolContext& context) override;
    void cancel() override;
    [[nodiscard]] ToolType getToolType() const override { return ToolType::Eraser; }

private:
    bool m_drawing{false};
    detail::SmoothPath m_path;
};
} // namespace minipaint
