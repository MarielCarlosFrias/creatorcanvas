#include "SnapEngine.h"

#include "core/Document.h"
#include "core/layers/Layer.h"

#include <algorithm>
#include <cmath>

namespace cc {

SnapEngine::SnapEngine() = default;

void SnapEngine::setThreshold(double thresholdDocPx)
{
    m_threshold = std::max(0.0, thresholdDocPx);
}

SnapResult SnapEngine::computeSnap(const Layer& moving,
                                   const QRectF& currentDocRect,
                                   const QRectF& canvasRect,
                                   const QList<Layer*>& otherLayers) const
{
    Q_UNUSED(moving);  // reserved for future per-layer heuristics (M17+)
    Q_UNUSED(canvasRect);
    const auto candidates = gatherCandidates(currentDocRect, canvasRect, otherLayers);
    return pickBest(candidates, canvasRect);
}

QList<SnapEngine::Candidate>
SnapEngine::gatherCandidates(const QRectF& moving,
                              const QRectF& canvas,
                              const QList<Layer*>& others) const
{
    QList<Candidate> out;

    const double mx[3] = { moving.left(), moving.center().x(), moving.right() };
    const double my[3] = { moving.top(), moving.center().y(), moving.bottom() };

    const double cx[3] = { canvas.left(), canvas.center().x(), canvas.right() };
    const double cy[3] = { canvas.top(), canvas.center().y(), canvas.bottom() };

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            const double d = std::abs(mx[i] - cx[j]);
            if (d <= m_threshold) {
                Candidate c;
                c.distance = d;
                c.delta.setX(cx[j] - mx[i]);
                c.guide.kind = GuideLine::Kind::Vertical;
                c.guide.axisPosition = cx[j];
                c.guide.spanInDocument = canvas;
                out.push_back(c);
            }
        }
    }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            const double d = std::abs(my[i] - cy[j]);
            if (d <= m_threshold) {
                Candidate c;
                c.distance = d;
                c.delta.setY(cy[j] - my[i]);
                c.guide.kind = GuideLine::Kind::Horizontal;
                c.guide.axisPosition = cy[j];
                c.guide.spanInDocument = canvas;
                out.push_back(c);
            }
        }
    }

    for (Layer* other : others) {
        if (!other || !other->visible || other->locked)
            continue;

        const QRectF local = other->contentBounds();
        if (local.isEmpty())
            continue;
        const QRectF doc = other->transform.matrix(local).mapRect(local);

        const double ox[3] = { doc.left(), doc.center().x(), doc.right() };
        const double oy[3] = { doc.top(), doc.center().y(), doc.bottom() };

        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                const double d = std::abs(mx[i] - ox[j]);
                if (d <= m_threshold) {
                    Candidate c;
                    c.distance = d + 1.0;
                    c.delta.setX(ox[j] - mx[i]);
                    c.guide.kind = GuideLine::Kind::Vertical;
                    c.guide.axisPosition = ox[j];
                    c.guide.spanInDocument = QRectF(
                        QPointF(ox[j], std::min(moving.top(), doc.top())),
                        QPointF(ox[j], std::max(moving.bottom(), doc.bottom())));
                    out.push_back(c);
                }
            }
        }
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                const double d = std::abs(my[i] - oy[j]);
                if (d <= m_threshold) {
                    Candidate c;
                    c.distance = d + 1.0;
                    c.delta.setY(oy[j] - my[i]);
                    c.guide.kind = GuideLine::Kind::Horizontal;
                    c.guide.axisPosition = oy[j];
                    c.guide.spanInDocument = QRectF(
                        QPointF(std::min(moving.left(), doc.left()), oy[j]),
                        QPointF(std::max(moving.right(), doc.right()), oy[j]));
                    out.push_back(c);
                }
            }
        }
    }

    return out;
}

SnapResult SnapEngine::pickBest(const QList<Candidate>& candidates,
                                 const QRectF& canvasRect) const
{
    Q_UNUSED(canvasRect);
    SnapResult result;

    const Candidate* bestV = nullptr;
    const Candidate* bestH = nullptr;

    for (const Candidate& c : candidates) {
        if (c.guide.isVertical()) {
            if (!bestV || c.distance < bestV->distance)
                bestV = &c;
        } else {
            if (!bestH || c.distance < bestH->distance)
                bestH = &c;
        }
    }

    if (bestV) {
        result.delta.setX(bestV->delta.x());
        result.guides.push_back(bestV->guide);
    }
    if (bestH) {
        result.delta.setY(bestH->delta.y());
        result.guides.push_back(bestH->guide);
    }

    return result;
}

} // namespace cc
