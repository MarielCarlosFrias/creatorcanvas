#pragma once

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/View.hpp>

namespace minipaint::detail
{
/// Owns the off-screen render texture and its committed pixel mirror.
class Canvas
{
public:
    explicit Canvas(sf::Vector2u size);

    /// Replaces all pixels. The canvas is resized if required.
    void replaceImage(const sf::Image& image);

    /// Writes |region| at |origin| (canvas pixels) into the canvas and syncs
    /// ONLY that region: mirror copy, partial texture upload, one sprite blit
    /// and display. |region| must fit inside the canvas (caller clamps).
    /// Thousands of times cheaper than replaceImage() during a stroke.
    void updateRegion(const sf::Image& region, sf::Vector2u origin);

    /// Restores the last committed texture without rebuilding/uploading it.
    /// The caller is responsible for displaying the render target afterwards.
    void restoreCommittedTexture();

    [[nodiscard]] const sf::Image& image() const noexcept;
    [[nodiscard]] sf::Vector2u size() const noexcept;

    [[nodiscard]] sf::RenderTexture& target() noexcept;
    [[nodiscard]] const sf::RenderTexture& target() const noexcept;

private:
    void uploadCurrentImage();
    void resetView();

    sf::RenderTexture m_target;
    sf::Image m_currentImage;
    sf::Texture m_committedTexture;
    /// Scratch texture reused by updateRegion so incremental strokes do not
    /// allocate a GPU texture per mouse event (same lesson as
    /// m_committedTexture).
    sf::Texture m_regionTexture;
};
} // namespace minipaint::detail
