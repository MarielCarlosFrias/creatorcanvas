#include "Drawing.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Vertex.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>

namespace minipaint::detail
{
namespace
{
constexpr float Pi = 3.14159265358979323846F;
constexpr float TwoPi = Pi * 2.0F;

// Anti-aliased brush texture: white disc with a soft alpha falloff, used for
// the GPU previews so they match the CPU rendering (which blends per pixel).
const sf::Texture& brushTexture()
{
    static const std::unique_ptr<sf::Texture> texture = [] {
        constexpr int size = 64;
        sf::Image image({static_cast<unsigned int>(size), static_cast<unsigned int>(size)},
                        sf::Color::White);
        const float center = size * 0.5F;
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const float dx = (static_cast<float>(x) + 0.5F) - center;
                const float dy = (static_cast<float>(y) + 0.5F) - center;
                const float coverage =
                    std::clamp(center - std::hypot(dx, dy), 0.0F, 1.0F);                image.setPixel({static_cast<unsigned int>(x), static_cast<unsigned int>(y)},
                               sf::Color(255, 255, 255,
                                         static_cast<std::uint8_t>(coverage * 255.0F)));
            }
        }
        return std::make_unique<sf::Texture>(image);
    }();
    return *texture;
}
template <typename Callback>
void forEachBresenhamPoint(int x0, int y0, int x1, int y1, Callback&& callback)
{
    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);
    const int sx = (x0 < x1) ? 1 : -1;
    const int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true)
    {
        callback(x0, y0);
        if (x0 == x1 && y0 == y1)
        {
            break;
        }
        const int e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}
} // namespace

void drawBrush(sf::RenderTarget& target,
               const sf::Vector2f center,
               const float diameter,
               const sf::Color& color)
{
    const int stamp = std::max(1, static_cast<int>(std::lround(std::max(1.0F, diameter))));
    const int centerX = static_cast<int>(std::round(center.x));
    const int centerY = static_cast<int>(std::round(center.y));

    // One and two pixel brushes are hard squares.
    if (stamp <= 2)
    {
        const int start = -(stamp / 2);
        const float size = static_cast<float>(stamp);
        sf::RectangleShape pixel({size, size});
        pixel.setPosition({static_cast<float>(centerX + start), static_cast<float>(centerY + start)});
        pixel.setFillColor(color);
        target.draw(pixel);
        return;
    }

    // Anti-aliased round brush, using the soft disc texture.
    const sf::Texture& texture = brushTexture();
    sf::Sprite sprite(texture);
    const float scale = static_cast<float>(stamp) / static_cast<float>(texture.getSize().x);
    sprite.setOrigin({static_cast<float>(texture.getSize().x) * 0.5F,
                      static_cast<float>(texture.getSize().y) * 0.5F});
    sprite.setPosition({static_cast<float>(centerX), static_cast<float>(centerY)});
    sprite.setScale({scale, scale});
    sprite.setColor(color);
    target.draw(sprite);
}

void drawBrushSegment(sf::RenderTarget& target,
                      const sf::Vector2f start,
                      const sf::Vector2f end,
                      const float diameter,
                      const sf::Color& color)
{
    const int stamp = std::max(1, static_cast<int>(std::lround(std::max(1.0F, diameter))));
    const int x0 = static_cast<int>(std::round(start.x));
    const int y0 = static_cast<int>(std::round(start.y));
    const int x1 = static_cast<int>(std::round(end.x));
    const int y1 = static_cast<int>(std::round(end.y));

    if (stamp <= 2)
    {
        forEachBresenhamPoint(x0, y0, x1, y1, [&](const int x, const int y) {
            drawBrush(target, {static_cast<float>(x), static_cast<float>(y)}, diameter, color);
        });
        return;
    }

    const sf::Vector2f delta = end - start;
    const float length = std::hypot(delta.x, delta.y);
    const float spacing = std::max(0.5F, static_cast<float>(stamp) * 0.2F);
    const auto steps = static_cast<unsigned int>(std::max(1.0F, std::ceil(length / spacing)));

    for (unsigned int step = 0; step <= steps; ++step)
    {
        const float amount = static_cast<float>(step) / static_cast<float>(steps);
        const sf::Vector2f point = start + delta * amount;
        drawBrush(target, point, diameter, color);
    }
}

