#pragma once

#include "Command.h"
#include "core/Document.h"

#include <QtGlobal>
#include <type_traits>

#include <memory>

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

private:
    Document* m_doc;
    std::unique_ptr<Layer> m_layer;
    LayerId m_layerId;
    LayerId m_parentId;
    int m_index;
};

class RemoveLayerCommand final : public Command
{
public:
    explicit RemoveLayerCommand(Document& doc, const LayerId& id)
        : Command(QStringLiteral("layer.remove"))
        , m_doc(&doc)
        , m_id(id)
    {
        GroupLayer* parent = doc.parentOf(id);
        Q_ASSERT(parent);
        m_parentId = parent->id();
        m_index = doc.indexOf(id);
    }

    void redo() override
    {
        Q_ASSERT(!m_layer);
        m_layer = m_doc->takeLayer(m_id);
        Q_ASSERT(m_layer);
    }

    void undo() override
    {
        Q_ASSERT(m_layer);
        m_doc->addLayer(std::move(m_layer),
                        detail::resolveGroup(m_doc, m_parentId), m_index);
    }

private:
    Document* m_doc;
    LayerId m_id;
    LayerId m_parentId;
    int m_index;
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
        GroupLayer* oldParent = doc.parentOf(id);
        Q_ASSERT(oldParent);
        m_oldParentId = oldParent->id();
        m_oldIndex = doc.indexOf(id);
        m_newParentId = newParent ? newParent->id() : doc.rootGroup()->id();
        m_newIndex = newIndex;
    }

    void redo() override
    {
        m_doc->reorderLayer(m_id, detail::resolveGroup(m_doc, m_newParentId),
                            m_newIndex);
    }

    void undo() override
    {
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

private:
    Document* m_doc;
    LayerId m_id;
    ImageEffects m_old;
    ImageEffects m_new;
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
using LayerOpacityCommand    = LayerPropertyCommand<float>;
using LayerBlendModeCommand  = LayerPropertyCommand<BlendMode>;

// Comandos de histórico fortemente tipados para operações de formas geométricas (ShapeLayer)
// Permitem desfazer (Undo) e refazer (Redo) edições de cores, bordas e cantos arredondados
using SetShapeFillCommand         = LayerPropertyCommand<QColor>;
using SetShapeStrokeCommand       = LayerPropertyCommand<QColor>;
using SetShapeStrokeWidthCommand  = LayerPropertyCommand<double>;
using SetShapeCornerRadiusCommand = LayerPropertyCommand<double>;

// Comandos de histórico para operações de texto (TextLayer)
using SetTextFontFamilyCommand = LayerPropertyCommand<QString>;
using SetTextSizeCommand       = LayerPropertyCommand<double>;
using SetTextBoldCommand       = LayerPropertyCommand<bool>;
using SetTextItalicCommand     = LayerPropertyCommand<bool>;
using SetTextUnderlineCommand  = LayerPropertyCommand<bool>;
using SetTextColorCommand      = LayerPropertyCommand<QColor>;
using SetTextAlignCommand      = LayerPropertyCommand<TextAlignment>;

/// Comando reversível para modificações de conteúdo em camadas de imagem (corte, varinha mágica, tesoura, clone stamp).
/// Preserva o asset anterior e novo, dimensões naturais e transformações (posição/escala) para suporte completo a Desfazer/Refazer.
class ModifyImageLayerCommand final : public Command
{
public:
    ModifyImageLayerCommand(Document& doc, const LayerId& id,
                            LayerId oldAssetId, int oldWidth, int oldHeight, AffineTransform oldTransform,
                            LayerId newAssetId, int newWidth, int newHeight, AffineTransform newTransform,
                            QString commandName = QStringLiteral("image.modify"))
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
    {
    }

    void redo() override
    {
        m_doc->setImageLayerAsset(m_id, m_newAssetId, m_newWidth, m_newHeight);
        m_doc->setLayerTransform(m_id, m_newTransform);
    }

    void undo() override
    {
        m_doc->setImageLayerAsset(m_id, m_oldAssetId, m_oldWidth, m_oldHeight);
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

        // Store the original parent and index of each layer
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
    }

    const LayerId& groupId() const { return m_groupId; }

    void redo() override
    {
        if (!m_created) {
            auto* group = static_cast<GroupLayer*>(m_groupHolder.get());
            // Take layers from document and add to group children
            for (const auto& id : m_layerIds) {
                if (auto taken = m_doc->takeLayer(id))
                    group->children.push_back(std::move(taken));
            }

            m_doc->addLayer(std::move(m_groupHolder));
            m_created = true;
        } else {
            Q_ASSERT(m_groupHolder);
            m_doc->addLayer(std::move(m_groupHolder));
        }
    }

    void undo() override
    {
        // Take the group from the document
        m_groupHolder = m_doc->takeLayer(m_groupId);
        Q_ASSERT(m_groupHolder);

        auto* group = static_cast<GroupLayer*>(m_groupHolder.get());

        // Restore child layers back to original positions in the document
        // Group children are moved back to their original parent
        for (const auto& info : m_layerInfos) {
            // Find and extract child from group
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
    std::vector<LayerInfo> m_layerInfos;
    std::unique_ptr<Layer> m_groupHolder;
    bool m_created = false;
};

} // namespace cc
