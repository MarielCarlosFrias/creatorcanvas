#pragma once

#include "Command.h"
#include "core/Document.h"

#include <QtGlobal>
#include <type_traits>

#include <memory>
#include <limits>

namespace cc {

class SetLayerTransformCommand final : public Command
{
public:
    SetLayerTransformCommand(Document& doc, const LayerId& id,
                             AffineTransform oldValue, AffineTransform newValue)
        : Command(QStringLiteral("layer.transform"))
        , m_doc(&doc)
        , m_id(id)
        , m_oldValue(std::move(oldValue))
        , m_newValue(std::move(newValue))
    {
    }

    void redo() override { m_doc->setLayerTransform(m_id, m_newValue); }
    void undo() override { m_doc->setLayerTransform(m_id, m_oldValue); }
    bool mergeWith(const Command& other) override
    {
        const auto* rhs = dynamic_cast<const SetLayerTransformCommand*>(&other);
        if (!rhs || rhs->m_doc != m_doc || rhs->m_id != m_id)
            return false;
        m_newValue = rhs->m_newValue;
        return true;
    }

private:
    Document* m_doc;
    LayerId m_id;
    AffineTransform m_oldValue;
    AffineTransform m_newValue;
};

namespace detail {
inline GroupLayer* resolveGroup(Document* doc, const LayerId& id)
{
    Layer* p = doc->findLayer(id);
    return (p && p->type() == LayerType::Group)
               ? static_cast<GroupLayer*>(p)
               : nullptr;
}

/// Percorre a camada (e subgrupos) coletando os assetIds que ela mantém
/// vivos no AssetStore. Usado pelo GC de comandos expulsos da pilha.
inline void collectLayerAssetIds(const Layer* layer, QSet<LayerId>& out)
{
    if (!layer)
        return;
    if (layer->type() == LayerType::Image)
        out.insert(static_cast<const ImageLayer*>(layer)->assetId);
    if (layer->type() == LayerType::Group)
        for (const auto& child : static_cast<const GroupLayer*>(layer)->children)
            collectLayerAssetIds(child.get(), out);
}
} // namespace detail

class AddLayerCommand final : public Command
{
public:
    explicit AddLayerCommand(Document& doc, std::unique_ptr<Layer> layer,
                             GroupLayer* parent = nullptr, int index = -1)
        : Command(QStringLiteral("layer.add"))
        , m_doc(&doc)
        , m_layer(std::move(layer))
        , m_layerId(m_layer ? m_layer->id() : LayerId())
        , m_parentId(parent ? parent->id() : doc.rootGroup()->id())
        , m_index(index)
    {
        Q_ASSERT(m_layer);
    }

    void redo() override
    {
        Q_ASSERT(m_layer);
        m_doc->addLayer(std::move(m_layer),
                        detail::resolveGroup(m_doc, m_parentId), m_index);
    }

    void undo() override
    {
        m_layer = m_doc->takeLayer(m_layerId);
        Q_ASSERT(m_layer);
    }

    void collectAssetIds(QSet<LayerId>& out) const override
    {
        detail::collectLayerAssetIds(m_layer.get(), out);
    }

private:
    Document* m_doc;
    std::unique_ptr<Layer> m_layer;
    LayerId m_layerId;
    LayerId m_parentId;
    int m_index;
};

/// Builds the common "text behind subject" composition without flattening
/// anything: the original text stays below the subject and a duplicate with
/// transparent fill/outline enabled is placed above it.
class TextBehindSubjectCommand final : public Command
{
public:
    TextBehindSubjectCommand(Document& doc, const LayerId& textId,
                             const LayerId& subjectId)
        : Command(QStringLiteral("text.behindSubject"))
        , m_doc(&doc)
        , m_textId(textId)
        , m_subjectId(subjectId)
    {
        Layer* text = doc.findLayer(textId);
        Layer* subject = doc.findLayer(subjectId);
        GroupLayer* parent = text ? doc.parentOf(textId) : nullptr;
        Q_ASSERT(text && text->type() == LayerType::Text);
        Q_ASSERT(subject && subject->type() == LayerType::Image);
        Q_ASSERT(parent && parent == doc.parentOf(subjectId));
        m_parentId = parent ? parent->id() : doc.rootGroup()->id();
        m_oldTextIndex = doc.indexOf(textId);
        m_oldSubjectEffects = static_cast<ImageLayer*>(subject)->effects;
        m_duplicate = text->deepCopy();
        m_duplicate->name = text->name + QStringLiteral(" outline");
        const auto* sourceText = static_cast<const TextLayer*>(text);
        auto* outlineText = static_cast<TextLayer*>(m_duplicate.get());
        outlineText->color = Qt::transparent;
        outlineText->effects.hollow = true;
        outlineText->effects.knockout = false;
        outlineText->effects.outline.enabled = true;
        outlineText->effects.outline.color = sourceText->color;
        outlineText->effects.outline.width = qMax(2.0, sourceText->effects.outline.width);
        m_newSubjectEffects = m_oldSubjectEffects;
        m_newSubjectEffects.outline.enabled = true;
        m_newSubjectEffects.outline.color = sourceText->color;
        m_newSubjectEffects.outline.width = qMax(4.0, sourceText->effects.outline.width);
        m_newSubjectEffects.outline.blur = 0.0;
        m_newSubjectEffects.outline.opacity = 1.0;
        m_duplicateId = m_duplicate->id();
    }

