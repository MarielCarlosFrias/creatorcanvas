#pragma once

#include "Tool.hpp"

namespace minipaint
{
class EllipseTool final : public Tool
{
public:
    void onMousePressed(sf::Vector2f position, ToolContext& context) override;
    void onMouseMoved(sf::Vector2f position, ToolContext& context) override;
    bool onMouseReleased(sf::Vector2f position, ToolContext& context) override;
    void renderPreview(sf::RenderTarget& target, ToolContext& context) override;
    bool supportsPreview() const override;
    void cancel() override;
    [[nodiscard]] ToolType getToolType() const override { return ToolType::Ellipse; }

private:
    bool m_active{false};
    sf::Vector2f m_start{};
    sf::Vector2f m_current{};
};
} // namespace minipaint
