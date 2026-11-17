#pragma once

#include <SFML/Graphics/Color.hpp>

#include <cstdint>

namespace minipaint::detail
{
/// A horizontal run of pixels whose before/after colors are identical.
struct PixelRun
{
    std::uint32_t startIndex{0U};
    std::uint32_t length{0U};
    std::uint32_t beforeColor{0U};
    std::uint32_t afterColor{0U};
};

inline std::uint32_t packColor(const sf::Color& color)
{
    return static_cast<std::uint32_t>(color.r) |
           (static_cast<std::uint32_t>(color.g) << 8U) |
           (static_cast<std::uint32_t>(color.b) << 16U) |
           (static_cast<std::uint32_t>(color.a) << 24U);
}

inline sf::Color unpackColor(const std::uint32_t color)
{
    return {static_cast<std::uint8_t>(color & 0xFFU),
            static_cast<std::uint8_t>((color >> 8U) & 0xFFU),
            static_cast<std::uint8_t>((color >> 16U) & 0xFFU),
            static_cast<std::uint8_t>((color >> 24U) & 0xFFU)};
}
} // namespace minipaint::detail
