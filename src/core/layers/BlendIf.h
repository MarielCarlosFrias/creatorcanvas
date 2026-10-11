#pragma once

#include <QtGlobal>
#include <algorithm>
#include <array>
#include <vector>

namespace cc {

/// One Blend If slider pair: black = [low, high], white = [low, high], scale 0..255.
/// Ported from PhotoCraft's BlendRange (crates/doc/src/blend_if.rs).
struct BlendRange
{
    std::array<quint8, 2> black{0, 0};
    std::array<quint8, 2> white{255, 255};

    static const BlendRange FULL;

    bool isFull() const
    {
        return black[0] == 0 && black[1] == 0 && white[0] == 255 && white[1] == 255;
    }

    bool operator==(const BlendRange& other) const
    {
        return black == other.black && white == other.white;
    }

    bool operator!=(const BlendRange& other) const
    {
        return !(*this == other);
    }

    /// Calculates visibility weight in [0.0, 1.0] for pixel channel value v in [0..255].
    /// Linear transition ramp between split halves.
    float weight(float v) const
    {
        auto ramp = [](quint8 lo, quint8 hi, float x) -> float {
            float flo = static_cast<float>(lo);
            float fhi = static_cast<float>(hi);
            if (fhi <= flo)
                return (x >= flo) ? 1.0f : 0.0f;
            return std::clamp((x - flo) / (fhi - flo), 0.0f, 1.0f);
        };

        float lo = ramp(black[0], black[1], v);
        float hi = ramp(255 - white[1], 255 - white[0], 255.0f - v);
        return lo * hi;
    }
};

inline const BlendRange BlendRange::FULL = BlendRange{{0, 0}, {255, 255}};

/// A layer's Blend If settings.
/// ranges[0] = Gray / Luminance: [thisLayer, underlyingLayer]
/// Ported from PhotoCraft's BlendIf (crates/doc/src/blend_if.rs).
struct BlendIf
{
    // Index 0: Gray (Luminance). Index 1..3: R, G, B channels if needed.
    // Each entry has [thisLayer, underlyingLayer].
    std::vector<std::array<BlendRange, 2>> ranges;

    bool isDefault() const
    {
        for (const auto& pair : ranges) {
            if (!pair[0].isFull() || !pair[1].isFull())
                return false;
        }
        return true;
    }

    std::array<BlendRange, 2> get(std::size_t i) const
    {
        if (i < ranges.size())
            return ranges[i];
        return {BlendRange::FULL, BlendRange::FULL};
    }

    void set(std::size_t i, const std::array<BlendRange, 2>& pair)
    {
        if (ranges.size() <= i) {
            ranges.resize(i + 1, {BlendRange::FULL, BlendRange::FULL});
        }
        ranges[i] = pair;
        while (!ranges.empty() && ranges.back()[0].isFull() && ranges.back()[1].isFull()) {
            ranges.pop_back();
        }
    }

    bool operator==(const BlendIf& other) const
    {
        return ranges == other.ranges;
    }

    bool operator!=(const BlendIf& other) const
    {
        return !(*this == other);
    }
};

} // namespace cc
