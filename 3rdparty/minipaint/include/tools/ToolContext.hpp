#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include <algorithm>
#include <functional>

namespace minipaint
{
/// Mutable settings shared by all tool strategies.
struct ToolSettings
{
    sf::Color primaryColor{sf::Color::Black};
    sf::Color secondaryColor{sf::Color::White};
    sf::Color backgroundColor{sf::Color::White};
    float brushSize{3.0F};
    bool fillShapes{false};
    unsigned int fillTolerance{0};
};

/// Services and state made available to a tool during an interaction.
class ToolContext
{
public:
    /// Off-screen render target receiving tool drawing operations (for preview).
    sf::RenderTarget* target{nullptr};

    /// Target image receiving tool drawing operations (for layer painting).
    /// When non-null, tools draw directly to this image instead of `target`.
    sf::Image* targetImage{nullptr};

    /// Composited canvas pixels (read-only for tools), updated at commit time.
    const sf::Image* image{nullptr};

    ToolSettings settings;

    /// Canvas-space region (inclusive, canvas pixels) touched by the tool
    /// since the last resetDirty(). Lets the caller sync only that region to
    /// the composited canvas instead of recompositing every layer per event.
    bool dirtyValid{false};
    int dirtyMinX{0};
    int dirtyMinY{0};
    int dirtyMaxX{0};
    int dirtyMaxY{0};

    /// Union |rect| into the pending dirty region (inclusive coordinates).
    void markDirty(int minX, int minY, int maxX, int maxY)
    {
        if (dirtyValid)
        {
            dirtyMinX = std::min(dirtyMinX, minX);
            dirtyMinY = std::min(dirtyMinY, minY);
            dirtyMaxX = std::max(dirtyMaxX, maxX);
            dirtyMaxY = std::max(dirtyMaxY, maxY);
        }
        else
        {
            dirtyMinX = minX;
            dirtyMinY = minY;
            dirtyMaxX = maxX;
            dirtyMaxY = maxY;
            dirtyValid = true;
        }
    }

    void resetDirty() noexcept
    {
        dirtyValid = false;
    }

    /// Called by the eyedropper when a color is selected.
    std::function<void(sf::Color)> colorPicked;

    /// Called by tools that operate directly on pixels (bucket fill).
    std::function<void(const sf::Image&)> imageReplaced;
};

} // namespace minipaint
