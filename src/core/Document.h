#pragma once

#include <QObject>
#include <QtGlobal>

#include <memory>

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
///
/// addLayer() consumes ownership of |layer| even on failure; failures
/// indicate programming errors (null layer, or a parent that does not
/// belong to this document).
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

    // Tree access (non-owning pointers) ----------------------------------------
    GroupLayer* rootGroup() const { return m_root.get(); }  // never null
    Layer* findLayer(const LayerId& id) const;              // null if absent
    GroupLayer* parentOf(const LayerId& id) const;          // null for root/absent
    int indexOf(const LayerId& id) const;                   // -1 for root/absent

    // Structural operations ------------------------------------------------------
    /// |index| -1 appends (top). Returns false on invalid input.
    bool addLayer(std::unique_ptr<Layer> layer,
                  GroupLayer* parent = nullptr,
                  int index = -1);
    /// Removes and returns ownership (for undo). Null if absent or root.
    std::unique_ptr<Layer> takeLayer(const LayerId& id);
    bool removeLayer(const LayerId& id);
    bool reorderLayer(const LayerId& id, GroupLayer* newParent, int newIndex);
    /// Deep copy with fresh ids, inserted directly ABOVE the original,
    /// named "<original> copy". Returns a null id on failure.
    LayerId duplicateLayer(const LayerId& id);

    // Property operations (return true if the layer exists;
    // emit layerPropertyChanged only on actual change) ----------------------------
    bool setLayerName(const LayerId& id, QString name);
    bool setLayerVisible(const LayerId& id, bool visible);
    bool setLayerLocked(const LayerId& id, bool locked);
    bool setLayerOpacity(const LayerId& id, float opacity);  // clamped [0,1]
    bool setLayerBlendMode(const LayerId& id, BlendMode mode);

    // Change tracking ---------------------------------------------------------
    /// Bumped on every effective mutation; render caches key off this.
    quint64 revision() const { return m_revision; }

signals:
    /// Tree topology changed (add / remove / reorder / duplicate).
    void structureChanged();
    /// A layer's name / visibility / lock / opacity / blend mode changed.
    void layerPropertyChanged(const LayerId& id);

private:
    Layer* findRecursive(GroupLayer* group, const LayerId& id) const;
    GroupLayer* findParentRecursive(GroupLayer* group, const LayerId& id) const;
    /// Removes the layer from its parent WITHOUT notifying. Null if absent/root.
    std::unique_ptr<Layer> detachLayer(const LayerId& id);
    void bumpRevision() { ++m_revision; }

    int m_width;
    int m_height;
    int m_dpi;
    std::unique_ptr<GroupLayer> m_root;
    quint64 m_revision = 0;
};

} // namespace cc
