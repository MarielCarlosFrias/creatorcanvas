#include "core/operations/LayerOperations.h"

#include <QPainter>
#include <algorithm>

#include "core/Document.h"
#include "core/layers/Layer.h"
#include "core/history/CommandStack.h"
#include "core/commands/LayerStructureCommands.h"
#include "core/commands/LayerTransformCommands.h"
#include "core/commands/RasterCommands.h"
#include "rendering/CanvasRenderer.h"

namespace cc {
namespace LayerOperations {

bool areMergeableSiblings(const Document& document, const QList<LayerId>& ids)
{
    if (ids.size() < 2)
        return false;
    const GroupLayer* parent = document.parentOf(ids.first());
    if (!parent)
        return false;
    for (const LayerId& id : ids) {
        const Layer* layer = document.findLayer(id);
        if (!layer || layer->type() == LayerType::Background || document.parentOf(id) != parent)
            return false;
    }
    return true;
}

std::unique_ptr<ImageLayer> rasterLayerFor(Document& document,
                                           const QList<LayerId>& layerIds,
                                           const QString& name)
{
    QImage raster(document.width(), document.height(), QImage::Format_ARGB32_Premultiplied);
    raster.fill(Qt::transparent);
    {
        QPainter painter(&raster);
        RenderOptions options;
        options.drawCheckerboard = false;
        options.canvasBorder = Qt::transparent;
        renderSiblingLayers(document, layerIds, &painter, options);
    }

    const LayerId assetId = document.assets().addImage(raster);
    if (assetId.isNull())
        return {};

    auto layer = std::make_unique<ImageLayer>();
    layer->name = name;
    layer->assetId = assetId;
    layer->naturalWidth = document.width();
    layer->naturalHeight = document.height();
    layer->transform.position = QPointF(document.width() / 2.0, document.height() / 2.0);
    return layer;
}

LayerId mergeSelected(Document& document,
                      CommandStack* history,
                      QList<LayerId> selectedIds,
                      const QString& defaultName)
{
    QList<LayerId> ids = selectedIds;
    if (ids.size() == 1) {
        // Merge Down: merge with the layer directly below it
        const LayerId id = ids.first();
        GroupLayer* parent = document.parentOf(id);
        if (parent) {
            const int idx = document.indexOf(id);
            if (idx > 0 && idx < static_cast<int>(parent->children.size())) {
                const Layer* below = parent->children[static_cast<std::size_t>(idx - 1)].get();
                if (below && below->type() != LayerType::Background) {
                    ids = {below->id(), id}; // in bottom-to-top order
                }
            }
        }
    }

    if (!areMergeableSiblings(document, ids))
        return {};

    auto raster = rasterLayerFor(document, ids, defaultName);
    if (!raster)
        return {};

    const LayerId mergedId = raster->id();
    auto cmd = std::make_unique<ReplaceLayersWithRasterCommand>(
        document, ids, std::move(raster), QStringLiteral("layer.mergeSelected"));
    if (history)
        history->execute(std::move(cmd));
    else
        cmd->redo();

    return mergedId;
}

LayerId mergeVisible(Document& document,
                     CommandStack* history,
                     const QString& defaultName)
{
    QList<LayerId> ids;
    for (const auto& child : document.rootGroup()->children) {
        // Camadas bloqueadas não participam — ficam intactas por cima
        if (child->visible && !child->locked
            && child->type() != LayerType::Background)
            ids.append(child->id());
    }
    if (ids.size() < 2)
        return {};

    auto raster = rasterLayerFor(document, ids, defaultName);
    if (!raster)
        return {};

    const LayerId mergedId = raster->id();
    auto cmd = std::make_unique<ReplaceLayersWithRasterCommand>(
        document, ids, std::move(raster), QStringLiteral("layer.mergeVisible"));
    if (history)
        history->execute(std::move(cmd));
    else
        cmd->redo();

    return mergedId;
}

LayerId flatten(Document& document,
                CommandStack* history,
                const QString& defaultName)
{
    QList<LayerId> ids;       // desbloqueadas — serão achatadas na imagem
    QList<Layer*> kept;       // bloqueadas — preservadas no lugar
    for (const auto& child : document.rootGroup()->children) {
        if (child->locked)
            kept.append(child.get());
        else
            ids.append(child->id());
    }
    if (ids.isEmpty())
        return {};

    // Rasterizar SOMENTE as camadas desbloqueadas: esconde as bloqueadas
    // durante o render (o renderer lê `visible` direto) e devolve o estado
    // EXATO de antes — antes, uma camada travada que estava oculta era
    // reexibida involuntariamente após o flatten.
    QVector<QPair<Layer*, bool>> hiddenForRender;
    hiddenForRender.reserve(kept.size());
    for (Layer* lockedLayer : kept) {
        if (lockedLayer->visible) {
            lockedLayer->visible = false;
            hiddenForRender.append({lockedLayer, true});
        }
    }
    QImage raster(document.width(), document.height(), QImage::Format_ARGB32_Premultiplied);
    raster.fill(Qt::transparent);
    {
        QPainter painter(&raster);
        RenderOptions options;
        options.drawCheckerboard = false;
        options.canvasBorder = Qt::transparent;
        renderDocument(document, &painter, QTransform(), options);
    }
    for (const auto& [layer, wasVisible] : hiddenForRender)
        layer->visible = wasVisible;

    const LayerId assetId = document.assets().addImage(raster);
    if (assetId.isNull())
        return {};

    auto flat = std::make_unique<ImageLayer>();
    flat->name = defaultName;
    flat->assetId = assetId;
    flat->naturalWidth = document.width();
    flat->naturalHeight = document.height();
    flat->transform.position = QPointF(document.width() / 2.0, document.height() / 2.0);
    const LayerId flatId = flat->id();

    auto cmd = std::make_unique<ReplaceLayersWithRasterCommand>(
        document, ids, std::move(flat), QStringLiteral("layer.flatten"));
    if (history)
        history->execute(std::move(cmd));
    else
        cmd->redo();

    return flatId;
}

bool textBehindSubject(Document& document,
                       CommandStack* history,
                       const LayerId& textId,
                       const LayerId& subjectId)
{
    if (textId.isNull() || subjectId.isNull()
        || document.parentOf(textId) != document.parentOf(subjectId))
        return false;

    const Layer* textLayer = document.findLayer(textId);
    const Layer* subjLayer = document.findLayer(subjectId);
    if (!textLayer || !subjLayer)
        return false;
    if (textLayer->type() != LayerType::Text || subjLayer->type() != LayerType::Image)
        return false;

    auto cmd = std::make_unique<TextBehindSubjectCommand>(document, textId, subjectId);
    if (history)
        history->execute(std::move(cmd));
    else
        cmd->redo();

    return true;
}

LayerId groupSelected(Document& document,
                      CommandStack* history,
                      const QList<LayerId>& rawIds,
                      const QString& groupName)
{
    if (rawIds.size() < 2)
        return {};

    // Filter out background layer and layers whose ancestors are already selected
    QList<LayerId> ids;
    for (const auto& id : rawIds) {
        Layer* layer = document.findLayer(id);
        if (!layer || layer->type() == LayerType::Background)
            continue;

        bool hasSelectedAncestor = false;
        GroupLayer* p = document.parentOf(id);
        while (p && p != document.rootGroup()) {
            if (rawIds.contains(p->id())) {
                hasSelectedAncestor = true;
                break;
            }
            p = document.parentOf(p->id());
        }
        if (!hasSelectedAncestor && !ids.contains(id)) {
            ids.append(id);
        }
    }

    if (ids.size() < 2)
        return {};

    if (history) {
        auto cmd = std::make_unique<GroupLayersCommand>(document, ids, groupName);
        const LayerId groupId = cmd->groupId();
        history->execute(std::move(cmd));
        return groupId;
    } else {
        auto group = std::make_unique<GroupLayer>();
        group->name = groupName;
        const LayerId groupId = group->id();

        for (const auto& lid : ids) {
            if (auto taken = document.takeLayer(lid))
                group->children.push_back(std::move(taken));
        }

        document.addLayer(std::move(group));
        return groupId;
    }
}

LayerId ungroup(Document& document,
                CommandStack* history,
                const LayerId& groupId)
{
    Layer* layer = document.findLayer(groupId);
    if (!layer || layer->type() != LayerType::Group)
        return {};

    auto* group = static_cast<GroupLayer*>(layer);
    if (group->children.empty())
        return {};

    const LayerId nextSelection = group->children.back()->id();
    if (history) {
        history->execute(std::make_unique<UngroupLayersCommand>(document, groupId));
    } else {
        GroupLayer* parent = document.parentOf(groupId);
        const int insertion = document.indexOf(groupId);
        auto holder = document.takeLayer(groupId);
        auto* heldGroup = static_cast<GroupLayer*>(holder.get());
        int index = insertion;
        while (!heldGroup->children.empty()) {
            auto child = std::move(heldGroup->children.front());
            heldGroup->children.erase(heldGroup->children.begin());
            document.addLayer(std::move(child), parent, index++);
        }
    }
    return nextSelection;
}

bool flip(Document& document,
          CommandStack* history,
          const LayerId& id,
          bool horizontal)
{
    if (id.isNull())
        return false;
    const auto locks = document.effectiveLocks(id);
    if (locks.position || locks.all)
        return false;
    Layer* layer = document.findLayer(id);
    if (!layer)
        return false;

    const AffineTransform oldT = layer->transform;
    AffineTransform newT = oldT;
    if (horizontal)
        newT.scaleX = -newT.scaleX;
    else
        newT.scaleY = -newT.scaleY;

    auto cmd = std::make_unique<SetLayerTransformCommand>(document, id, oldT, newT);
    if (history)
        history->execute(std::move(cmd));
    else
        cmd->redo();

    return true;
}

bool rotate(Document& document,
            CommandStack* history,
            const LayerId& id,
            double degrees)
{
    if (id.isNull())
        return false;
    const auto locks = document.effectiveLocks(id);
    if (locks.position || locks.all)
        return false;
    Layer* layer = document.findLayer(id);
    if (!layer)
        return false;

    const AffineTransform oldT = layer->transform;
    AffineTransform newT = oldT;
    newT.rotationDeg += degrees;

    auto cmd = std::make_unique<SetLayerTransformCommand>(document, id, oldT, newT);
    if (history)
        history->execute(std::move(cmd));
    else
        cmd->redo();

    return true;
}

bool deleteLayer(Document& document,
                 CommandStack* history,
                 const LayerId& id)
{
    if (id.isNull())
        return false;
    if (document.effectiveLocks(id).all)
        return false;
    if (!document.findLayer(id))
        return false;

    auto cmd = std::make_unique<RemoveLayerCommand>(document, id);
    if (history)
        history->execute(std::move(cmd));
    else
        cmd->redo();

    return true;
}

} // namespace LayerOperations
} // namespace cc
