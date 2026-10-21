#include "SelectionEngine.h"

#include <QColor>
#include <QPolygonF>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cc {

namespace {

// ===== Helpers internos =====

/// Stride de uma QImage ARGB32 em pixels (bytesPerLine pode ter padding).
inline int argbStride(const QImage& img)
{
    return img.bytesPerLine() / 4;
}

/// Acessa o pixel ARGB em (x, y) respeitando o stride da imagem.
inline QRgb argbAt(const QImage& img, int stride, int x, int y)
{
    return reinterpret_cast<const QRgb*>(img.constBits())[y * stride + x];
}

/// Luminância aproximada de um pixel (0-255).
inline int luminance(int r, int g, int b)
{
    return (r * 299 + g * 587 + b * 114) / 1000;
}

/// Distância máxima por canal entre dois pixels ARGB (0-255).
inline int channelDistance(QRgb a, QRgb b)
{
    return std::max({std::abs(qRed(a) - qRed(b)),
                     std::abs(qGreen(a) - qGreen(b)),
                     std::abs(qBlue(a) - qBlue(b)),
                     std::abs(qAlpha(a) - qAlpha(b))});
}

/// Cobertura quantizada para 8 bits (0-255), NaN -> 0.
inline uchar coverage8(float v)
{
    if (std::isnan(v))
        return 0;
    return static_cast<uchar>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
}

/// Converte máscara de cobertura float (0..1) para QImage Alpha8.
QImage coverageToAlpha8(const std::vector<float>& mask, int w, int h)
{
    if (w <= 0 || h <= 0 || static_cast<int>(mask.size()) < w * h)
        return {};
    QImage out(w, h, QImage::Format_Alpha8);
    for (int y = 0; y < h; ++y) {
        uchar* row = out.scanLine(y);
        const int base = y * w;
        for (int x = 0; x < w; ++x)
            row[x] = coverage8(mask[base + x]);
    }
    return out;
}

/// Anti-aliasing da Photocraft: pixels de borda viram a média 3x3.
/// Mantém pixels selecionados na maior parte selecionados (fringe suave).
void antialias(std::vector<float>& m, int w, int h)
{
    const std::vector<float> src = m;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int i = y * w + x;
            float sum = 0.0f;
            int cnt = 0;
            bool edge = false;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int xx = x + dx;
                    const int yy = y + dy;
                    const float v = (xx < 0 || yy < 0 || xx >= w || yy >= h)
                                        ? src[i]
                                        : src[yy * w + xx];
                    edge |= (v != src[i]);
                    sum += v;
                    ++cnt;
                }
            }
            if (edge) {
                const float avg = sum / float(cnt);
                m[i] = src[i] > 0.5f ? 0.5f + 0.5f * avg : 0.5f * avg;
            }
        }
    }
}

/// Blur de caixa separável sobre float (raio em pixels), bordas replicadas.
void boxBlur(std::vector<float>& m, int w, int h, int r)
{
    if (r <= 0 || w <= 0 || h <= 0)
        return;
    r = std::min(r, std::max(w, h));

    std::vector<float> tmp(m.size());

    // Pass horizontal (janela deslizante, bordas replicadas)
    for (int y = 0; y < h; ++y) {
        const int row = y * w;
        float sum = 0.0f;
        for (int x = -r; x <= r; ++x)
            sum += m[row + std::clamp(x, 0, w - 1)];
        const float inv = 1.0f / float(2 * r + 1);
        for (int x = 0; x < w; ++x) {
            tmp[row + x] = sum * inv;
            sum -= m[row + std::clamp(x - r, 0, w - 1)];
            sum += m[row + std::clamp(x + r + 1, 0, w - 1)];
        }
    }

    // Pass vertical
    for (int x = 0; x < w; ++x) {
        float sum = 0.0f;
        for (int y = -r; y <= r; ++y)
            sum += tmp[std::clamp(y, 0, h - 1) * w + x];
        const float inv = 1.0f / float(2 * r + 1);
        for (int y = 0; y < h; ++y) {
            m[y * w + x] = sum * inv;
            sum -= tmp[std::clamp(y - r, 0, h - 1) * w + x];
            sum += tmp[std::clamp(y + r + 1, 0, h - 1) * w + x];
        }
    }
}