void drawRectangle(sf::RenderTarget& target,
                   const sf::Vector2f start,
                   const sf::Vector2f end,
                   const float outlineThickness,
                   const sf::Color& color,
                   const bool filled)
{
    const float left = std::min(start.x, end.x);
    const float top = std::min(start.y, end.y);
    const float width = std::max(1.0F, std::abs(end.x - start.x));
    const float height = std::max(1.0F, std::abs(end.y - start.y));

    // Fill with the SFML shape (crisp edges), then draw the outline with the
    // same brush segments used by the image-based path so the preview and the
    // committed result are pixel-identical.
    if (filled)
    {
        sf::RectangleShape rectangle({width, height});
        rectangle.setPosition({left, top});
        rectangle.setFillColor(color);
        rectangle.setOutlineColor(sf::Color::Transparent);
        rectangle.setOutlineThickness(0.0F);
        target.draw(rectangle);
    }

    const float diameter = std::max(1.0F, outlineThickness);
    drawBrushSegment(target, {left, top}, {left + width, top}, diameter, color);
    drawBrushSegment(target, {left, top + height}, {left + width, top + height}, diameter, color);
    drawBrushSegment(target, {left, top}, {left, top + height}, diameter, color);
    drawBrushSegment(target, {left + width, top}, {left + width, top + height}, diameter, color);
}

void drawEllipse(sf::RenderTarget& target,
                 const sf::Vector2f start,
                 const sf::Vector2f end,
                 const float outlineThickness,
                 const sf::Color& color,
                 const bool filled)
{
    const sf::Vector2f center((start.x + end.x) * 0.5F, (start.y + end.y) * 0.5F);
    const float radiusX = std::abs(end.x - start.x) * 0.5F;
    const float radiusY = std::abs(end.y - start.y) * 0.5F;

    if (radiusX < 0.5F || radiusY < 0.5F)
    {
        const float thickness = std::max(1.0F, outlineThickness);
        if (radiusX < 0.5F && radiusY < 0.5F)
        {
            drawBrush(target, center, thickness, color);
        }
        else if (radiusY < 0.5F)
        {
            drawBrushSegment(target,
                             {center.x - radiusX, center.y},
                             {center.x + radiusX, center.y},
                             thickness,
                             color);
        }
        else
        {
            drawBrushSegment(target,
                             {center.x, center.y - radiusY},
                             {center.x, center.y + radiusY},
                             thickness,
                             color);
        }
        return;
    }

    constexpr std::size_t minimumFillSegments = 128U;
    constexpr std::size_t maximumOutlineSegments = 4096U;

    const auto ellipsePoint = [center, radiusX, radiusY](const std::size_t index,
                                                          const std::size_t segmentCount) {
        const float angle = static_cast<float>(index) * TwoPi / static_cast<float>(segmentCount);
        return sf::Vector2f(center.x + radiusX * std::cos(angle),
                            center.y + radiusY * std::sin(angle));
    };

    if (filled)
    {
        sf::VertexArray vertices(sf::PrimitiveType::TriangleFan, minimumFillSegments + 2U);
        vertices[0].position = center;
        vertices[0].color = color;

        for (std::size_t index = 0; index < minimumFillSegments; ++index)
        {
            const sf::Vector2f point = ellipsePoint(index, minimumFillSegments);
            vertices[index + 1U].position = point;
            vertices[index + 1U].color = color;
        }

        vertices[minimumFillSegments + 1U].position = vertices[1U].position;
        vertices[minimumFillSegments + 1U].color = vertices[1U].color;
        target.draw(vertices);
    }

    const float brushDiameter = std::max(1.0F, outlineThickness);
    const float brushRadius = brushDiameter * 0.5F;
    const float ellipsePerimeter = Pi *
                                   (3.0F * (radiusX + radiusY) -
                                    std::sqrt((3.0F * radiusX + radiusY) * (radiusX + 3.0F * radiusY)));
    const float sampleSpacing = std::max(0.5F, brushRadius * 0.75F);
    const auto requiredSegments = static_cast<std::size_t>(
        std::max(1.0F, std::ceil(ellipsePerimeter / sampleSpacing)));
    const std::size_t outlineSegments = std::min(maximumOutlineSegments,
                                                 std::max<std::size_t>(minimumFillSegments, requiredSegments));

    for (std::size_t index = 0; index < outlineSegments; ++index)
    {
        const std::size_t next = (index + 1U) % outlineSegments;
        drawBrushSegment(target,
                         ellipsePoint(index, outlineSegments),
                         ellipsePoint(next, outlineSegments),
                         brushDiameter,
                         color);
    }
}

