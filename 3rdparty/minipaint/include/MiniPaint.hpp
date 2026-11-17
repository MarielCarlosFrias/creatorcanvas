#pragma once

#include "Layer.hpp"
#include "Tool.hpp"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Window/Event.hpp>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace minipaint
{
namespace detail
{
class Canvas;
}

/// A reusable, window-independent Paint-like editor with layer support.
///
/// MiniPaint owns a stack of layers and an off-screen SFML render texture
/// used for composited display. The host application forwards mouse events
/// to handleEvent(), draws the composited canvas with draw() (or
/// applyToTexture()), and retrieves the edited pixels with getResult().
class MiniPaint
{
public:
    static constexpr std::size_t DefaultUndoLimit = 20;

    explicit MiniPaint(unsigned int width = 800, unsigned int height = 600);
    ~MiniPaint();

    MiniPaint(const MiniPaint&) = delete;
    MiniPaint& operator=(const MiniPaint&) = delete;

    // --------------------------------------------------------------- Layers --

    /// Adds a new layer and makes it active. Returns the new layer index.
    [[nodiscard]] std::size_t addLayer(std::string name,
                                       sf::Color fillColor = sf::Color::Transparent);

    /// Removes the layer at the given index. Returns false if out of range.
    bool removeLayer(std::size_t index);

    /// Returns the number of layers.
    [[nodiscard]] std::size_t getLayerCount() const noexcept;

    /// Returns the index of the currently active layer.
    [[nodiscard]] std::size_t getActiveLayerIndex() const noexcept;

    /// Sets the active layer. Returns false if out of range.
    bool setActiveLayer(std::size_t index);

    /// Returns the name of the layer at the given index.
    [[nodiscard]] const std::string& getLayerName(std::size_t index) const;

    /// Sets the name of the layer at the given index.
    bool setLayerName(std::size_t index, std::string name);

    /// Returns the opacity of the layer (0.0 = transparent, 1.0 = opaque).
    [[nodiscard]] float getLayerOpacity(std::size_t index) const;

    /// Sets the opacity of the layer (clamped to [0, 1]).
    void setLayerOpacity(std::size_t index, float opacity);

    /// Returns whether the layer is visible.
    [[nodiscard]] bool getLayerVisibility(std::size_t index) const;

    /// Sets the layer visibility.
    void setLayerVisibility(std::size_t index, bool visible);

    /// Returns whether the layer is locked (cannot be painted on).
    [[nodiscard]] bool getLayerLocked(std::size_t index) const;

    /// Sets the layer locked state.
    void setLayerLocked(std::size_t index, bool locked);

    /// Returns a reference to the active layer's image.
    [[nodiscard]] const sf::Image& getActiveLayerImage() const;

    /// Returns the size of the active layer (or the canvas if no layers).
    [[nodiscard]] sf::Vector2u getSize() const noexcept;

    // ------------------------------------------------------------ Events -----

    /// Feeds an SFML event using the viewport most recently configured.
    void handleEvent(const sf::Event& event);

    /// Feeds an SFML event and maps mouse coordinates through viewport.
    /// The viewport is expressed in the host target's screen coordinates.
    void handleEvent(const sf::Event& event, const sf::FloatRect& viewport);

    // ------------------------------------------------------------ Display ---

    /// Draws the composited canvas at its native pixel size, starting at (0, 0).
    void draw(sf::RenderTarget& target) const;

    /// Draws the composited canvas into destination, scaling when necessary.
    void draw(sf::RenderTarget& target, const sf::FloatRect& destination) const;

    /// Draws the composited canvas at the origin of an external render texture.
    void applyToTexture(sf::RenderTexture& target) const;

    /// Draws the composited canvas at the origin of any external render target.
    void applyToTexture(sf::RenderTarget& target) const;

    /// Returns the texture owned by the internal render texture (composited).
    const sf::Texture& getCanvasTexture() const;

    /// Returns a composite of all visible layers as an image.
    sf::Image getResult() const;

    // --------------------------------------------------------- Load / Save ---

    /// Replaces all layer content with a single layer and clears history.
    /// The canvas is resized when the image dimensions differ.
    bool loadFromImage(const sf::Image& image);

    /// Loads a PNG image as a single layer, clears history, resizes if needed.
    bool loadFromFile(const std::filesystem::path& path);

    /// Saves the composited canvas as a PNG file.
    bool saveToFile(const std::filesystem::path& path) const;

    // ------------------------------------------------------------ Edit -----

    /// Clears the active layer to backgroundColor as an undoable command.
    void clear(sf::Color backgroundColor = sf::Color::White);

    /// Mirrors the active layer as an undoable edit.
    bool flipActiveLayerHorizontal();
    bool flipActiveLayerVertical();

    /// Rotates the active layer by 90 degrees.
    bool rotateActiveLayer(bool clockwise = true);

    /// Resizes the complete document while preserving all layer pixels at the
    /// top-left. Dimensions are validated by the implementation.
    bool resizeCanvas(sf::Vector2u newSize,
                      sf::Color fillColor = sf::Color::Transparent);

    /// Rasterizes editable text onto the active layer as an undoable edit.
    bool drawText(const sf::Font& font, const std::string& text,
                  sf::Vector2f position, unsigned int characterSize = 24);

    // ----------------------------------------------------------- Tools -----

    void setTool(ToolType tool);
    [[nodiscard]] ToolType getTool() const;

    // ------------------------------------------------------------ Colors ---

    void setPrimaryColor(sf::Color color);
    [[nodiscard]] sf::Color getPrimaryColor() const;

    /// Sets the color used by right-button drawing operations.
    void setSecondaryColor(sf::Color color);
    [[nodiscard]] sf::Color getSecondaryColor() const;

    void setBackgroundColor(sf::Color color);
    [[nodiscard]] sf::Color getBackgroundColor() const;

    // ---------------------------------------------------------- Brush ------

    /// Sets the brush diameter in pixels. Values are clamped to [1, 128].
    void setBrushSize(float diameter);
    [[nodiscard]] float getBrushSize() const;

    // -------------------------------------------------------- Shape fill ---

    /// Enables or disables filling for rectangle and ellipse tools.
    void setFillShapes(bool enabled);
    [[nodiscard]] bool getFillShapes() const;

    // ------------------------------------------------------- Bucket fill ---

    /// Sets the maximum per-channel difference accepted by bucket fill.
    void setFillTolerance(unsigned int tolerance);
    [[nodiscard]] unsigned int getFillTolerance() const;

    // ----------------------------------------------------------- Palette ---

    /// Replaces the palette and synchronizes the primary color with the
    /// selected entry (or keeps the current color if the palette is empty).
    void setPalette(std::vector<sf::Color> colors);
    [[nodiscard]] const std::vector<sf::Color>& getPalette() const;

    /// Selects a palette entry and updates the primary color.
    bool selectPaletteColor(std::size_t index);

    [[nodiscard]] std::size_t getSelectedPaletteIndex() const;

    // ------------------------------------------------------- Undo / Redo ---

    /// Undoes the latest command. Returns false when there is nothing to undo.
    bool undo();

    /// Redoes the latest undone command. Returns false when unavailable.
    bool redo();

    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;

    void setUndoLimit(std::size_t limit);
    [[nodiscard]] std::size_t getUndoLimit() const;
    void clearHistory();

    // ----------------------------------------------------------- Viewport --

    /// Sets the screen-space rectangle used to map incoming mouse events.
    void setViewport(const sf::FloatRect& viewport);
    [[nodiscard]] const sf::FloatRect& getViewport() const;

    [[nodiscard]] bool isDrawing() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace minipaint

// Convenient global alias matching the integration example in the project plan.
using MiniPaint = minipaint::MiniPaint;
