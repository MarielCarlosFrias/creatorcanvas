#pragma once

#include "BaseCommands.h"
#include <limits>
#include <algorithm>

namespace cc {

/// Replaces sibling layers with one raster layer while retaining every source
/// layer for undo. The raster pixels are prepared through CanvasRenderer.
class ReplaceLayersWithRasterCommand final : public Command
{
public:
    ReplaceLayersWithRasterCommand(Document& doc, QList<LayerId> layerIds,
                                   std::unique_ptr<ImageLayer> raster,
                                   QString commandName)
        : Command(std::move(commandName))
        , m_doc(&doc)
        , m_layerIds(std::move(layerIds))
        , m_raster(std::move(raster))
    {
        Q_ASSERT(m_raster && !m_layerIds.isEmpty());
        GroupLayer* parent = doc.parentOf(m_layerIds.first());
        Q_ASSERT(parent);
        m_parentId = parent ? parent->id() : doc.rootGroup()->id();
        m_rasterId = m_raster->id();
        m_insertIndex = std::numeric_limits<int>::max();
        for (const LayerId& id : m_layerIds) {
            Q_ASSERT(doc.parentOf(id) == parent);
            SourceInfo info;
            info.id = id;
            info.index = doc.indexOf(id);
            m_insertIndex = qMin(m_insertIndex, info.index);
            m_sources.push_back(std::move(info));
        }
        std::sort(m_sources.begin(), m_sources.end(),
                  [](const SourceInfo& a, const SourceInfo& b) { return a.index < b.index; });
    }

    const LayerId& rasterId() const { return m_rasterId; }

    void redo() override
    {
        for (const auto& source : m_sources) {
            if (m_doc->effectiveLocks(source.id).all)
                return;
        }

        for (auto& source : m_sources) {
            if (auto taken = m_doc->takeLayer(source.id))
                source.layer = std::move(taken);
        }
        Q_ASSERT(m_raster);
        m_doc->addLayer(std::move(m_raster), detail::resolveGroup(m_doc, m_parentId), m_insertIndex);
    }

    void collectAssetIds(QSet<LayerId>& out) const override
    {
        detail::collectLayerAssetIds(m_raster.get(), out);
        for (const auto& source : m_sources)
            detail::collectLayerAssetIds(source.layer.get(), out);
    }

    void undo() override
    {
        m_raster = m_doc->takeLayer(m_rasterId);
        Q_ASSERT(m_raster);
        GroupLayer* parent = detail::resolveGroup(m_doc, m_parentId);
        for (auto& source : m_sources) {
            Q_ASSERT(source.layer);
            m_doc->addLayer(std::move(source.layer), parent, source.index);
        }
    }

private:
    struct SourceInfo {
        LayerId id;
        int index = -1;
        std::unique_ptr<Layer> layer;
    };

    Document* m_doc;
    QList<LayerId> m_layerIds;
    LayerId m_parentId;
    LayerId m_rasterId;
    int m_insertIndex = 0;
    std::vector<SourceInfo> m_sources;
    std::unique_ptr<Layer> m_raster;
};

} // namespace cc