    void redo() override
    {
        GroupLayer* parent = detail::resolveGroup(m_doc, m_parentId);
        Layer* subject = m_doc->findLayer(m_subjectId);
        Layer* text = m_doc->findLayer(m_textId);
        if (!parent || !subject || !text || !m_duplicate)
            return;

        int subjectIndex = m_doc->indexOf(m_subjectId);
        const int textIndex = m_doc->indexOf(m_textId);
        if (textIndex > subjectIndex) {
            m_doc->reorderLayer(m_textId, parent, subjectIndex);
            subjectIndex = m_doc->indexOf(m_subjectId);
        }
        m_doc->addLayer(std::move(m_duplicate), parent, subjectIndex + 1);
        m_doc->setLayerImageEffects(m_subjectId, m_newSubjectEffects);
    }

    void undo() override
    {
        m_duplicate = m_doc->takeLayer(m_duplicateId);
        if (m_duplicate)
            m_doc->reorderLayer(m_textId, detail::resolveGroup(m_doc, m_parentId), m_oldTextIndex);
        m_doc->setLayerImageEffects(m_subjectId, m_oldSubjectEffects);
    }

private:
    Document* m_doc;
    LayerId m_textId;
    LayerId m_subjectId;
    LayerId m_parentId;
    LayerId m_duplicateId;
    int m_oldTextIndex = -1;
    ImageEffects m_oldSubjectEffects;
    ImageEffects m_newSubjectEffects;
    std::unique_ptr<Layer> m_duplicate;
};

class RemoveLayerCommand final : public Command
{
public:
    explicit RemoveLayerCommand(Document& doc, const LayerId& id)
        : Command(QStringLiteral("layer.remove"))
        , m_doc(&doc)
        , m_id(id)
    {
        // A1: Q_ASSERT compila fora em release — um id pendurado
        // (seleção pós-undo) deref nulo aqui. Guard em vez de assert.
        GroupLayer* parent = doc.parentOf(id);
        m_valid = (parent != nullptr);
        if (m_valid) {
            m_parentId = parent->id();
            m_index = doc.indexOf(id);
        } else {
            m_parentId = LayerId();
            m_index = -1;
        }
    }

    void redo() override
    {
        if (!m_valid)
            return;
        Q_ASSERT(!m_layer);
        m_layer = m_doc->takeLayer(m_id);
        Q_ASSERT(m_layer);
    }

    void undo() override
    {
        if (!m_valid || !m_layer)
            return;
        m_doc->addLayer(std::move(m_layer),
                        detail::resolveGroup(m_doc, m_parentId), m_index);
    }

    void collectAssetIds(QSet<LayerId>& out) const override
    {
        detail::collectLayerAssetIds(m_layer.get(), out);
    }

private:
    Document* m_doc;
    LayerId m_id;
    LayerId m_parentId;
    int m_index = -1;
    bool m_valid = false;
    std::unique_ptr<Layer> m_layer;
};

class ReorderLayerCommand final : public Command
{
public:
    ReorderLayerCommand(Document& doc, const LayerId& id,
                        GroupLayer* newParent, int newIndex)
        : Command(QStringLiteral("layer.reorder"))
        , m_doc(&doc)
        , m_id(id)
    {
        // B3: Q_ASSERT some em release — um id inválido aqui virava null
        // deref. Guard explícito + comando no-op em vez de crash.
        GroupLayer* oldParent = doc.parentOf(id);
        m_valid = (oldParent != nullptr);
        if (m_valid) {
            m_oldParentId = oldParent->id();
            m_oldIndex = doc.indexOf(id);
            m_newParentId = newParent ? newParent->id() : doc.rootGroup()->id();
            m_newIndex = newIndex;
        }
    }

