#pragma once

#include "BaseCommands.h"
#include <algorithm>

namespace cc {

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
        if (m_doc->effectiveLocks(m_parentId).all)
            return;
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

class RemoveLayerCommand final : public LayerCommand
{
public:
    explicit RemoveLayerCommand(Document& doc, const LayerId& id)
        : LayerCommand(QStringLiteral("layer.remove"), doc, id, LockRequirement::All)
    {
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
        if (!m_valid || !canExecute())
            return;
        Q_ASSERT(!m_layer);
        m_layer = m_doc->takeLayer(m_layerId);
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
    LayerId m_parentId;
    int m_index = -1;
    bool m_valid = false;
    std::unique_ptr<Layer> m_layer;
};

class ReorderLayerCommand final : public LayerCommand
{
public:
    ReorderLayerCommand(Document& doc, const LayerId& id,
                        GroupLayer* newParent, int newIndex)
        : LayerCommand(QStringLiteral("layer.reorder"), doc, id, LockRequirement::All)
    {
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
        if (m_valid && canExecute())
            m_doc->reorderLayer(m_layerId, detail::resolveGroup(m_doc, m_newParentId),
                                m_newIndex);
    }

    void undo() override
    {
        if (m_valid && canExecute())
            m_doc->reorderLayer(m_layerId, detail::resolveGroup(m_doc, m_oldParentId),
                                m_oldIndex);
    }

private:
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
            // Photocraft: camada com pixels trancados não pode ser duplicada
            if (m_doc->effectiveLocks(m_originalId).pixels || m_doc->effectiveLocks(m_originalId).all)
                return;
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
        // Se qualquer membro estiver bloqueado (pixels ou all), aborta a operação de agrupamento
        for (const auto& id : m_layerIds) {
            const auto locks = m_doc->effectiveLocks(id);
            if (locks.pixels || locks.all)
                return;
        }

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
        m_groupHolder = m_doc->takeLayer(m_groupId);
        if (!m_groupHolder)
            return;
        auto* group = static_cast<GroupLayer*>(m_groupHolder.get());

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
    std::vector<LayerInfo> m_layerInfos;
    std::unique_ptr<Layer> m_groupHolder;
};

/// Ungroups a group without rasterizing its contents.
class UngroupLayersCommand final : public Command
{
public:
    UngroupLayersCommand(Document& doc, const LayerId& groupId)
        : Command(QStringLiteral("layer.ungroup"))
        , m_doc(&doc)
        , m_groupId(groupId)
    {
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
        if (m_doc->effectiveLocks(m_groupId).all)
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
        if (m_doc->effectiveLocks(m_textId).all || m_doc->effectiveLocks(m_subjectId).all)
            return;

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

} // namespace cc
