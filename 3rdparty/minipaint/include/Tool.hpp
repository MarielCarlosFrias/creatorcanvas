#pragma once

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include <memory>
#include <string_view>

namespace minipaint
{
enum class ToolType
{
    Pencil,
    Eraser,
    Line,
    Rectangle,
    Ellipse,
    BucketFill,
    Eyedropper,
    Airbrush,
    Text
};

class ToolContext;

/// Strategy interface implemented by every drawing tool.
class Tool
{
public:
    virtual ~Tool() = default;

    virtual void onMousePressed(sf::Vector2f position, ToolContext& context) = 0;
    virtual void onMouseMoved(sf::Vector2f position, ToolContext& context) = 0;

    /// Returns true when the tool changed the canvas.
    virtual bool onMouseReleased(sf::Vector2f position, ToolContext& context) = 0;

    /// Renders the tool's current preview/final shape.
    virtual void renderPreview(sf::RenderTarget& target, ToolContext& context) = 0;

    /// True when the preview must be reconstructed from the stroke's base image.
    virtual bool supportsPreview() const
    {
        return false;
    }

    /// Cancels any in-progress interaction.
    virtual void cancel() = 0;

    /// Returns the tool type.
    [[nodiscard]] virtual ToolType getToolType() const = 0;
};

/// Creates a tool strategy by type.
[[nodiscard]] std::unique_ptr<Tool> makeTool(ToolType type);

/// Returns a stable, human-readable tool name.
[[nodiscard]] std::string_view toolName(ToolType type);

} // namespace minipaint