    void redo() override
    {
        if (m_valid)
            m_doc->reorderLayer(m_id, detail::resolveGroup(m_doc, m_newParentId),
                                m_newIndex);
    }

    void undo() override
    {
        if (m_valid)
            m_doc->reorderLayer(m_id, detail::resolveGroup(m_doc, m_oldParentId),
                                m_oldIndex);
    }

private:
    Document* m_doc;
    LayerId m_id;
    LayerId m_oldParentId;
    int m_oldIndex = -1;
    LayerId m_newParentId;
    int m_newIndex = -1;
    bool m_valid = false;
};

class DuplicateLayerCommand final : public Command
{
public:
    explicit DuplicateLayerCommand(Document& doc, const LayerId& originalId)
        : Command(QStringLiteral("layer.duplicate"))
        , m_doc(&doc)
        , m_originalId(originalId)
    {
        Q_ASSERT(doc.findLayer(originalId));
    }

    const LayerId& copyId() const { return m_copyId; }

    void redo() override
    {
        if (!m_created) {
            Layer* original = m_doc->findLayer(m_originalId);
            Q_ASSERT(original);
            m_clone = original->deepCopy();
            m_clone->name = original->name + QStringLiteral(" copy");
            m_copyId = m_clone->id();
            GroupLayer* parent = m_doc->parentOf(m_originalId);
            m_parentId = parent ? parent->id() : m_doc->rootGroup()->id();
            m_index = m_doc->indexOf(m_originalId) + 1;
            m_doc->addLayer(std::move(m_clone), parent, m_index);
            m_created = true;
        } else {
            Q_ASSERT(m_clone);
            m_doc->addLayer(std::move(m_clone),
                            detail::resolveGroup(m_doc, m_parentId), m_index);
        }
    }

    void undo() override
    {
        Q_ASSERT(!m_copyId.isNull());
        m_clone = m_doc->takeLayer(m_copyId);
        Q_ASSERT(m_clone);
    }

    void collectAssetIds(QSet<LayerId>& out) const override
    {
        detail::collectLayerAssetIds(m_clone.get(), out);
    }

private:
    Document* m_doc;
    LayerId m_originalId;
    LayerId m_copyId;
    LayerId m_parentId;
    int m_index = -1;
    bool m_created = false;
    std::unique_ptr<Layer> m_clone;
};

/// Applies a full text-effects set (outline + shadow).
class SetLayerTextEffectsCommand final : public Command
{
public:
    SetLayerTextEffectsCommand(Document& doc, const LayerId& id,
                               TextEffects oldValue, TextEffects newValue)
        : Command(QStringLiteral("layer.textEffects"))
        , m_doc(&doc)
        , m_id(id)
        , m_old(std::move(oldValue))
        , m_new(std::move(newValue))
    {
    }

    void redo() override { m_doc->setLayerTextEffects(m_id, m_new); }
    void undo() override { m_doc->setLayerTextEffects(m_id, m_old); }
    bool mergeWith(const Command& other) override
    {
        const auto* rhs = dynamic_cast<const SetLayerTextEffectsCommand*>(&other);
        if (!rhs || rhs->m_doc != m_doc || rhs->m_id != m_id)
            return false;
        m_new = rhs->m_new;
        return true;
    }

private:
    Document* m_doc;
    LayerId m_id;
    TextEffects m_old;
    TextEffects m_new;
};

/// Applies image effects (outline/glow/sticker).
class SetLayerImageEffectsCommand final : public Command
{
public:
    SetLayerImageEffectsCommand(Document& doc, const LayerId& id,
                                ImageEffects oldValue, ImageEffects newValue)
        : Command(QStringLiteral("layer.imageEffects"))
        , m_doc(&doc)
        , m_id(id)
        , m_old(std::move(oldValue))
        , m_new(std::move(newValue))
    {
    }

