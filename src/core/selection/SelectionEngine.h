#pragma once

#include <QImage>
#include <QRect>
#include <vector>

namespace cc {

/// How a new selection combines with the current one (Photoshop-style).
enum class SelectionMode {
    Replace,   // New selection replaces the old one
    Add,       // Union with current selection
    Subtract,  // Remove new selection from current
    Intersect  // Keep only intersection
};

/// A selected region: 8-bit coverage over a bounding box (row-major).
/// Values are 0-255 where 0 = not selected, 255 = fully selected.
struct SelectionRegion {
    QRect bbox;          // Bounding box of selected pixels (incl. anti-aliased fringe)
    QImage mask;         // Format_Alpha8, same size as bbox, coverage 0-255

    /// Coverage (0.0-1.0) at document pixel (x, y); zero outside the box.
    float at(int x, int y) const;
};

/// Engine for selection operations, ported from Photocraft's algo/selection.rs.
/// All operations work on 8-bit coverage masks (GRAY8) and support
/// anti-aliased edges.
class SelectionEngine final
{
public:
    SelectionEngine() = default;

    // ===== Magic Wand =====
    /// Flood-fill from a seed pixel, selecting pixels similar to the seed
    /// (per-channel difference <= tolerance, alpha included).
    /// If contiguous, only the 4-connected region around the seed is selected.
    /// If antiAlias, edges are softened with a 3x3 average.
    static QImage magicWand(const QImage& image, const QRect& area,
                            int seedX, int seedY,
                            float tolerance, bool contiguous, bool antiAlias);

    // ===== Color Range =====
    /// Select pixels whose color distance to `target` (per-channel) <= fuzziness.
    /// Only pixels with alpha > 0 are considered.
    static QImage colorRange(const QImage& image, const QRect& area,
                             const QColor& target, float fuzziness);

    /// Sampled Colors: multiple samples with optional localized clusters.
    /// Each sample takes its best match; localized samples fade with distance
    /// from where they were picked, reaching 0 at `localizedRadius` pixels.
    static QImage colorRangeSamples(const QImage& image, const QRect& area,
                                    const std::vector<QColor>& targets,
                                    float fuzziness,
                                    const std::vector<QPoint>& samplePoints = {},
                                    float localizedRadius = 0.0f);

    // ===== Tonal Ranges =====
    /// Shadows: 0 = black, fading over `feather` levels.
    static QImage tonalShadows(const QImage& image, const QRect& area,
                               float shadowLimit, float feather);
    /// Midtones: between shadowLimit and highlightLimit.
    static QImage tonalMidtones(const QImage& image, const QRect& area,
                                float shadowLimit, float highlightLimit,
                                float feather);
    /// Highlights: highlightLimit = white, fading over `feather` levels.
    static QImage tonalHighlights(const QImage& image, const QRect& area,
                                  float highlightLimit, float feather);

    // ===== Morphological Operations =====
    /// Dilate selection by `radius` pixels (expand).
    static QImage expand(const QImage& selection, float radius);
    /// Erode selection by `radius` pixels (contract).
    static QImage contract(const QImage& selection, float radius);
    /// Border: pixels within `radius` of the selection edge.
    static QImage border(const QImage& selection, float radius);
    /// Smooth: remove specks and round corners (box average).
    static QImage smooth(const QImage& selection, float radius);
    /// Feather: Gaussian blur of mask edges with sigma = radius / 2.
    static QImage feather(const QImage& selection, float radius);

    // ===== Combining Selections =====
    /// Combine `newMask` with `oldMask` using `mode`.
    /// Returns empty QImage when nothing is selected.
    static QImage combine(const QImage& oldMask, const QImage& newMask,
                          const QRect& area, SelectionMode mode);

    // ===== Polygon Selection =====
    /// Anti-aliased polygon coverage (even-odd rule) over `area`.
    /// `points` are in document coordinates.
    static QImage polygon(const std::vector<QPointF>& points,
                          const QRect& area, bool antiAlias);

    // ===== AI subject selection =====
    /// Converte uma máscara de silhueta (ex.: a máscara bruta/refinada da
    /// rede neural de remoção de fundo) numa seleção de píxeis Alpha8 do
    /// tamanho do layer (`targetSize`), pronta para
    /// Document::setPixelSelection().
    /// Aceita a máscara em qualquer formato: Alpha8/Grayscale8 são usados
    /// diretamente; imagens com canal alfa têm o alfa extraído; restantes
    /// são convertidas por luminância. Máscaras com tamanho diferente de
    /// `targetSize` são redimensionadas com interpolação suave para não
    /// degradar a borda do sujeito.
    /// Retorna uma QImage nula se a máscara for nula ou o tamanho inválido.
    static QImage fromAlphaMask(const QImage& alphaMask, const QSize& targetSize);

    /// Como acima, mas combina o resultado com a seleção atual usando
    /// `mode` (Replace/Add/Subtract/Intersect). Se a seleção atual for
    /// nula ou tiver tamanho diferente de `targetSize`, o resultado é a
    /// nova máscara sozinha (não há base significativa para combinar).
    static QImage fromAlphaMask(const QImage& alphaMask, const QSize& targetSize,
                                const QImage& currentSelection, SelectionMode mode);

    // ===== Helpers =====
    /// Normalize a selection mask to Format_Alpha8 with values clamped 0-255.
    static QImage normalize(const QImage& mask);

    /// Check if a selection mask is empty (all zeros).
    static bool isEmpty(const QImage& mask);

    /// Get the bounding box of non-zero pixels in a mask.
    static QRect boundingBox(const QImage& mask);
};

} // namespace cc