void drawBrushImage(sf::Image& image,
                    const sf::Vector2f center,
                    const float diameter,
                    const sf::Color& color)
{
    const int centerX = static_cast<int>(std::round(center.x));
    const int centerY = static_cast<int>(std::round(center.y));
    const auto size = image.getSize();

    const int stamp = std::max(1, static_cast<int>(std::lround(std::max(1.0F, diameter))));

    // One and two pixel brushes are hard squares, exactly like MS Paint.
    if (stamp <= 2)
    {
        const int start = -(stamp / 2);
        const int end = start + stamp - 1;
        for (int dy = start; dy <= end; ++dy)
        {
            for (int dx = start; dx <= end; ++dx)
            {
                const int x = centerX + dx;
                const int y = centerY + dy;
                if (x >= 0 && x < static_cast<int>(size.x) &&
                    y >= 0 && y < static_cast<int>(size.y))
                {
                    image.setPixel({static_cast<unsigned int>(x),
                                    static_cast<unsigned int>(y)}, color);
                }
            }
        }
        return;
    }

    // Bigger brushes are round and anti-aliased: every pixel is blended with
    // the amount of coverage it gets, so diagonals and curves look clean
    // instead of stair-stepped.
    const float radius = static_cast<float>(stamp) * 0.5F;
    const int reach = static_cast<int>(std::ceil(radius)) + 1;

    for (int dy = -reach; dy <= reach; ++dy)
    {
        for (int dx = -reach; dx <= reach; ++dx)
        {
            const float distance = std::hypot(static_cast<float>(dx),
                                              static_cast<float>(dy));
            const float coverage = std::clamp(radius + 0.5F - distance, 0.0F, 1.0F);
            if (coverage <= 0.0F)
            {
                continue;
            }

            const int x = centerX + dx;
            const int y = centerY + dy;
            if (x < 0 || x >= static_cast<int>(size.x) ||
                y < 0 || y >= static_cast<int>(size.y))
            {
                continue;
            }

            const sf::Color destination =
                image.getPixel({static_cast<unsigned int>(x), static_cast<unsigned int>(y)});
            const float inverse = 1.0F - coverage;
            const auto mix = [&](const std::uint8_t src, const std::uint8_t dst) {
                return static_cast<std::uint8_t>(
                    std::clamp(static_cast<float>(src) * coverage +
                                   static_cast<float>(dst) * inverse,
                               0.0F, 255.0F));
            };

            image.setPixel({static_cast<unsigned int>(x), static_cast<unsigned int>(y)},
                           sf::Color(mix(color.r, destination.r),
                                     mix(color.g, destination.g),
                                     mix(color.b, destination.b),
                                     mix(color.a, destination.a)));
        }
    }
}

void drawBrushSegmentImage(sf::Image& image,
                           const sf::Vector2f start,
                           const sf::Vector2f end,
                           const float diameter,
                           const sf::Color& color)
{
    const int stamp = std::max(1, static_cast<int>(std::lround(std::max(1.0F, diameter))));
    const int x0 = static_cast<int>(std::round(start.x));
    const int y0 = static_cast<int>(std::round(start.y));
    const int x1 = static_cast<int>(std::round(end.x));
    const int y1 = static_cast<int>(std::round(end.y));

    if (stamp <= 2)
    {
        forEachBresenhamPoint(x0, y0, x1, y1, [&](const int x, const int y) {
            drawBrushImage(image, {static_cast<float>(x), static_cast<float>(y)}, diameter, color);
        });
        return;
    }

    const sf::Vector2f delta = end - start;
    const float length = std::hypot(delta.x, delta.y);
    const float spacing = std::max(0.5F, static_cast<float>(stamp) * 0.2F);
    const auto steps = static_cast<unsigned int>(std::max(1.0F, std::ceil(length / spacing)));

    for (unsigned int step = 0; step <= steps; ++step)
    {
        const float amount = static_cast<float>(step) / static_cast<float>(steps);
        const sf::Vector2f point = start + delta * amount;
        drawBrushImage(image, point, diameter, color);
    }
}