    void redo() override { m_doc->setLayerImageEffects(m_id, m_new); }
    void undo() override { m_doc->setLayerImageEffects(m_id, m_old); }

    /// Arrastar um slider gera um comando por tick; coalesce os seguidos
    /// da mesma camada num único passo de undo (mantém o oldValue primeiro).
    bool mergeWith(const Command& other) override
    {
        const auto* next = dynamic_cast<const SetLayerImageEffectsCommand*>(&other);
        if (!next || next->m_id != m_id)
            return false;
        m_new = next->m_new;
        return true;
    }

private:
    Document* m_doc;
    LayerId m_id;
    ImageEffects m_old;
    ImageEffects m_new;
};

/// Troca o asset bitmap de uma camada de imagem (filtros destrutivos como
/// Inverter/Posterizar/Threshold). O asset antigo continua no AssetStore,
/// então undo/redo é só restaurar o id e as dimensões gravadas.
class SetLayerImageAssetCommand final : public Command
{
public:
    SetLayerImageAssetCommand(Document& doc, const LayerId& layerId,
                              LayerId oldAssetId, int oldW, int oldH,
                              LayerId newAssetId, int newW, int newH)
        : Command(QStringLiteral("layer.imageAsset"))
        , m_doc(&doc)
        , m_id(layerId)
        , m_oldAssetId(std::move(oldAssetId))
        , m_oldW(oldW)
        , m_oldH(oldH)
        , m_newAssetId(std::move(newAssetId))
        , m_newW(newW)
        , m_newH(newH)
    {
    }

    void collectAssetIds(QSet<LayerId>& out) const override
    {
        out.insert(m_oldAssetId);
        out.insert(m_newAssetId);
    }

    void redo() override
    {
        m_doc->setImageLayerAsset(m_id, m_newAssetId, m_newW, m_newH);
    }
    void undo() override
    {
        m_doc->setImageLayerAsset(m_id, m_oldAssetId, m_oldW, m_oldH);
    }

private:
    Document* m_doc;
    LayerId m_id;
    LayerId m_oldAssetId;
    int m_oldW = 0;
    int m_oldH = 0;
    LayerId m_newAssetId;
    int m_newW = 0;
    int m_newH = 0;
};

/// Resizes a text layer's wrap box (handle gesture on text).
class SetLayerTextBoxCommand final : public Command
{
public:
    SetLayerTextBoxCommand(Document& doc, const LayerId& id,
                           QSizeF oldValue, QSizeF newValue)
        : Command(QStringLiteral("layer.textBox"))
        , m_doc(&doc)
        , m_id(id)
        , m_old(std::move(oldValue))
        , m_new(std::move(newValue))
    {
    }

    void redo() override { m_doc->setLayerTextBox(m_id, m_new); }
    void undo() override { m_doc->setLayerTextBox(m_id, m_old); }

private:
    Document* m_doc;
    LayerId m_id;
    QSizeF m_old;
    QSizeF m_new;
};

/// Generic property change routed through a Document setter, so clamping and
/// change notifications behave exactly like a direct edit.
///
/// CONVENTION: Document setters used with this command MUST take the value
/// BY VALUE (ValueType), e.g. setLayerName(id, QString). A setter taking
/// const T& will fail to compile here — that is intentional.
template <typename T>
class LayerPropertyCommand final : public Command
{
public:
    using ValueType = std::decay_t<T>;
    using ApplyFn = bool (Document::*)(const LayerId&, ValueType);

    LayerPropertyCommand(Document& doc, const LayerId& id, QString name,
                         ApplyFn apply, ValueType oldValue, ValueType newValue)
        : Command(std::move(name))
        , m_doc(&doc)
        , m_id(id)
        , m_apply(apply)
        , m_oldValue(std::move(oldValue))
        , m_newValue(std::move(newValue))
    {
    }

