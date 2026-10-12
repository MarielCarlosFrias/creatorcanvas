#pragma once

#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <vector>

namespace cc {

struct CurvePoint
{
    float input = 0.0f;
    float output = 0.0f;

    bool operator==(const CurvePoint& o) const
    {
        return qFuzzyCompare(input, o.input) && qFuzzyCompare(output, o.output);
    }
};

/// Ported from PhotoCraft's LevelsChannel (crates/doc/src/adjust.rs & compose/src/adjust.rs)
struct LevelsChannel
{
    float inBlack = 0.0f;    // 0..255 normalized to 0.0..1.0
    float inWhite = 1.0f;    // 0..255 normalized to 0.0..1.0
    float gamma = 1.0f;      // Midtones gamma (0.1 .. 9.99)
    float outBlack = 0.0f;   // 0..255 normalized to 0.0..1.0
    float outWhite = 1.0f;   // 0..255 normalized to 0.0..1.0

    bool isIdentity() const
    {
        return inBlack <= 0.0001f && inWhite >= 0.9999f
            && std::abs(gamma - 1.0f) <= 0.0001f
            && outBlack <= 0.0001f && outWhite >= 0.9999f;
    }

    bool operator==(const LevelsChannel& o) const
    {
        return qFuzzyCompare(inBlack, o.inBlack)
            && qFuzzyCompare(inWhite, o.inWhite)
            && qFuzzyCompare(gamma, o.gamma)
            && qFuzzyCompare(outBlack, o.outBlack)
            && qFuzzyCompare(outWhite, o.outWhite);
    }
};

class ToneCurve
{
public:
    static constexpr int LUT_SIZE = 256;

    /// Evaluates Levels equation ported from PhotoCraft
    static inline float evaluateLevels(const LevelsChannel& ch, float v)
    {
        const float range = std::max(1e-6f, ch.inWhite - ch.inBlack);
        const float u = std::clamp((v - ch.inBlack) / range, 0.0f, 1.0f);
        const float g = std::max(0.01f, ch.gamma);

        float t = 0.0f;
        if (g > 1.0f) {
            const float power = std::pow(u, 1.0f / g);
            const float line = 0.93f * std::pow(2.0f, g) * u;
            if (power <= 1e-6f || line <= 1e-6f) {
                t = std::min(power, line);
            } else {
                constexpr float p = 10.0f;
                t = std::pow(std::pow(power, -p) + std::pow(line, -p), -1.0f / p);
            }
        } else {
            t = std::pow(u, 1.0f / g);
        }
        return std::clamp(ch.outBlack + t * (ch.outWhite - ch.outBlack), 0.0f, 1.0f);
    }

    /// Evaluates natural cubic spline ported from PhotoCraft (curve_lut)
    static inline std::vector<quint8> buildCurveLut(std::vector<CurvePoint> points)
    {
        std::vector<quint8> lut(LUT_SIZE);
        std::sort(points.begin(), points.end(), [](const CurvePoint& a, const CurvePoint& b) {
            return a.input < b.input;
        });

        // Deduplicate
        points.erase(std::unique(points.begin(), points.end(), [](const CurvePoint& a, const CurvePoint& b) {
            return std::abs(a.input - b.input) < 1e-6f;
        }), points.end());

        if (points.size() < 2) {
            for (int i = 0; i < LUT_SIZE; ++i)
                lut[i] = static_cast<quint8>(i);
            return lut;
        }

        const size_t n = points.size();
        std::vector<double> m2(n, 0.0);
        if (n > 2) {
            std::vector<double> x(n);
            std::vector<double> y(n);
            for (size_t i = 0; i < n; ++i) {
                x[i] = points[i].input;
                y[i] = points[i].output;
            }
            std::vector<double> c(n, 0.0);
            std::vector<double> d(n, 0.0);
            for (size_t i = 1; i < n - 1; ++i) {
                const double h0 = x[i] - x[i - 1];
                const double h1 = x[i + 1] - x[i];
                const double a = h0 / 6.0;
                const double b = (h0 + h1) / 3.0;
                const double cc = h1 / 6.0;
                const double r = (y[i + 1] - y[i]) / h1 - (y[i] - y[i - 1]) / h0;
                const double denom = b - a * c[i - 1];
                c[i] = cc / denom;
                d[i] = (r - a * d[i - 1]) / denom;
            }
            for (int i = static_cast<int>(n) - 2; i >= 1; --i) {
                m2[i] = d[i] - c[i] * m2[i + 1];
            }
        }

        for (int k = 0; k < LUT_SIZE; ++k) {
            const float xv = static_cast<float>(k) / static_cast<float>(LUT_SIZE - 1);
            if (xv <= points.front().input) {
                lut[k] = static_cast<quint8>(std::round(std::clamp(points.front().output, 0.0f, 1.0f) * 255.0f));
                continue;
            }
            if (xv >= points.back().input) {
                lut[k] = static_cast<quint8>(std::round(std::clamp(points.back().output, 0.0f, 1.0f) * 255.0f));
                continue;
            }

            size_t idx = 0;
            for (size_t i = 0; i < n - 1; ++i) {
                if (xv <= points[i + 1].input) {
                    idx = i;
                    break;
                }
            }

            const double x0 = points[idx].input;
            const double y0 = points[idx].output;
            const double x1 = points[idx + 1].input;
            const double y1 = points[idx + 1].output;
            const double h = x1 - x0;
            const double t = xv;
            const double a = (x1 - t) / h;
            const double b = (t - x0) / h;
            const double val = a * y0 + b * y1 + ((a * a * a - a) * m2[idx] + (b * b * b - b) * m2[idx + 1]) * h * h / 6.0;
            lut[k] = static_cast<quint8>(std::round(std::clamp(static_cast<float>(val), 0.0f, 1.0f) * 255.0f));
        }

        return lut;
    }
};

} // namespace cc
