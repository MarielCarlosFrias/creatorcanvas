#include "LayerClipboard.h"

#include "core/commands/LayerStructureCommands.h"
#include "core/history/CommandStack.h"

#include <QSet>
#include <functional>

namespace cc {
namespace {

void collectAssets(const Document& document, const Layer& layer,
                   QHash<LayerId, QImage>& out)
{
    if (layer.type() == LayerType::Image) {
        const auto& image = static_cast<const ImageLayer&>(layer);
        if (!image.assetId.isNull() && !out.contains(image.assetId)) {
            const QImage bitmap = document.assets().decodedImage(image.assetId);
            if (!bitmap.isNull())
                out.insert(image.assetId, bitmap);
        }
    }
    if (layer.type() == LayerType::Group) {
        for (const auto& child : static_cast<const GroupLayer&>(layer).children)
            collectAssets(document, *child, out);
    }
}

void mapLayerIds(const Layer& source, const Layer& copy,
                 QHash<LayerId, LayerId>& out)
{
    out.insert(source.id(), copy.id());
    if (source.type() == LayerType::Group) {
        const auto& srcChildren = static_cast<const GroupLayer&>(source).children;
        const auto& dstChildren = static_cast<const GroupLayer&>(copy).children;
        const size_t count = qMin(srcChildren.size(), dstChildren.size());
        for (size_t i = 0; i < count; ++i)
            mapLayerIds(*srcChildren[i], *dstChildren[i], out);
    }
}

void remapLinksAndAssets(Layer& layer, Document& target,
                         const QHash<LayerId, QImage>& assets,
                         const QHash<LayerId, LayerId>& idMap)
{
    if (layer.type() == LayerType::Image) {
        auto& image = static_cast<ImageLayer&>(layer);
        const LayerId oldAsset = image.assetId;
        if (!oldAsset.isNull()) {
            const auto it = assets.constFind(oldAsset);
            image.assetId = it == assets.constEnd()
                                ? LayerId()
                                : target.assets().addImage(it.value());
        }
        if (image.adjustmentLayerId) {
            const auto it = idMap.constFind(*image.adjustmentLayerId);
            image.adjustmentLayerId = it == idMap.constEnd()
                                          ? std::nullopt
                                          : std::optional<LayerId>(it.value());
        }
    } else if (layer.type() == LayerType::Adjustment) {
        auto& adjustment = static_cast<AdjustmentLayer&>(layer);
        if (adjustment.sourceLayerId) {
            const auto it = idMap.constFind(*adjustment.sourceLayerId);
            adjustment.sourceLayerId = it == idMap.constEnd()
                                           ? std::nullopt
                                           : std::optional<LayerId>(it.value());
        }
    } else if (layer.type() == LayerType::Text) {
        auto& text = static_cast<TextLayer&>(layer);
        if (text.pathLayerId) {
            const auto it = idMap.constFind(*text.pathLayerId);
            text.pathLayerId = it == idMap.constEnd()
                                   ? std::nullopt
                                   : std::optional<LayerId>(it.value());
        }
    }

    if (layer.type() == LayerType::Group) {
        for (auto& child : static_cast<GroupLayer&>(layer).children)
            remapLinksAndAssets(*child, target, assets, idMap);
    }
}

} // namespace

bool LayerClipboard::capture(const Document& document, const QList<LayerId>& layerIds)
{
    clear();
    const QSet<LayerId> selected(layerIds.begin(), layerIds.end());
    if (selected.isEmpty())
        return false;

    QSet<LayerId> selectedRoots;
    QSet<LayerId> includedIds;
    QSet<LayerId> includedImageIds;
    QSet<LayerId> linkedAdjustmentIds;

    std::function<void(const Layer&)> includeSubtree = [&](const Layer& layer) {
        includedIds.insert(layer.id());
        if (layer.type() == LayerType::Image) {
            const auto& image = static_cast<const ImageLayer&>(layer);
            includedImageIds.insert(layer.id());
            if (image.adjustmentLayerId)
                linkedAdjustmentIds.insert(*image.adjustmentLayerId);
        }
        if (layer.type() == LayerType::Group) {
            for (const auto& child : static_cast<const GroupLayer&>(layer).children)
                includeSubtree(*child);
        }
    };

    // Normaliza primeiro a seleção: um descendente selecionado junto com o
    // grupo já pertence ao snapshot desse grupo e não deve gerar outra raiz.
    std::function<void(const Layer&)> selectRoots = [&](const Layer& layer) {
        if (selected.contains(layer.id())) {
            selectedRoots.insert(layer.id());
            includeSubtree(layer);
            return;
        }
        if (layer.type() == LayerType::Group) {
            for (const auto& child : static_cast<const GroupLayer&>(layer).children)
                selectRoots(*child);
        }
    };
    for (const auto& child : document.rootGroup()->children)
        selectRoots(*child);

    // O vínculo é normalmente bidirecional, mas sourceLayerId também basta
    // para identificar um ajuste ligado a uma das imagens capturadas.
    std::function<void(const Layer&)> findSourceLinks = [&](const Layer& layer) {
        if (layer.type() == LayerType::Adjustment) {
            const auto& adjustment = static_cast<const AdjustmentLayer&>(layer);
            if (adjustment.sourceLayerId
                && includedImageIds.contains(*adjustment.sourceLayerId)) {
                linkedAdjustmentIds.insert(layer.id());
            }
        }
        if (layer.type() == LayerType::Group) {
            for (const auto& child : static_cast<const GroupLayer&>(layer).children)
                findSourceLinks(*child);
        }
    };
    for (const auto& child : document.rootGroup()->children)
        findSourceLinks(*child);

    QSet<LayerId> captureIds = selectedRoots;
    for (const LayerId& id : linkedAdjustmentIds) {
        // Um ajuste já selecionado, ou contido num grupo selecionado, é
        // capturado uma única vez. Ajustes inválidos/antigos são ignorados.
        if (!includedIds.contains(id) && document.findAdjustmentLayer(id))
            captureIds.insert(id);
    }

    // Percorrer só depois de conhecer todas as dependências mantém a ordem de
    // pintura mesmo quando o ajuste aparece antes da imagem no documento.
    std::function<void(const Layer&)> captureLayers = [&](const Layer& layer) {
        if (captureIds.contains(layer.id())) {
            Entry entry{layer.id(), layer.deepCopy()};
            if (entry.layer) {
                // Preserva a correspondência entre os IDs originais e os IDs
                // do snapshot, incluindo todos os descendentes do grupo.
                mapLayerIds(layer, *entry.layer, m_sourceToSnapshot);
                collectAssets(document, layer, m_assets);
                m_layers.push_back(std::move(entry));
            }
            return;
        }
        if (layer.type() == LayerType::Group) {
            for (const auto& child : static_cast<const GroupLayer&>(layer).children)
                captureLayers(*child);
        }
    };
    for (const auto& child : document.rootGroup()->children)
        captureLayers(*child);

    if (m_layers.empty()) {
        clear();
        return false;
    }
    return true;
}

QList<LayerId> LayerClipboard::paste(Document& target, CommandStack* history)
{
    QList<LayerId> pastedIds;
    if (m_layers.empty())
        return pastedIds;

    std::vector<std::unique_ptr<Layer>> clones;
    clones.reserve(m_layers.size());
    QHash<LayerId, LayerId> idMap;
    for (const auto& entry : m_layers) {
        auto clone = entry.layer->deepCopy();
        if (!clone)
            continue;
        // ID da raiz original → nova raiz.
        idMap.insert(entry.sourceId, clone->id());
        // IDs do snapshot → novos clones.
        mapLayerIds(*entry.layer, *clone, idMap);
        if (!clone->name.isEmpty())
            clone->name += QStringLiteral(" copy");
        pastedIds.append(clone->id());
        clones.push_back(std::move(clone));
    }
    if (clones.empty())
        return {};

    // Ligações guardadas no snapshot apontam para IDs do documento original.
    // Resolve original → snapshot → clone também para filhos de grupos.
    for (auto it = m_sourceToSnapshot.constBegin();
         it != m_sourceToSnapshot.constEnd(); ++it) {
        const auto cloneIt = idMap.constFind(it.value());
        if (cloneIt != idMap.constEnd())
            idMap.insert(it.key(), cloneIt.value());
    }

    for (auto& clone : clones)
        remapLinksAndAssets(*clone, target, m_assets, idMap);

    auto compound = std::make_unique<CompoundCommand>(QStringLiteral("clipboard.paste"));
    for (auto& clone : clones)
        compound->addCommand(std::make_unique<AddLayerCommand>(
            target, std::move(clone), target.rootGroup(), -1));

    if (history) {
        history->execute(std::move(compound));
    } else {
        compound->redo();
    }
    return pastedIds;
}

void LayerClipboard::clear()
{
    m_layers.clear();
    m_assets.clear();
    m_sourceToSnapshot.clear();
}

} // namespace cc