    void redo() override { (m_doc->*m_apply)(m_id, m_newValue); }
    void undo() override { (m_doc->*m_apply)(m_id, m_oldValue); }
    bool mergeWith(const Command& other) override
    {
        const auto* rhs = dynamic_cast<const LayerPropertyCommand<T>*>(&other);
        const bool mergeableTextEdit = name().startsWith(QStringLiteral("text.")) ||
                                       name() == QStringLiteral("layer.text");
        if (!mergeableTextEdit || !rhs || rhs->m_doc != m_doc || rhs->m_id != m_id ||
            rhs->m_apply != m_apply || rhs->name() != name())
            return false;
        m_newValue = rhs->m_newValue;
        return true;
    }

private:
    Document* m_doc;
    LayerId m_id;
    ApplyFn m_apply;
    ValueType m_oldValue;
    ValueType m_newValue;
};

using RenameLayerCommand     = LayerPropertyCommand<QString>;
using LayerVisibilityCommand = LayerPropertyCommand<bool>;
using LayerLockCommand       = LayerPropertyCommand<bool>;
using LayerColorTagCommand   = LayerPropertyCommand<LayerColorTag>;
using LayerOpacityCommand    = LayerPropertyCommand<float>;
using LayerBlendModeCommand  = LayerPropertyCommand<BlendMode>;
using LayerBlendIfCommand    = LayerPropertyCommand<BlendIf>;

// Comandos de histórico fortemente tipados para operações de formas geométricas (ShapeLayer)
// Permitem desfazer (Undo) e refazer (Redo) edições de cores, bordas e cantos arredondados
using SetShapeFillCommand         = LayerPropertyCommand<QColor>;
using SetShapeStrokeCommand       = LayerPropertyCommand<QColor>;
using SetShapeStrokeWidthCommand  = LayerPropertyCommand<double>;
using SetShapeCornerRadiusCommand = LayerPropertyCommand<double>;
using SetShapeKindCommand         = LayerPropertyCommand<ShapeKind>;
using SetShapeGradientCommand     = LayerPropertyCommand<Gradient>;
using SetShapeStarPointsCommand   = LayerPropertyCommand<int>;
using SetShapeStarInnerRatioCommand = LayerPropertyCommand<double>;
using SetShapeStrokeDashRatioCommand = LayerPropertyCommand<double>;
using SetShapeShadowCommand       = LayerPropertyCommand<ShapeShadow>;
using SetBackgroundGradientCommand = LayerPropertyCommand<Gradient>;

// Comandos de histórico para operações de texto (TextLayer)
using SetTextFontFamilyCommand = LayerPropertyCommand<QString>;
using SetTextSizeCommand       = LayerPropertyCommand<double>;
using SetTextBoldCommand       = LayerPropertyCommand<bool>;
using SetTextItalicCommand     = LayerPropertyCommand<bool>;
using SetTextUnderlineCommand  = LayerPropertyCommand<bool>;
using SetTextColorCommand      = LayerPropertyCommand<QColor>;
using SetTextAlignCommand      = LayerPropertyCommand<TextAlignment>;
using SetTextLetterSpacingCommand = LayerPropertyCommand<double>;
using SetTextLineHeightCommand   = LayerPropertyCommand<double>;

/// Comando reversível para modificações de conteúdo em camadas de imagem (corte, varinha mágica, tesoura, clone stamp).
/// Preserva o asset anterior e novo, dimensões naturais e transformações (posição/escala) para suporte completo a Desfazer/Refazer.
class ModifyImageLayerCommand final : public Command
{
public:
    ModifyImageLayerCommand(Document& doc, const LayerId& id,
                            LayerId oldAssetId, int oldWidth, int oldHeight, AffineTransform oldTransform,
                            LayerId newAssetId, int newWidth, int newHeight, AffineTransform newTransform,
                            QString commandName = QStringLiteral("image.modify"),
                            QImage oldAlphaMask = {}, QImage newAlphaMask = {})
        : Command(std::move(commandName))
        , m_doc(&doc)
        , m_id(id)
        , m_oldAssetId(oldAssetId)
        , m_oldWidth(oldWidth)
        , m_oldHeight(oldHeight)
        , m_oldTransform(std::move(oldTransform))
        , m_newAssetId(newAssetId)
        , m_newWidth(newWidth)
        , m_newHeight(newHeight)
        , m_newTransform(std::move(newTransform))
        , m_oldAlphaMask(std::move(oldAlphaMask))
        , m_newAlphaMask(std::move(newAlphaMask))
        , m_hasAlphaMasks(!m_oldAlphaMask.isNull() || !m_newAlphaMask.isNull())
    {
    }

    void collectAssetIds(QSet<LayerId>& out) const override
    {
        out.insert(m_oldAssetId);
        out.insert(m_newAssetId);
    }

