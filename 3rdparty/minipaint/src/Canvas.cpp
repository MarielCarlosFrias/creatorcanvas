#include "Canvas.hpp"

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <cstring>
#include <stdexcept>

namespace minipaint::detail
{
namespace
{
sf::Vector2u validateSize(const sf::Vector2u size)
{
    if (size.x == 0U || size.y == 0U)
    {
        throw std::invalid_argument("MiniPaint canvas dimensions must be greater than zero");
    }

    return size;
}
} // namespace

Canvas::Canvas(const sf::Vector2u size)
    : m_target(validateSize(size))
    , m_currentImage()
{
    m_currentImage.resize(size, sf::Color::White);
    resetView();
    uploadCurrentImage();
}

void Canvas::replaceImage(const sf::Image& image)
{
    if (image.getSize() != m_currentImage.getSize())
    {
        if (!m_target.resize(image.getSize()))
        {
            throw std::runtime_error("Failed to resize the MiniPaint render texture");
        }

        m_currentImage.resize(image.getSize(), sf::Color::Transparent);
        resetView();
    }

    m_currentImage = image;
    uploadCurrentImage();
}

void Canvas::restoreCommittedTexture()
{
    if (m_committedTexture.getSize().x == 0U || m_committedTexture.getSize().y == 0U)
    {
        return;
    }

    sf::Sprite sprite(m_committedTexture);
    m_target.clear(sf::Color::Transparent);
    m_target.draw(sprite, sf::RenderStates(sf::BlendNone));
}

void Canvas::updateRegion(const sf::Image& region, const sf::Vector2u origin)
{
    const sf::Vector2u regionSize = region.getSize();
    if (regionSize.x == 0U || regionSize.y == 0U)
    {
        return;
    }

    const sf::Vector2u canvasSize = m_currentImage.getSize();
    if (origin.x > canvasSize.x - regionSize.x ||
        origin.y > canvasSize.y - regionSize.y)
    {
        // Region escapes the canvas: fall back to the safe full path.
        replaceImage(region);
        return;
    }

    // 1) CPU mirror: copy just the region rows.
    {
        const std::uint8_t* src = region.getPixelsPtr();
        std::uint8_t* dst = const_cast<std::uint8_t*>(m_currentImage.getPixelsPtr());
        const std::size_t rowBytes = static_cast<std::size_t>(regionSize.x) * 4U;
        const std::size_t dstStride = static_cast<std::size_t>(canvasSize.x) * 4U;

        for (unsigned int y = 0; y < regionSize.y; ++y)
        {
            std::memcpy(dst + static_cast<std::size_t>(origin.y + y) * dstStride
                            + static_cast<std::size_t>(origin.x) * 4U,
                        src + static_cast<std::size_t>(y) * rowBytes,
                        rowBytes);
        }
    }

    // 2) GPU: partial update of the committed texture (no full re-upload).
    if (m_committedTexture.getSize() != canvasSize)
    {
        m_committedTexture = sf::Texture(m_currentImage);
    }
    else
    {
        m_committedTexture.update(region.getPixelsPtr(), regionSize, origin);
    }

    // 3) Blit only the region into the render target, replacing that rect.
    if (m_regionTexture.getSize() != regionSize)
    {
        m_regionTexture = sf::Texture(region);
    }
    else
    {
        m_regionTexture.update(region);
    }

    sf::Sprite sprite(m_regionTexture);
    sprite.setPosition(sf::Vector2f{static_cast<float>(origin.x),
                                    static_cast<float>(origin.y)});
    m_target.draw(sprite, sf::RenderStates(sf::BlendNone));
    m_target.display();
}

const sf::Image& Canvas::image() const noexcept
{
    return m_currentImage;
}

sf::Vector2u Canvas::size() const noexcept
{
    return m_currentImage.getSize();
}

sf::RenderTexture& Canvas::target() noexcept
{
    return m_target;
}

const sf::RenderTexture& Canvas::target() const noexcept
{
    return m_target;
}

void Canvas::uploadCurrentImage()
{
    // Reuse the GPU allocation while painting. Recreating an sf::Texture for
    // every mouse event caused visible stalls and unnecessary driver work.
    if (m_committedTexture.getSize() == m_currentImage.getSize())
    {
        m_committedTexture.update(m_currentImage);
    }
    else
    {
        m_committedTexture = sf::Texture(m_currentImage);
    }
    sf::Sprite sprite(m_committedTexture);

    m_target.clear(sf::Color::Transparent);
    m_target.draw(sprite, sf::RenderStates(sf::BlendNone));
    m_target.display();
}

void Canvas::resetView()
{
    const sf::Vector2u size = m_target.getTexture().getSize();
    m_target.setView(sf::View(sf::FloatRect({0.0F, 0.0F}, {static_cast<float>(size.x), static_cast<float>(size.y)})));
}
} // namespace minipaint::detail
