#pragma once

#include "Tool.hpp"
#include "tools/SmoothPath.hpp"

#include <random>

namespace minipaint
{
/// Airbrush: random spray pattern around the cursor (like classic Paint).
class AirbrushTool final : public Tool
{
public:
    void onMousePressed(sf::Vector2f position, ToolContext& context) override;
    void onMouseMoved(sf::Vector2f position, ToolContext& context) override;
    bool onMouseReleased(sf::Vector2f position, ToolContext& context) override;
    void renderPreview(sf::RenderTarget& target, ToolContext& context) override;
    void cancel() override;
    [[nodiscard]] ToolType getToolType() const override { return ToolType::Airbrush; }

private:
    void spray(const sf::Vector2f& position, ToolContext& context, int count);

    [[nodiscard]] float unit() { return m_distribution(m_rng); }

    bool m_active{false};
    detail::SmoothPath m_path;
    std::mt19937 m_rng{std::random_device{}()};
    std::uniform_real_distribution<float> m_distribution{0.0F, 1.0F};
};
} // namespace minipaint