/// Filtro 1D de máximo/mínimo com janela [i-r, i+r] em O(n) independente do
/// raio (estratégia van Herk / Gil-Werman). Bloco de tamanho b = 2r+1 (a
/// própria largura da janela): blocos adjacentes cobrem a janela EXATAMENTE
/// via suf[lo] ∪ pref[hi]; janela clamped dentro de um bloco é recomputada
/// direto (só acontece nas bordas do array).
/// direction > 0 = máximo (dilata), < 0 = mínimo (erode).
void vanHerkPass(const float* src, float* dst, int n, int r, int direction)
{
    if (n <= 0)
        return;
    r = std::min(r, std::max(1, n - 1));
    const int b = 2 * r + 1; // largura da janela = tamanho do bloco
    const int blocks = (n + b - 1) / b;

    // pref[i] = max/min do início do bloco de i até i.
    // suf[i] = max/min de i até o fim do bloco de i.
    std::vector<float> pref(n), suf(n);
    for (int blk = 0; blk < blocks; ++blk) {
        const int start = blk * b;
        const int end = std::min(start + b, n);
        if (start >= end)
            break;
        float acc = src[start];
        pref[start] = acc;
        for (int i = start + 1; i < end; ++i) {
            acc = direction > 0 ? std::max(acc, src[i]) : std::min(acc, src[i]);
            pref[i] = acc;
        }
        acc = src[end - 1];
        suf[end - 1] = acc;
        for (int i = end - 2; i >= start; --i) {
            acc = direction > 0 ? std::max(acc, src[i]) : std::min(acc, src[i]);
            suf[i] = acc;
        }
    }

    for (int i = 0; i < n; ++i) {
        const int lo = std::max(0, i - r);
        const int hi = std::min(n - 1, i + r);
        if (lo / b == hi / b) {
            // Janela clamped dentro de um bloco (bordas do array): direto.
            float acc = src[lo];
            for (int j = lo + 1; j <= hi; ++j)
                acc = direction > 0 ? std::max(acc, src[j]) : std::min(acc, src[j]);
            dst[i] = acc;
        } else {
            // Blocos adjacentes: suf[lo] cobre [lo, fim_bloco(lo)] e
            // pref[hi] cobre [início_bloco(hi), hi]; a união é a janela
            // exata porque fim_bloco(lo)+1 == início_bloco(hi).
            dst[i] = direction > 0 ? std::max(suf[lo], pref[hi])
                                    : std::min(suf[lo], pref[hi]);
        }
    }
}

/// Dilatação (direction > 0) / erosão (direction < 0) morfológica separável:
/// uma passada horizontal + uma vertical, cada uma O(w·h) — o elemento
/// estrutural é um quadrado (distância Chebyshev), como o do Photocraft.
/// A versão anterior era O(w·h·r²) e congelava o app por minutos com
/// raios grandes (raio 200 numa seleção 1920×1080 ≈ 320 bilhões de
/// operações), fazendo o gerenciador de janelas escurecer a janela.
void morphology(std::vector<float>& m, int w, int h, float radius, int direction)
{
    if (radius <= 0.0f || w <= 0 || h <= 0)
        return;
    const int r = std::max(1, static_cast<int>(std::round(radius)));

    // Passada horizontal.
    std::vector<float> tmp(m.size());
    for (int y = 0; y < h; ++y)
        vanHerkPass(m.data() + y * w, tmp.data() + y * w, w, r, direction);

    // Passada vertical (colunas como arrays contíguos).
    std::vector<float> col(h), colOut(h);
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y)
            col[y] = tmp[y * w + x];
        vanHerkPass(col.data(), colOut.data(), h, r, direction);
        for (int y = 0; y < h; ++y)
            m[y * w + x] = colOut[y];
    }
}

} // namespace

// ===== SelectionRegion =====

