#pragma once

#include <QList>
#include <QPointF>
#include <QRectF>

namespace cc {

class Layer;
class Document;

/// A single alignment guide shown on top of the canvas during gestures.
///
/// Coordinate space is the SAME as the document (not screen). The caller
/// is responsible for mapping to device coordinates before drawing.
struct GuideLine
{
    enum class Kind { Vertical, Horizontal };
    Kind kind = Kind::Vertical;

    /// Vertical: the X coordinate. Horizontal: the Y coordinate.
    /// (Other axis is the full canvas extent so the renderer can draw
    /// a line across the whole viewport.)
    double axisPosition = 0.0;
    QRectF spanInDocument;

    bool isVertical() const { return kind == Kind::Vertical; }
};

/// Result of a snap query: how to nudge the layer and which guides to draw.
struct SnapResult
{
    QPointF delta = {0.0, 0.0};
    QList<GuideLine> guides;
};

/// Pure-geometry alignment engine. Stateless: each query is independent.
///
/// "Snap" = magnetic pull when a layer's edge or center is within
/// |thresholdDocPx| of a snap target (canvas edge/center, or another
/// visible layer's edge/center). Returns the delta to apply to the
/// layer's position and up to two guide lines (one H + one V) to
/// visualize the alignment.
class SnapEngine
{
public:
    SnapEngine();

    void setThreshold(double thresholdDocPx);
    double threshold() const { return m_threshold; }

    /// |moving|             the layer the user is currently manipulating
    /// |currentDocRect|     its rect in document coordinates (already with
    ///                      current transform applied — caller computes this)
    /// |canvasRect|         the document canvas rect (0,0,w,h)
    /// |otherLayers|        all OTHER layers visible/unlocked (the engine
    ///                      skips the moving one automatically by id)
    SnapResult computeSnap(const Layer& moving,
                           const QRectF& currentDocRect,
                           const QRectF& canvasRect,
                           const QList<Layer*>& otherLayers) const;

private:
    struct Candidate
    {
        double distance = 0.0;
        QPointF delta = {0.0, 0.0};
        GuideLine guide;
    };

    QList<Candidate> gatherCandidates(const QRectF& moving,
                                      const QRectF& canvas,
                                      const QList<Layer*>& others) const;

    SnapResult pickBest(const QList<Candidate>& candidates,
                        const QRectF& canvasRect) const;

    double m_threshold = 6.0;
};

} // namespace cc
