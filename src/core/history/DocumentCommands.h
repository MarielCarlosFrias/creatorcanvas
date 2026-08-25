#pragma once

#include "Command.h"
#include "core/Document.h"

#include <QtGlobal>
#include <type_traits>

#include <memory>

namespace cc {

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

} // namespace cc
