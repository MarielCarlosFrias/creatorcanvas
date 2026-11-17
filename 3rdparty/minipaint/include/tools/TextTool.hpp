#pragma once

#include "Tool.hpp"

namespace minipaint
{
/// Text tool: places a text cursor at click position.
/// The host application is responsible for opening a text entry dialog.
class TextTool final : public Tool
{
public:
    void onMousePressed(sf::Vector2f position, ToolContext& context) override;
    void onMouseMoved(sf::Vector2f position, ToolContext& context) override;
    bool onMouseReleased(sf::Vector2f position, ToolContext& context) override;
    void renderPreview(sf::RenderTarget& target, ToolContext& context) override;
    void cancel() override;
    [[nodiscard]] ToolType getToolType() const override { return ToolType::Text; }

private:
    bool m_placed{false};
    sf::Vector2f m_cursorPosition{};
};
} // namespace minipaint
