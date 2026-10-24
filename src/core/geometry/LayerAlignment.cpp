#include "LayerAlignment.h"
#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/commands/BaseCommands.h"
#include "core/commands/LayerTransformCommands.h"

#include <algorithm>
#include <cmath>

namespace cc {

bool LayerAlignment::alignLayer(Document& doc, CommandStack* history, const LayerId& id, AlignTarget target)
{
    if (id.isNull())
        return false;

    Layer* layer = doc.findLayer(id);
    if (!layer)
        return false;

    const auto locks = doc.effectiveLocks(id);
    if (locks.position || locks.all || layer->locked)
        return false;

    const QRectF local = layer->contentBounds();
    if (local.isEmpty())
        return false;

    const QRectF docBounds = layer->transform.matrix(local).mapRect(local);
    const double docW = doc.width();
    const double docH = doc.height();

    double dx = 0.0;
    double dy = 0.0;

    switch (target) {
    case AlignTarget::Left:
        dx = -docBounds.left();
        break;
    case AlignTarget::CenterX:
        dx = (docW / 2.0) - docBounds.center().x();
        break;
    case AlignTarget::Right:
        dx = docW - docBounds.right();
        break;
    case AlignTarget::Top:
        dy = -docBounds.top();
        break;
    case AlignTarget::CenterY:
        dy = (docH / 2.0) - docBounds.center().y();
        break;
    case AlignTarget::Bottom:
        dy = docH - docBounds.bottom();
        break;
    case AlignTarget::CenterBoth:
        dx = (docW / 2.0) - docBounds.center().x();
        dy = (docH / 2.0) - docBounds.center().y();
        break;
    }

    if (qFuzzyIsNull(dx) && qFuzzyIsNull(dy))
        return false;

    const AffineTransform oldT = layer->transform;
    AffineTransform newT = oldT;
    newT.position += QPointF(dx, dy);

    if (history) {
        history->execute(std::make_unique<SetLayerTransformCommand>(
            doc, id, oldT, newT));
    } else {
        doc.setLayerTransform(id, newT);
    }
    return true;
}

bool LayerAlignment::alignLayers(Document& doc, CommandStack* history, const QList<LayerId>& ids, AlignTarget target)
{
    if (ids.size() < 2)
        return false;

    struct LayerInfo {
        LayerId id;
        QRectF docBounds;
        AffineTransform oldTransform;
    };
    QVector<LayerInfo> infos;
    QRectF unionRect;

    for (const auto& lid : ids) {
        Layer* layer = doc.findLayer(lid);
        if (!layer)
            continue;
        const auto locks = doc.effectiveLocks(lid);
        if (locks.position || locks.all || layer->locked)
            continue;
        const QRectF local = layer->contentBounds();
        if (local.isEmpty())
            continue;
        const QRectF docBounds = layer->transform.matrix(local).mapRect(local);
        infos.append({lid, docBounds, layer->transform});
        unionRect = unionRect.isNull() ? docBounds : unionRect.united(docBounds);
    }

    if (infos.size() < 2 || unionRect.isEmpty())
        return false;

    auto compound = std::make_unique<CompoundCommand>(QStringLiteral("layer.align"));
    bool anyMoved = false;

    for (const auto& info : infos) {
        double dx = 0.0;
        double dy = 0.0;

        switch (target) {
        case AlignTarget::Left:
            dx = unionRect.left() - info.docBounds.left();
            break;
        case AlignTarget::CenterX:
            dx = unionRect.center().x() - info.docBounds.center().x();
            break;
        case AlignTarget::Right:
            dx = unionRect.right() - info.docBounds.right();
            break;
        case AlignTarget::Top:
            dy = unionRect.top() - info.docBounds.top();
            break;
        case AlignTarget::CenterY:
            dy = unionRect.center().y() - info.docBounds.center().y();
            break;
        case AlignTarget::Bottom:
            dy = unionRect.bottom() - info.docBounds.bottom();
            break;
        case AlignTarget::CenterBoth:
            dx = unionRect.center().x() - info.docBounds.center().x();
            dy = unionRect.center().y() - info.docBounds.center().y();
            break;
        }

        if (!qFuzzyIsNull(dx) || !qFuzzyIsNull(dy)) {
            anyMoved = true;
            AffineTransform newT = info.oldTransform;
            newT.position += QPointF(dx, dy);
            if (history) {
                compound->addCommand(std::make_unique<SetLayerTransformCommand>(
                    doc, info.id, info.oldTransform, newT));
            } else {
                doc.setLayerTransform(info.id, newT);
            }
        }
    }

    if (history && !compound->isEmpty()) {
        history->execute(std::move(compound));
    }
    return anyMoved;
}

bool LayerAlignment::distributeHorizontally(Document& doc, CommandStack* history, const QList<LayerId>& ids)
{
    if (ids.size() < 3)
        return false;

    struct Info {
        LayerId id;
        QRectF docBounds;
        AffineTransform oldTransform;
        double centerX;
    };
    QVector<Info> infos;

    for (const auto& lid : ids) {
        Layer* layer = doc.findLayer(lid);
        if (!layer)
            continue;
        const auto locks = doc.effectiveLocks(lid);
        if (locks.position || locks.all || layer->locked)
            continue;
        const QRectF local = layer->contentBounds();
        if (local.isEmpty())
            continue;
        const QRectF db = layer->transform.matrix(local).mapRect(local);
        infos.append({lid, db, layer->transform, db.center().x()});
    }

    if (infos.size() < 3)
        return false;

    std::sort(infos.begin(), infos.end(), [](const Info& a, const Info& b) {
        return a.centerX < b.centerX;
    });

    const double firstCenter = infos.first().centerX;
    const double lastCenter = infos.last().centerX;
    const double step = (lastCenter - firstCenter) / (infos.size() - 1);

    auto compound = std::make_unique<CompoundCommand>(QStringLiteral("layer.distributeH"));
    bool anyMoved = false;

    for (int i = 1; i < infos.size() - 1; ++i) {
        const double targetX = firstCenter + step * i;
        const double dx = targetX - infos[i].centerX;
        if (!qFuzzyIsNull(dx)) {
            anyMoved = true;
            AffineTransform newT = infos[i].oldTransform;
            newT.position += QPointF(dx, 0.0);
            if (history) {
                compound->addCommand(std::make_unique<SetLayerTransformCommand>(
                    doc, infos[i].id, infos[i].oldTransform, newT));
            } else {
                doc.setLayerTransform(infos[i].id, newT);
            }
        }
    }

    if (history && !compound->isEmpty()) {
        history->execute(std::move(compound));
    }
    return anyMoved;
}

bool LayerAlignment::distributeVertically(Document& doc, CommandStack* history, const QList<LayerId>& ids)
{
    if (ids.size() < 3)
        return false;

    struct Info {
        LayerId id;
        QRectF docBounds;
        AffineTransform oldTransform;
        double centerY;
    };
    QVector<Info> infos;

    for (const auto& lid : ids) {
        Layer* layer = doc.findLayer(lid);
        if (!layer)
            continue;
        const auto locks = doc.effectiveLocks(lid);
        if (locks.position || locks.all || layer->locked)
            continue;
        const QRectF local = layer->contentBounds();
        if (local.isEmpty())
            continue;
        const QRectF db = layer->transform.matrix(local).mapRect(local);
        infos.append({lid, db, layer->transform, db.center().y()});
    }

    if (infos.size() < 3)
        return false;

    std::sort(infos.begin(), infos.end(), [](const Info& a, const Info& b) {
        return a.centerY < b.centerY;
    });

    const double firstCenter = infos.first().centerY;
    const double lastCenter = infos.last().centerY;
    const double step = (lastCenter - firstCenter) / (infos.size() - 1);

    auto compound = std::make_unique<CompoundCommand>(QStringLiteral("layer.distributeV"));
    bool anyMoved = false;

    for (int i = 1; i < infos.size() - 1; ++i) {
        const double targetY = firstCenter + step * i;
        const double dy = targetY - infos[i].centerY;
        if (!qFuzzyIsNull(dy)) {
            anyMoved = true;
            AffineTransform newT = infos[i].oldTransform;
            newT.position += QPointF(0.0, dy);
            if (history) {
                compound->addCommand(std::make_unique<SetLayerTransformCommand>(
                    doc, infos[i].id, infos[i].oldTransform, newT));
            } else {
                doc.setLayerTransform(infos[i].id, newT);
            }
        }
    }

    if (history && !compound->isEmpty()) {
        history->execute(std::move(compound));
    }
    return anyMoved;
}

} // namespace cc
