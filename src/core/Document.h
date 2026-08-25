#pragma once

#include <QObject>
#include <QtGlobal>

#include <memory>

#include "assets/AssetStore.h"
#include "layers/Layer.h"

namespace cc {

/// Owns the layer tree, the asset store and document-level state. ALL
/// structural mutations go through this class.
///
/// Z-order: rootGroup()->children[0] is the BOTTOM layer, the last element
/// is the TOP layer (paint order). reorderLayer() uses final-position
/// semantics.
class Document final : public QObject
{
    Q_OBJECT
public:
    explicit Document(int width, int height, int dpi = 96,
                      QObject* parent = nullptr);
    ~Document() override;

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    int width() const { return m_width; }
    int height() const { return m_height; }
    int dpi() const { return m_dpi; }

    AssetStore& assets() { return m_assets; }
    const AssetStore& assets() const { return m_assets; }

    GroupLayer* rootGroup() const { return m_root.get(); }
    Layer* findLayer(const LayerId& id) const;
    GroupLayer* parentOf(const LayerId& id) const;
    int indexOf(const LayerId& id) const;

    bool addLayer(std::unique_ptr<Layer> layer,
                  GroupLayer* parent = nullptr,
                  int index = -1);
    std::unique_ptr<Layer> takeLayer(const LayerId& id);
    bool removeLayer(const LayerId& id);
    bool reorderLayer(const LayerId& id, GroupLayer* newParent, int newIndex);
    LayerId duplicateLayer(const LayerId& id);

    bool setLayerName(const LayerId& id, QString name);
    bool setLayerVisible(const LayerId& id, bool visible);
    bool setLayerLocked(const LayerId& id, bool locked);
    bool setLayerOpacity(const LayerId& id, float opacity);
    bool setLayerBlendMode(const LayerId& id, BlendMode mode);
    bool setLayerTransform(const LayerId& id, const AffineTransform& transform);
    bool setLayerTextBox(const LayerId& id, const QSizeF& box);
    bool setLayerTextEffects(const LayerId& id, const TextEffects& effects);
    void touchLayer(const LayerId& id);
    bool setLayerTextContent(const LayerId& id, QString content);

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
    quint64 m_revision = 0;
    AssetStore m_assets;
};

} // namespace cc