void drawRectangleImage(sf::Image& image,
                        const sf::Vector2f start,
                        const sf::Vector2f end,
                        const float outlineThickness,
                        const sf::Color& color,
                        const bool filled)
{
    const float left = std::min(start.x, end.x);
    const float top = std::min(start.y, end.y);
    const float right = std::max(start.x, end.x);
    const float bottom = std::max(start.y, end.y);

    const auto size = image.getSize();

    if (filled)
    {
        const int l = static_cast<int>(std::floor(left));
        const int t = static_cast<int>(std::floor(top));
        const int r = static_cast<int>(std::ceil(right));
        const int b = static_cast<int>(std::ceil(bottom));
        const int endX = std::max(l + 1, r);
        const int endY = std::max(t + 1, b);

        for (int y = t; y < endY; ++y)
        {
            for (int x = l; x < endX; ++x)
            {
                if (x >= 0 && x < static_cast<int>(size.x) &&
                    y >= 0 && y < static_cast<int>(size.y))
                {
                    image.setPixel({static_cast<unsigned int>(x),
                                    static_cast<unsigned int>(y)}, color);
                }
            }
        }
        return;
    }

    // Draw outline using brush segments for proper thickness and corner coverage.
    const float diameter = std::max(1.0F, outlineThickness);
    drawBrushSegmentImage(image, {left, top}, {right, top}, diameter, color);
    drawBrushSegmentImage(image, {left, bottom}, {right, bottom}, diameter, color);
    drawBrushSegmentImage(image, {left, top}, {left, bottom}, diameter, color);
    drawBrushSegmentImage(image, {right, top}, {right, bottom}, diameter, color);
}

void drawEllipseImage(sf::Image& image,
                      const sf::Vector2f start,
                      const sf::Vector2f end,
                      const float outlineThickness,
                      const sf::Color& color,
                      const bool filled)
{
    const sf::Vector2f center((start.x + end.x) * 0.5F, (start.y + end.y) * 0.5F);
    const float radiusX = std::abs(end.x - start.x) * 0.5F;
    const float radiusY = std::abs(end.y - start.y) * 0.5F;
    const float thickness = std::max(1.0F, outlineThickness);
    const auto size = image.getSize();

    const int cx = static_cast<int>(std::round(center.x));
    const int cy = static_cast<int>(std::round(center.y));
    const int rx = static_cast<int>(std::ceil(radiusX));
    const int ry = static_cast<int>(std::ceil(radiusY));

    if (rx < 1 || ry < 1)
    {
        if (rx < 1 && ry < 1)
        {
            drawBrushImage(image, center, thickness, color);
        }
        else if (ry < 1)
        {
            drawBrushSegmentImage(image,
                                  {center.x - radiusX, center.y},
                                  {center.x + radiusX, center.y},
                                  thickness,
                                  color);
        }
        else
        {
            drawBrushSegmentImage(image,
                                  {center.x, center.y - radiusY},
                                  {center.x, center.y + radiusY},
                                  thickness,
                                  color);
        }
        return;
    }

    if (filled)
    {
        // Fill ellipse using scanline approach
        for (int y = -ry; y <= ry; ++y)
        {
            const float dy = static_cast<float>(y);
            const float yNorm = dy / static_cast<float>(ry);
            const float xHalf = radiusX * std::sqrt(std::max(0.0F, 1.0F - yNorm * yNorm));
            const int xStart = static_cast<int>(std::ceil(center.x - xHalf));
            const int xEnd = static_cast<int>(std::floor(center.x + xHalf));
            const int yy = cy + y;

            if (yy >= 0 && yy < static_cast<int>(size.y))
            {
                for (int x = xStart; x <= xEnd; ++x)
                {
                    if (x >= 0 && x < static_cast<int>(size.x))
                    {
                        image.setPixel({static_cast<unsigned int>(x),
                                        static_cast<unsigned int>(yy)}, color);
                    }
                }
            }
        }
        return;
    }

    // Outline: draw ellipse stroke using the same brush as the GPU preview.
    for (int angle = 0; angle < 360; ++angle)
    {
        const float a = static_cast<float>(angle) * Pi / 180.0F;
        const int x = cx + static_cast<int>(std::round(radiusX * std::cos(a)));
        const int y = cy + static_cast<int>(std::round(radiusY * std::sin(a)));
        drawBrushImage(image, {static_cast<float>(x), static_cast<float>(y)}, thickness, color);
    }
}
} // namespace minipaint::detail
