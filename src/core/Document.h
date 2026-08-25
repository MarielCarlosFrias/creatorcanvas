#pragma once

#include <QObject>
#include <QtGlobal>

#include <memory>

#include "assets/AssetStore.h"
#include "layers/Layer.h"

namespace cc {

/// Owns the layer tree and document-level state. ALL structural mutations
/// go through this class: it maintains invariants (membership, acyclicity,
/// opacity range) and emits change notifications.
///
/// Z-order contract: rootGroup()->children[0] is the BOTTOM layer and the
/// last element is the TOP layer, matching paint order (bottom -> top).
///
/// reorderLayer() semantics: after a successful call the layer sits at
/// exactly |newIndex| within |newParent| (final-position semantics).
class Document final : public QObject
{
    Q_OBJECT
public:
    explicit Document(int width, int height, int dpi = 96,
                      QObject* parent = nullptr);
    ~Document() override;

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    // Canvas -----------------------------------------------------------------
    int width() const { return m_width; }
    int height() const { return m_height; }
    int dpi() const { return m_dpi; }

    // Assets -------------------------------------------------------------------
    AssetStore& assets() { return m_assets; }
    const AssetStore& assets() const { return m_assets; }

    // Tree access (non-owning pointers) ----------------------------------------
    GroupLayer* rootGroup() const { return m_root.get(); }  // never null
    Layer* findLayer(const LayerId& id) const;              // null if absent
    GroupLayer* parentOf(const LayerId& id) const;          // null for root/absent
    int indexOf(const LayerId& id) const;                   // -1 for root/absent

    // Structural operations ------------------------------------------------------
    bool addLayer(std::unique_ptr<Layer> layer,
                  GroupLayer* parent = nullptr,
                  int index = -1);
    std::unique_ptr<Layer> takeLayer(const LayerId& id);
    bool removeLayer(const LayerId& id);
    bool reorderLayer(const LayerId& id, GroupLayer* newParent, int newIndex);
    LayerId duplicateLayer(const LayerId& id);

    // Property operations (return true if the layer exists;
    // emit layerPropertyChanged only on actual change) ----------------------------
    bool setLayerName(const LayerId& id, QString name);
    bool setLayerVisible(const LayerId& id, bool visible);
    bool setLayerLocked(const LayerId& id, bool locked);
    bool setLayerOpacity(const LayerId& id, float opacity);
    bool setLayerBlendMode(const LayerId& id, BlendMode mode);

    // Change tracking ---------------------------------------------------------
    quint64 revision() const { return m_revision; }

signals:
    void structureChanged();
    void layerPropertyChanged(const LayerId& id);

private:
    Layer* findRecursive(GroupLayer* group, const LayerId& id) const;
    GroupLayer* findParentRecursive(GroupLayer* group, const LayerId& id) const;
    std::unique_ptr<Layer> detachLayer(const LayerId& id);
    void bumpRevision() { ++m_revision; }

    int m_width;
    int m_height;
    int m_dpi;
    std::unique_ptr<GroupLayer> m_root;
    AssetStore m_assets;
    quint64 m_revision = 0;
};

} // namespace cc