float SelectionRegion::at(int x, int y) const
{
    if (!bbox.contains(x, y) || mask.isNull())
        return 0.0f;
    const int lx = x - bbox.x();
    const int ly = y - bbox.y();
    if (lx < 0 || ly < 0 || lx >= mask.width() || ly >= mask.height())
        return 0.0f;
    return mask.constScanLine(ly)[lx] / 255.0f;
}

// ===== Magic Wand =====

QImage SelectionEngine::magicWand(const QImage& image, const QRect& area,
                                  int seedX, int seedY,
                                  float tolerance, bool contiguous, bool antiAlias)
{
    if (image.isNull() || area.isEmpty())
        return {};
    if (!area.contains(seedX, seedY))
        return {};

    const QImage src = image.convertToFormat(QImage::Format_ARGB32);
    const int w = area.width();
    const int h = area.height();
    std::vector<float> m(static_cast<std::size_t>(w) * h, 0.0f);

    const int stride = argbStride(src);
    const QRgb target = argbAt(src, stride, seedX, seedY);
    const float tol = std::max(0.0f, tolerance);

    auto similar = [&](int idx) -> bool {
        return channelDistance(
                   argbAt(src, stride, area.x() + idx % w, area.y() + idx / w),
                   target) <= tol + 1e-3f;
    };

    if (contiguous) {
        // Flood fill por pilha (equivalente ao span fill do Photocraft).
        const int si = (seedY - area.y()) * w + (seedX - area.x());
        std::vector<int> stack;
        stack.reserve(static_cast<std::size_t>(w) * h);
        stack.push_back(si);
        m[si] = 1.0f;
        while (!stack.empty()) {
            const int i = stack.back();
            stack.pop_back();
            const int x = i % w;
            const int y = i / w;
            auto visit = [&](int j) {
                if (m[j] == 0.0f && similar(j)) {
                    m[j] = 1.0f;
                    stack.push_back(j);
                }
            };
            if (x > 0)        visit(i - 1);
            if (x + 1 < w)    visit(i + 1);
            if (y > 0)        visit(i - w);
            if (y + 1 < h)    visit(i + w);
        }
    } else {
        for (int i = 0; i < w * h; ++i)
            m[i] = similar(i) ? 1.0f : 0.0f;
    }

    if (antiAlias)
        antialias(m, w, h);

    return coverageToAlpha8(m, w, h);
}

// ===== Color Range =====

QImage SelectionEngine::colorRange(const QImage& image, const QRect& area,
                                   const QColor& target, float fuzziness)
{
    return colorRangeSamples(image, area, {target}, fuzziness);
}

QImage SelectionEngine::colorRangeSamples(const QImage& image, const QRect& area,
                                          const std::vector<QColor>& targets,
                                          float fuzziness,
                                          const std::vector<QPoint>& samplePoints,
                                          float localizedRadius)
{
    if (image.isNull() || area.isEmpty() || targets.empty())
        return {};

    const QImage src = image.convertToFormat(QImage::Format_ARGB32);
    const int w = area.width();
    const int h = area.height();
    const float f = std::max(0.0f, fuzziness) + 1.0f;
    const bool localized = localizedRadius > 0.0f && samplePoints.size() == targets.size();

    std::vector<float> m(static_cast<std::size_t>(w) * h);
    const int stride = argbStride(src);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int i = y * w + x;
            const QRgb p = argbAt(src, stride, area.x() + x, area.y() + y);
            const float a = qAlpha(p) / 255.0f;
            if (a <= 0.0f) {
                m[i] = 0.0f;
                continue;
            }
            const float pr = qRed(p) / 255.0f;
            const float pg = qGreen(p) / 255.0f;
            const float pb = qBlue(p) / 255.0f;

            float best = 0.0f;
            for (std::size_t s = 0; s < targets.size(); ++s) {
                const QColor& c = targets[s];
                const float dr = std::abs(pr - c.redF()) * 255.0f;
                const float dg = std::abs(pg - c.greenF()) * 255.0f;
                const float db = std::abs(pb - c.blueF()) * 255.0f;
                float k = 1.0f - std::max({dr, dg, db}) / f;
                k = std::clamp(k, 0.0f, 1.0f);

                if (localized) {
                    const QPoint& sp = samplePoints[s];
                    const float dx = float(x - (sp.x() - area.x()));
                    const float dy = float(y - (sp.y() - area.y()));
                    const float dist = std::sqrt(dx * dx + dy * dy);
                    k *= std::clamp(1.0f - dist / localizedRadius, 0.0f, 1.0f);
                }
                best = std::max(best, k);
            }
            m[i] = best;
        }
    }
    return coverageToAlpha8(m, w, h);
}