    void redo() override
    {
        m_doc->setImageLayerAsset(m_id, m_newAssetId, m_newWidth, m_newHeight);
        if (m_hasAlphaMasks)
            m_doc->setLayerAlphaMask(m_id, m_newAlphaMask);
        m_doc->setLayerTransform(m_id, m_newTransform);
    }

    void undo() override
    {
        m_doc->setImageLayerAsset(m_id, m_oldAssetId, m_oldWidth, m_oldHeight);
        if (m_hasAlphaMasks)
            m_doc->setLayerAlphaMask(m_id, m_oldAlphaMask);
        m_doc->setLayerTransform(m_id, m_oldTransform);
    }

private:
    Document* m_doc;
    LayerId m_id;
    LayerId m_oldAssetId;
    int m_oldWidth;
    int m_oldHeight;
    AffineTransform m_oldTransform;
    LayerId m_newAssetId;
    int m_newWidth;
    int m_newHeight;
    AffineTransform m_newTransform;
    QImage m_oldAlphaMask;
    QImage m_newAlphaMask;
    bool m_hasAlphaMasks = false;
};

/// Macro command that executes multiple commands sequentially as a single undo/redo step.
class CompoundCommand final : public Command
{
public:
    explicit CompoundCommand(QString name = QStringLiteral("compound"))
        : Command(std::move(name))
    {
    }

    void addCommand(std::unique_ptr<Command> cmd)
    {
        if (cmd)
            m_commands.push_back(std::move(cmd));
    }

    bool isEmpty() const { return m_commands.empty(); }
    std::size_t size() const { return m_commands.size(); }

    void collectAssetIds(QSet<LayerId>& out) const override
    {
        for (const auto& cmd : m_commands)
            cmd->collectAssetIds(out);
    }

    void redo() override
    {
        for (auto& cmd : m_commands)
            cmd->redo();
    }

    void undo() override
    {
        for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it)
            (*it)->undo();
    }

private:
    std::vector<std::unique_ptr<Command>> m_commands;
};

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

/// Groups multiple layers into a single GroupLayer with full undo/redo support.
class GroupLayersCommand final : public Command
{
public:
    GroupLayersCommand(Document& doc, QList<LayerId> layerIds, QString groupName = QStringLiteral("Group"))
        : Command(QStringLiteral("layer.group"))
        , m_doc(&doc)
        , m_layerIds(std::move(layerIds))
        , m_groupName(std::move(groupName))
    {
        auto group = std::make_unique<GroupLayer>();
        group->name = m_groupName;
        m_groupId = group->id();
        m_groupHolder = std::move(group);

        // A2: os ids chegam em ordem de CLIQUE, não de empilhamento.
        // Guardamos as infos e as ordenamos ASC por (pai, índice) para
        // (1) os filhos entrarem no grupo mantendo a ordem relativa e
        // (2) o undo restaurar índices sem deslocar os ainda pendentes.
        for (const auto& id : m_layerIds) {
            LayerInfo info;
            info.id = id;
            if (GroupLayer* parent = m_doc->parentOf(id)) {
                info.parentId = parent->id();
                info.index = m_doc->indexOf(id);
            } else {
                info.parentId = m_doc->rootGroup()->id();
                info.index = -1;
            }
            m_layerInfos.push_back(info);
        }
        std::sort(m_layerInfos.begin(), m_layerInfos.end(),
                  [](const LayerInfo& a, const LayerInfo& b) {
                      if (a.parentId != b.parentId)
                          return a.parentId < b.parentId;
                      return a.index < b.index;
                  });

        // O grupo entra no pai do membro mais alto, na posição dele:
        // acima dos irmãos que sobraram abaixo dele.
        if (!m_layerInfos.empty()) {
            const LayerInfo& top = m_layerInfos.back();
            m_groupParentId = top.parentId;
            int below = 0;
            for (const auto& info : m_layerInfos)
                if (info.parentId == top.parentId && info.index < top.index)
                    ++below;
            m_groupIndex = std::max(0, top.index - below);
        } else {
            m_groupParentId = m_doc->rootGroup()->id();
            m_groupIndex = -1;
        }
    }

    const LayerId& groupId() const { return m_groupId; }

    void collectAssetIds(QSet<LayerId>& out) const override
    {
        detail::collectLayerAssetIds(m_groupHolder.get(), out);
    }

