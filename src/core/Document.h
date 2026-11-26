#pragma once

#include <QObject>
#include <QVariant>
#include <QDateTime>
#include <QPointF>
#include <QtGlobal>

#include <memory>
#include <optional>
#include <QSet>

#include "assets/AssetStore.h"
#include "document/Guide.h"
#include "document/LayerComps.h"
#include "document/WebSlice.h"
#include "document/DocumentVersion.h"
#include "document/DocumentVersionStore.h"
#include "layers/Layer.h"
#include "color/ColorSpace.h"
#include "selection/SelectionState.h"

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

    /// Altera as dimensões da tela (canvas). As camadas mantêm as transforms;
    /// |layerOffset| desloca as camadas do grupo raiz (usado pela âncora do
    /// diálogo "Canvas Size"). Emite canvasSizeChanged.
    bool setCanvasSize(int width, int height, const QPointF& layerOffset = {});

    /// Rotaciona a tela em `degrees` (90, -90 ou 180): troca W/H quando
    /// necessário e roda as transforms das camadas do grupo raiz à volta do
    /// centro do canvas (grupos propagam a rotação aos filhos).
    bool rotateCanvas(double degrees);

    ColorSpace colorSpace() const { return m_colorSpace; }
    void setColorSpace(ColorSpace space);

    /// Original ICC profile imported from the document's source image.
    /// Preserved across project save/load and preferred when exporting.
    QByteArray iccProfile() const { return m_iccProfile; }
    void setIccProfile(const QByteArray& profile);

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
    /// Remove assets não referenciados por nenhuma camada (e que não estejam em extraKeepIds).
    /// Retorna a quantidade de assets expurgados da memória.
    int purgeUnreferencedAssets(const QSet<LayerId>& extraKeepIds = {});

    bool setLayerName(const LayerId& id, QString name);
    bool setLayerVisible(const LayerId& id, bool visible);
    bool setLayerLocked(const LayerId& id, bool locked);
    bool setGroupExpanded(const LayerId& id, bool expanded);
    bool setLayerColorTag(const LayerId& id, LayerColorTag tag);
    bool setLayerOpacity(const LayerId& id, float opacity);
    bool setLayerBlendMode(const LayerId& id, BlendMode mode);
    bool setLayerBlendIf(const LayerId& id, BlendIf blendIf);
    bool setLayerFillOpacity(const LayerId& id, float fillOpacity);
    bool setLayerClipped(const LayerId& id, bool clipped);
    bool setLayerLinkGroup(const LayerId& id, std::optional<qint64> linkGroup);
    bool setLayerVectorMask(const LayerId& id, QImage mask);
    /// Substitui os nós de uma camada vectorial (edição de caminho Bézier).
    /// Devolve false se a camada não existir ou não for vectorial.
    bool setVectorLayerNodes(const LayerId& id, std::vector<VectorNode> nodes);
    /// Vincula uma camada de ajuste a uma camada de imagem (ajuste não destrutivo).
    /// Se |adjustmentLayerId| for nulo, remove o vínculo existente.
    bool setLayerAdjustmentLayerId(const LayerId& imageLayerId,
                                   const std::optional<LayerId>& adjustmentLayerId);
    /// Cria uma nova camada de ajuste vinculada (opcionalmente) a uma camada de imagem.
    /// Retorna o id da nova camada ou inválido em caso de falha.
    LayerId createAdjustmentLayer(AdjustmentType type,
                                  const std::optional<LayerId>& sourceLayerId = {});
    /// Retorna a camada de ajuste com o id dado, ou nullptr se não existir ou não for ajuste.
    AdjustmentLayer* findAdjustmentLayer(const LayerId& id) const;
    /// Parâmetros de uma camada de ajuste (chave → valor). Usado pelo
    /// LayerPropertyCommand para undo/redo dos sliders do painel.
    bool setAdjustmentParameters(
        const LayerId& id,
        std::vector<std::pair<QString, double>> parameters);
    /// Muda o tipo de uma camada de ajuste (ex.: o utilizador aplica Curves
    /// onde havia Levels). Parâmetros existentes ficam como estavam — a UI
    /// reescreve-os de seguida quando o diálogo é confirmado.
    bool setAdjustmentKind(const LayerId& id, AdjustmentType kind);
    bool setLayerLocks(const LayerId& id, LayerLocks locks);
    /// Locks efetivos: os da camada UNIDOS aos de todos os grupos
    /// ancestrais (Photocraft locks_at: trancar um grupo tranca o
    /// conteúdo). O booleano legacy `locked` também entra na união.
    LayerLocks effectiveLocks(const LayerId& id) const;
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
    /// Texto em caminho: vincula (ou desvincula, com nullopt) o texto a uma
    /// camada vectorial. Só aceita camadas de texto e caminhos vectoriais.
    bool setTextPathLayerId(const LayerId& textId, std::optional<LayerId> pathLayerId);
    /// Desloca o ponto de partida do texto ao longo do caminho (0..1).
    bool setTextPathOffset(const LayerId& textId, double offsetPct);

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

    const QImage& pixelSelection() const { return m_selection.image(); }
    SelectionSpace pixelSelectionSpace() const { return m_selection.imageSpace(); }
    /// A API legacy infere Document quando as dimensões coincidem com o
    /// canvas e Layer nos restantes casos. O overload explícito elimina a
    /// ambiguidade quando uma camada tem exatamente o tamanho do documento.
    void setPixelSelection(QImage selection);
    void setPixelSelection(QImage selection, SelectionSpace space);
    void clearPixelSelection();

    /// Última seleção descartada/substituída — usado pelo comando Reselect.
    const QImage& previousPixelSelection() const { return m_selection.previousImage(); }

    // Seleções guardadas com nome (canais de seleção).
    const std::vector<SavedSelection>& savedSelections() const { return m_selection.savedSelections(); }
    QUuid addSavedSelection(const QString& name, const QImage& mask);
    QUuid addSavedSelection(const QString& name, const QImage& mask,
                            SelectionSpace space);
    /// Restaura uma seleção guardada vinda do projeto, preservando o id/espaço.
    QUuid restoreSavedSelection(const QUuid& id, const QString& name, const QImage& mask);
    QUuid restoreSavedSelection(const QUuid& id, const QString& name, const QImage& mask,
                                SelectionSpace space);
    bool removeSavedSelection(const QUuid& id);
    const SavedSelection* findSavedSelection(const QUuid& id) const
    {
        return m_selection.findSavedSelection(id);
    }
    void clearSavedSelections();

    /// Snapshot usado pelos comandos de canvas: recorte/rotação não são
    /// reversíveis por simples aplicação da transformação inversa.
    SelectionState selectionSnapshot() const { return m_selection; }
    void restoreSelectionSnapshot(const SelectionState& snapshot);

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
    void setWebSlices(std::vector<WebSlice> slices);

    // Guias do canvas (coordenadas de documento)
    const std::vector<Guide>& guides() const { return m_guides; }
    void addGuide(Guide guide);
    void clearGuides();
    void setGuides(std::vector<Guide> guides);

    quint64 revision() const { return m_revision; }

    // ---------- Version History ----------
    /// Versão do documento (snapshot do estado em um momento).
    using DocumentVersion = cc::DocumentVersion;

    /// Cria uma versão do estado atual do documento.
    /// Retorna o id da versão criada.
    QUuid createDocumentVersion(const QString& name, const QString& description = QString());

    /// Lista todas as versões salvas (mais recente primeiro).
    std::vector<DocumentVersion> documentVersions() const;

    /// Restaura o documento para a versão com o id dado.
    /// Retorna true se bem-sucedido.
    bool restoreDocumentVersion(const QUuid& versionId);

    /// Remove uma versão pelo id.
    bool removeDocumentVersion(const QUuid& versionId);

    /// Limpa todas as versões.
    void clearDocumentVersions();

signals:
    void structureChanged();
    void layerPropertyChanged(const LayerId& id);
    void canvasSizeChanged(int width, int height);

private:
    Layer* findRecursive(GroupLayer* group, const LayerId& id) const;
    GroupLayer* findParentRecursive(GroupLayer* group, const LayerId& id) const;
    std::unique_ptr<Layer> detachLayer(const LayerId& id);
    SelectionSpace inferredSelectionSpace(const QImage& mask) const;
    void bumpRevision() { ++m_revision; }

    int m_width;
    int m_height;
    int m_dpi;
    ColorSpace m_colorSpace = ColorSpace::sRGB;
    QByteArray m_iccProfile;
    std::unique_ptr<GroupLayer> m_root;
    quint64 m_revision = 0;
    AssetStore m_assets;
    SelectionState m_selection;
    std::vector<LayerComp> m_layerComps;
    std::vector<WebSlice> m_webSlices;
    std::vector<Guide> m_guides;

    // Version History
    DocumentVersionStore m_documentVersions;
};

} // namespace cc