// ===== Tonal Ranges =====

QImage SelectionEngine::tonalShadows(const QImage& image, const QRect& area,
                                    float shadowLimit, float feather)
{
    if (image.isNull() || area.isEmpty())
        return {};
    const QImage src = image.convertToFormat(QImage::Format_ARGB32);
    const int w = area.width();
    const int h = area.height();
    const float limit = std::clamp(shadowLimit, 0.0f, 255.0f);
    const float fz = std::max(1.0f, feather);
    std::vector<float> m(static_cast<std::size_t>(w) * h);

    const int stride = argbStride(src);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const QRgb p = argbAt(src, stride, area.x() + x, area.y() + y);
            if (qAlpha(p) <= 0) {
                m[y * w + x] = 0.0f;
                continue;
            }
            const float lum = float(luminance(qRed(p), qGreen(p), qBlue(p)));
            m[y * w + x] = std::clamp(1.0f - (lum - limit) / fz, 0.0f, 1.0f);
        }
    }
    return coverageToAlpha8(m, w, h);
}

QImage SelectionEngine::tonalMidtones(const QImage& image, const QRect& area,
                                      float shadowLimit, float highlightLimit,
                                      float feather)
{
    if (image.isNull() || area.isEmpty())
        return {};
    const QImage src = image.convertToFormat(QImage::Format_ARGB32);
    const int w = area.width();
    const int h = area.height();
    const float lo = std::clamp(shadowLimit, 0.0f, 255.0f);
    const float hi = std::clamp(highlightLimit, 0.0f, 255.0f);
    const float fz = std::max(1.0f, feather);
    std::vector<float> m(static_cast<std::size_t>(w) * h);

    const int stride = argbStride(src);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const QRgb p = argbAt(src, stride, area.x() + x, area.y() + y);
            if (qAlpha(p) <= 0) {
                m[y * w + x] = 0.0f;
                continue;
            }
            const float lum = float(luminance(qRed(p), qGreen(p), qBlue(p)));
            float k = 1.0f;
            if (lum < lo)
                k = std::clamp((lum - (lo - fz)) / fz, 0.0f, 1.0f);
            else if (lum > hi)
                k = std::clamp(((hi + fz) - lum) / fz, 0.0f, 1.0f);
            m[y * w + x] = k;
        }
    }
    return coverageToAlpha8(m, w, h);
}

QImage SelectionEngine::tonalHighlights(const QImage& image, const QRect& area,
                                        float highlightLimit, float feather)
{
    if (image.isNull() || area.isEmpty())
        return {};
    const QImage src = image.convertToFormat(QImage::Format_ARGB32);
    const int w = area.width();
    const int h = area.height();
    const float limit = std::clamp(highlightLimit, 0.0f, 255.0f);
    const float fz = std::max(1.0f, feather);
    std::vector<float> m(static_cast<std::size_t>(w) * h);

    const int stride = argbStride(src);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const QRgb p = argbAt(src, stride, area.x() + x, area.y() + y);
            if (qAlpha(p) <= 0) {
                m[y * w + x] = 0.0f;
                continue;
            }
            const float lum = float(luminance(qRed(p), qGreen(p), qBlue(p)));
            m[y * w + x] = std::clamp((lum - limit) / fz, 0.0f, 1.0f);
        }
    }
    return coverageToAlpha8(m, w, h);
}

// ===== Morphological Operations =====