    void redo() override
    {
        // Idempotente: funciona no primeiro redo E no redo-pós-undo
        // (antes, o re-redo adicionava o grupo VAZIO e perdia os filhos).
        auto* group = static_cast<GroupLayer*>(m_groupHolder.get());
        Q_ASSERT(group);
        for (const auto& info : m_layerInfos) {
            if (auto taken = m_doc->takeLayer(info.id))
                group->children.push_back(std::move(taken));
        }
        m_doc->addLayer(std::move(m_groupHolder),
                        detail::resolveGroup(m_doc, m_groupParentId),
                        m_groupIndex);
    }

    void undo() override
    {
        // Take the group from the document
        m_groupHolder = m_doc->takeLayer(m_groupId);
        if (!m_groupHolder)
            return;
        auto* group = static_cast<GroupLayer*>(m_groupHolder.get());

        // Restore child layers back to original positions in the document,
        // em ordem ASC de índice — inserir em ordem decrescente/click
        // corrompia o empilhamento dos irmãos (swap permanente de camadas).
        for (const auto& info : m_layerInfos) {
            auto& children = group->children;
            auto it = std::find_if(children.begin(), children.end(),
                                   [&](const auto& c) { return c->id() == info.id; });
            if (it != children.end()) {
                auto child = std::move(*it);
                children.erase(it);
                GroupLayer* targetParent = detail::resolveGroup(m_doc, info.parentId);
                m_doc->addLayer(std::move(child), targetParent, info.index);
            }
        }
    }

private:
    struct LayerInfo {
        LayerId id;
        LayerId parentId;
        int index = -1;
    };

    Document* m_doc;
    QList<LayerId> m_layerIds;
    QString m_groupName;
    LayerId m_groupId;
    LayerId m_groupParentId;
    int m_groupIndex = -1;
    std::vector<LayerInfo> m_layerInfos; // ordenado ASC (pai, índice)
    std::unique_ptr<Layer> m_groupHolder;
};

/// Ungroups a group without rasterizing its contents. The children return to
/// the group's parent at precisely the former group position, so undo/redo
/// remains predictable.
class UngroupLayersCommand final : public Command
{
public:
    UngroupLayersCommand(Document& doc, const LayerId& groupId)
        : Command(QStringLiteral("layer.ungroup"))
        , m_doc(&doc)
        , m_groupId(groupId)
    {
        // B3: guard em vez de Q_ASSERT (some em release).
        Layer* layer = doc.findLayer(groupId);
        m_valid = (layer && layer->type() == LayerType::Group);
        if (!m_valid)
            return;
        GroupLayer* parent = doc.parentOf(groupId);
        m_parentId = parent ? parent->id() : doc.rootGroup()->id();
        m_groupIndex = doc.indexOf(groupId);
        const auto* group = static_cast<const GroupLayer*>(layer);
        for (const auto& child : group->children)
            m_childIds.append(child->id());
    }

    void redo() override
    {
        if (!m_valid)
            return;
        m_groupHolder = m_doc->takeLayer(m_groupId);
        if (!m_groupHolder)
            return;
        auto* group = static_cast<GroupLayer*>(m_groupHolder.get());
        GroupLayer* parent = detail::resolveGroup(m_doc, m_parentId);
        int insertion = m_groupIndex;
        while (!group->children.empty()) {
            auto child = std::move(group->children.front());
            group->children.erase(group->children.begin());
            m_doc->addLayer(std::move(child), parent, insertion++);
        }
    }

    void undo() override
    {
        if (!m_valid || !m_groupHolder)
            return;
        auto* group = static_cast<GroupLayer*>(m_groupHolder.get());
        for (const LayerId& childId : m_childIds) {
            if (auto child = m_doc->takeLayer(childId))
                group->children.push_back(std::move(child));
        }
        m_doc->addLayer(std::move(m_groupHolder), detail::resolveGroup(m_doc, m_parentId), m_groupIndex);
    }

    void collectAssetIds(QSet<LayerId>& out) const override
    {
        detail::collectLayerAssetIds(m_groupHolder.get(), out);
    }

private:
    Document* m_doc;
    LayerId m_groupId;
    LayerId m_parentId;
    int m_groupIndex = -1;
    QList<LayerId> m_childIds;
    std::unique_ptr<Layer> m_groupHolder;
    bool m_valid = false;
};

} // namespace cc
