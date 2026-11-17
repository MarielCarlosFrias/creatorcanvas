#pragma once

#include "Tool.hpp"

namespace minipaint
{
class EyedropperTool final : public Tool
{
public:
    void onMousePressed(sf::Vector2f position, ToolContext& context) override;
    void onMouseMoved(sf::Vector2f position, ToolContext& context) override;
    bool onMouseReleased(sf::Vector2f position, ToolContext& context) override;
    void renderPreview(sf::RenderTarget& target, ToolContext& context) override;
    void cancel() override;
    [[nodiscard]] ToolType getToolType() const override { return ToolType::Eyedropper; }
};
} // namespace minipaint
