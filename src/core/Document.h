#pragma once

#include <QObject>
#include <QtGlobal>

#include <memory>
#include <QSet>

#include "assets/AssetStore.h"
#include "document/LayerComps.h"
#include "document/WebSlice.h"
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

    /// Todos os assetIds referenciados por camadas de imagem do documento
    /// (recursivo em grupos). Base do GC do AssetStore.
    /// includeTransient=false exclui camadas de prévia (mockup) — usado
    /// pelo writer para não gravar assets órfãos no arquivo.
    QSet<LayerId> referencedAssetIds(bool includeTransient = true) const;

    bool setLayerName(const LayerId& id, QString name);
    bool setLayerVisible(const LayerId& id, bool visible);
    bool setLayerLocked(const LayerId& id, bool locked);
    bool setGroupExpanded(const LayerId& id, bool expanded);
    bool setLayerColorTag(const LayerId& id, LayerColorTag tag);
    bool setLayerOpacity(const LayerId& id, float opacity);
    bool setLayerBlendMode(const LayerId& id, BlendMode mode);
    bool setLayerBlendIf(const LayerId& id, BlendIf blendIf);
    bool setLayerTransform(const LayerId& id, const AffineTransform& transform);
    bool setLayerTextBox(const LayerId& id, const QSizeF& box);
    bool setLayerTextEffects(const LayerId& id, const TextEffects& effects);
    bool setLayerImageEffects(const LayerId& id, const ImageEffects& effects);
    void touchLayer(const LayerId& id);
    bool setLayerTextContent(const LayerId& id, QString content);
    bool setLayerFontFamily(const LayerId& id, QString family);
    bool setLayerTextSize(const LayerId& id, double sizePt);
    bool setLayerTextBold(const LayerId& id, bool bold);
    bool setLayerTextItalic(const LayerId& id, bool italic);
    bool setLayerTextUnderline(const LayerId& id, bool underline);
    bool setLayerTextColor(const LayerId& id, QColor color);
    bool setLayerTextAlign(const LayerId& id, TextAlignment align);
    bool setLayerLetterSpacing(const LayerId& id, double letterSpacingPx);
    bool setLayerLineHeight(const LayerId& id, double lineHeightMult);

    // Propriedades de ShapeLayer (retângulos, elipses, linhas)
    // Passadas por valor para garantir compatibilidade com LayerPropertyCommand (Undo/Redo)
    bool setShapeFill(const LayerId& id, QColor fill);
    bool setShapeGradient(const LayerId& id, Gradient gradient);
    bool setShapeStarPoints(const LayerId& id, int points);
    bool setShapeStarInnerRatio(const LayerId& id, double ratio);
    bool setShapeStrokeDashRatio(const LayerId& id, double ratio);
    bool setShapeShadow(const LayerId& id, ShapeShadow shadow);
    bool setBackgroundGradient(const LayerId& id, Gradient gradient);
    bool setShapeStroke(const LayerId& id, QColor stroke);
    bool setShapeStrokeWidth(const LayerId& id, double width);
    bool setShapeCornerRadius(const LayerId& id, double radius);
    bool setShapeKind(const LayerId& id, ShapeKind kind);

    // Atualiza o asset associado a uma camada de imagem (ImageLayer), com dimensões naturais
    bool setImageLayerAsset(const LayerId& id, const LayerId& newAssetId, int naturalWidth, int naturalHeight);
    bool setLayerAlphaMask(const LayerId& id, QImage mask);
    bool invertLayerAlphaMask(const LayerId& id);
    bool removeLayerAlphaMask(const LayerId& id);

    const QImage& pixelSelection() const { return m_pixelSelection; }
    void setPixelSelection(QImage selection);
    void clearPixelSelection();

    // Layer Comps (Composições de Camadas estilo PhotoCraft / Photoshop)
    const std::vector<LayerComp>& layerComps() const { return m_layerComps; }
    void addLayerComp(LayerComp comp);
    bool applyLayerComp(const QString& compId);
    bool removeLayerComp(const QString& compId);

    // Web Slices (Fatias de Exportação Web)
    const std::vector<WebSlice>& webSlices() const { return m_webSlices; }
    void addWebSlice(WebSlice slice);
    bool removeWebSlice(const QString& sliceId);
    void clearWebSlices();

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
    QImage m_pixelSelection;
    std::vector<LayerComp> m_layerComps;
    std::vector<WebSlice> m_webSlices;
};

} // namespace cc
