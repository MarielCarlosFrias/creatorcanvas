#pragma once

#include <QHash>
#include <QImage>
#include <QList>
#include <vector>
#include <memory>

#include "core/Document.h"

namespace cc {

class CommandStack;

/// Clipboard interno de camadas. Guarda cópias independentes das camadas e
/// dos bitmaps referenciados, permitindo colar várias camadas e mudar de
/// documento sem deixar assetIds pendurados.
class LayerClipboard final
{
public:
    LayerClipboard() = default;
    LayerClipboard(const LayerClipboard&) = delete;
    LayerClipboard& operator=(const LayerClipboard&) = delete;
    LayerClipboard(LayerClipboard&&) = default;
    LayerClipboard& operator=(LayerClipboard&&) = default;
    ~LayerClipboard() = default;

    /// Copia as camadas selecionadas por ordem de pintura, incluindo os
    /// AdjustmentLayers ligados às imagens capturadas. Se um grupo for
    /// selecionado, os descendentes não são duplicados por seleção separada;
    /// ajustes já selecionados ou contidos no grupo também entram só uma vez.
    /// Devolve false se não houver nada válido para copiar.
    bool capture(const Document& document, const QList<LayerId>& layerIds);

    /// Cola no topo do grupo raiz como UM passo de undo. Os assets e as
    /// relações entre camadas são remapeados para os novos ids.
    QList<LayerId> paste(Document& target, CommandStack* history);

    void clear();
    bool isEmpty() const { return m_layers.empty(); }
    int layerCount() const { return static_cast<int>(m_layers.size()); }

private:
    struct Entry {
        LayerId sourceId;
        std::unique_ptr<Layer> layer;
    };

    std::vector<Entry> m_layers;
    QHash<LayerId, QImage> m_assets;
    /// IDs do documento original → IDs do snapshot interno (necessário porque
    /// deepCopy renova os IDs, mas os links continuam a apontar ao original).
    QHash<LayerId, LayerId> m_sourceToSnapshot;
};

} // namespace cc