QImage SelectionEngine::expand(const QImage& selection, float radius)
{
    if (selection.isNull() || radius <= 0.0f)
        return selection;
    const QImage mask = normalize(selection);
    const int w = mask.width();
    const int h = mask.height();
    std::vector<float> m(static_cast<std::size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            m[y * w + x] = mask.constScanLine(y)[x] / 255.0f;
    morphology(m, w, h, radius, +1);
    return coverageToAlpha8(m, w, h);
}

QImage SelectionEngine::contract(const QImage& selection, float radius)
{
    if (selection.isNull() || radius <= 0.0f)
        return selection;
    const QImage mask = normalize(selection);
    const int w = mask.width();
    const int h = mask.height();
    std::vector<float> m(static_cast<std::size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            m[y * w + x] = mask.constScanLine(y)[x] / 255.0f;
    morphology(m, w, h, radius, -1);
    return coverageToAlpha8(m, w, h);
}

QImage SelectionEngine::border(const QImage& selection, float radius)
{
    if (selection.isNull() || radius <= 0.0f)
        return selection;
    const QImage mask = normalize(selection);
    const int w = mask.width();
    const int h = mask.height();
    const int r = std::max(1, static_cast<int>(std::round(radius / 2.0f)));

    std::vector<float> base(static_cast<std::size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            base[y * w + x] = mask.constScanLine(y)[x] / 255.0f;

    std::vector<float> outer = base;
    morphology(outer, w, h, float(r), +1);
    std::vector<float> inner = base;
    morphology(inner, w, h, float(r), -1);

    std::vector<float> out(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = std::max(0.0f, outer[i] - inner[i]);
    return coverageToAlpha8(out, w, h);
}

QImage SelectionEngine::smooth(const QImage& selection, float radius)
{
    if (selection.isNull() || radius <= 0.0f)
        return selection;
    const QImage mask = normalize(selection);
    const int w = mask.width();
    const int h = mask.height();
    const int r = std::max(1, static_cast<int>(std::round(radius)));

    std::vector<float> m(static_cast<std::size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            m[y * w + x] = mask.constScanLine(y)[x] / 255.0f;

    boxBlur(m, w, h, r);

    // Re-threshold: acima de 0.5 -> 1, abaixo -> 0 (como o Photocraft).
    for (float& v : m)
        v = v > 0.5f ? 1.0f : 0.0f;
    return coverageToAlpha8(m, w, h);
}

QImage SelectionEngine::feather(const QImage& selection, float radius)
{
    if (selection.isNull() || radius <= 0.0f)
        return selection;
    const QImage mask = normalize(selection);
    const int w = mask.width();
    const int h = mask.height();
    // sigma = radius / 2; kernel truncado em 3 sigma -> 3 passos de box blur
    // (aproximação de Gauss, como no Photocraft).
    const int r = std::max(1, static_cast<int>(std::round(radius / 2.0f)));

    std::vector<float> m(static_cast<std::size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            m[y * w + x] = mask.constScanLine(y)[x] / 255.0f;

    // 3 iterações de box blur ~ gaussiana.
    for (int i = 0; i < 3; ++i)
        boxBlur(m, w, h, r);
    return coverageToAlpha8(m, w, h);
}

// ===== Combining Selections =====

QImage SelectionEngine::combine(const QImage& oldMask, const QImage& newMask,
                                const QRect& area, SelectionMode mode)
{
    if (area.isEmpty() || newMask.isNull())
        return {};

    const int w = area.width();
    const int h = area.height();
    const QImage cur = oldMask.convertToFormat(QImage::Format_Alpha8);
    std::vector<float> out(static_cast<std::size_t>(w) * h);

    const bool hasOld = !cur.isNull() && cur.size() == newMask.size();
    const QImage nw = newMask.convertToFormat(QImage::Format_Alpha8);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * w + x;
            const float n = nw.constScanLine(y)[x] / 255.0f;
            switch (mode) {
            case SelectionMode::Replace:
                out[i] = n;
                break;
            case SelectionMode::Add: {
                const float o = hasOld ? cur.constScanLine(y)[x] / 255.0f : 0.0f;
                out[i] = std::max(o, n);
                break;
            }
            case SelectionMode::Subtract: {
                const float o = hasOld ? cur.constScanLine(y)[x] / 255.0f : 0.0f;
                out[i] = std::max(0.0f, o - n);
                break;
            }
            case SelectionMode::Intersect: {
                const float o = hasOld ? cur.constScanLine(y)[x] / 255.0f : 0.0f;
                out[i] = std::min(o, n);
                break;
            }
            }
        }
    }

    // Tudo zero = nenhuma seleção.
    bool any = false;
    for (float v : out)
        if (v > 0.0f) { any = true; break; }
    if (!any)
        return {};

    return coverageToAlpha8(out, w, h);
}

// ===== Polygon Selection =====

QImage SelectionEngine::polygon(const std::vector<QPointF>& points,
                                 const QRect& area, bool antiAlias)
{
    if (points.size() < 3 || area.isEmpty())
        return {};

    const int w = area.width();
    const int h = area.height();
    std::vector<float> m(static_cast<std::size_t>(w) * h, 0.0f);

    // 4 sub-rows por linha de pixel quando anti-aliased (como Photocraft).
    const int sub = antiAlias ? 4 : 1;

    for (int py = 0; py < h; ++py) {
        for (int s = 0; s < sub; ++s) {
            const float y = area.y() + py + (float(s) + 0.5f) / sub;
            // Interseções scanline (regra par/ímpar).
            std::vector<float> xs;
            for (std::size_t i = 0; i < points.size(); ++i) {
                const QPointF& a = points[i];
                const QPointF& b = points[(i + 1) % points.size()];
                if ((a.y() <= y && b.y() > y) || (b.y() <= y && a.y() > y))
                    xs.push_back(a.x() + (y - a.y()) / (b.y() - a.y()) * (b.x() - a.x()));
            }
            std::sort(xs.begin(), xs.end());
            for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
                const float x0 = std::max(xs[k] - area.x(), 0.0f);
                const float x1 = std::min(xs[k + 1] - area.x(), float(w));
                if (x1 <= x0)
                    continue;
                for (int cx = int(x0); cx < int(std::ceil(x1)) && cx < w; ++cx) {
                    float cov;
                    if (antiAlias) {
                        cov = std::min(float(cx + 1), x1) - std::max(float(cx), x0);
                    } else {
                        const float c = cx + 0.5f;
                        cov = (c >= x0 && c < x1) ? 1.0f : 0.0f;
                    }
                    if (cx >= 0)
                        m[py * w + cx] += std::max(0.0f, cov) / sub;
                }
            }
        }
    }
    for (float& v : m)
        v = std::min(v, 1.0f);
    return coverageToAlpha8(m, w, h);
}

// ===== Helpers =====

QImage SelectionEngine::normalize(const QImage& mask)
{
    if (mask.isNull())
        return {};
    if (mask.format() == QImage::Format_Alpha8)
        return mask;
    return mask.convertToFormat(QImage::Format_Alpha8);
}

bool SelectionEngine::isEmpty(const QImage& mask)
{
    if (mask.isNull())
        return true;
    const QImage a = normalize(mask);
    for (int y = 0; y < a.height(); ++y) {
        const uchar* row = a.constScanLine(y);
        for (int x = 0; x < a.width(); ++x)
            if (row[x] != 0)
                return false;
    }
    return true;
}

QRect SelectionEngine::boundingBox(const QImage& mask)
{
    if (mask.isNull())
        return {};
    const QImage a = normalize(mask);
    int x0 = a.width(), y0 = a.height(), x1 = -1, y1 = -1;
    for (int y = 0; y < a.height(); ++y) {
        const uchar* row = a.constScanLine(y);
        for (int x = 0; x < a.width(); ++x) {
            if (row[x] > 8) {
                x0 = std::min(x0, x);
                y0 = std::min(y0, y);
                x1 = std::max(x1, x);
                y1 = std::max(y1, y);
            }
        }
    }
    if (x1 < 0)
        return {};
    return QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
}

} // namespace cc
