#pragma once

#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <cmath>

namespace minipaint::detail
{
/// Smooths a stream of raw pointer positions into a fluid curve.
///
/// The pointer only delivers a handful of positions per second, so drawing
/// straight segments between them produces sharp, irregular corners. This
/// helper keeps the previous position and draws a quadratic Bezier through
/// the midpoints of consecutive samples, which yields the smooth, even
/// strokes expected from a paint application.
class SmoothPath
{
public:
    /// Starts a new path at the given position.
    void reset(const sf::Vector2f& point)
    {
        m_control = point;
        m_current = point;
        m_hasPrevious = true;
    }

    /// Feeds a new raw position and stamps the resulting curve segment.
    /// The stamp callback receives every interpolated point of the curve.
    template <typename Stamp>
    void advance(const sf::Vector2f& point, Stamp&& stamp)
    {
        if (!m_hasPrevious)
        {
            m_hasPrevious = true;
            m_control = point;
            m_current = point;
            stamp(point);
            return;
        }

        const sf::Vector2f mid = (m_control + point) * 0.5F;
        stampCurve(m_current, m_control, mid, stamp);
        m_current = mid;
        m_control = point;
    }

    /// Stamps the remaining curve up to the final position.
    template <typename Stamp>
    void finish(const sf::Vector2f& point, Stamp&& stamp)
    {
        if (!m_hasPrevious)
        {
            stamp(point);
            return;
        }

        stampCurve(m_current, m_control, point, stamp);
        m_current = point;
    }

private:
    template <typename Stamp>
    void stampCurve(const sf::Vector2f& from,
                    const sf::Vector2f& control,
                    const sf::Vector2f& to,
                    Stamp&& stamp)
    {
        const float length = std::hypot(to.x - from.x, to.y - from.y);
        const int steps = std::max(1, static_cast<int>(std::ceil(length)));
        for (int step = 1; step <= steps; ++step)
        {
            const float t = static_cast<float>(step) / static_cast<float>(steps);
            const float u = 1.0F - t;
            stamp({u * u * from.x + 2.0F * u * t * control.x + t * t * to.x,
                   u * u * from.y + 2.0F * u * t * control.y + t * t * to.y});
        }
    }

    sf::Vector2f m_control{};
    sf::Vector2f m_current{};
    bool m_hasPrevious{false};
};
} // namespace minipaint::detail
