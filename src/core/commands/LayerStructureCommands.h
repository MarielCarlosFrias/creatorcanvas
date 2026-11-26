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
        Layer* layer = doc.findLayer(id);
        GroupLayer* oldParent = doc.parentOf(id);
        GroupLayer* destination = newParent ? newParent : doc.rootGroup();
        m_valid = layer && layer != doc.rootGroup() && oldParent && destination
                  && doc.findLayer(destination->id()) == destination;
        if (!m_valid)
            return;

        // Mover um grupo para dentro de si próprio é inválido e, sobretudo,
        // não pode alterar o transform antes de Document::reorderLayer falhar.
        for (GroupLayer* p = destination; p; p = doc.parentOf(p->id())) {
            if (p == layer) {
                m_valid = false;
                return;
            }
        }

        m_oldParentId = oldParent->id();
        m_oldIndex = doc.indexOf(id);
        m_newParentId = destination->id();
        m_newIndex = newIndex;
        m_movedOldTransform = layer->transform;
        m_changesParent = oldParent != destination;

        if (m_changesParent) {
            const QTransform oldToDocument =
                detail::parentToDocumentMatrix(&doc, oldParent);
            const QTransform newToDocument =
                detail::parentToDocumentMatrix(&doc, destination);
            bool invertible = false;
            const QTransform documentToNew = newToDocument.inverted(&invertible);
            m_valid = invertible;
            if (!m_valid)
                return;
            m_parentSpaceChange = oldToDocument * documentToNew;
            detail::captureGroupAndAncestors(&doc, oldParent, m_parentStates);
            detail::captureGroupAndAncestors(&doc, destination, m_parentStates);
        }
    }

    void redo() override
    {
        if (!canMove(m_oldParentId, m_newParentId))
            return;

        Layer* layer = m_doc->findLayer(m_layerId);
        const AffineTransform newTransform = m_changesParent
            ? detail::rebaseTransform(m_parentSpaceChange, *layer)
            : layer->transform;

        // Toda a validação e todo o cálculo falível acontecem antes da primeira
        // mutação. Se reorderLayer recusar, nem o filho nem os grupos mudam.
        if (!m_doc->reorderLayer(m_layerId,
                                 detail::resolveGroup(m_doc, m_newParentId),
                                 m_newIndex))
            return;
        if (m_changesParent) {
            layer->transform = newTransform;
            detail::preserveGroupMatrices(m_doc, m_parentStates);
        }
    }

    void undo() override
    {
        if (!canMove(m_newParentId, m_oldParentId))
            return;
        if (!m_doc->reorderLayer(m_layerId,
                                 detail::resolveGroup(m_doc, m_oldParentId),
                                 m_oldIndex))
            return;
        if (m_changesParent) {
            if (Layer* layer = m_doc->findLayer(m_layerId))
                layer->transform = m_movedOldTransform;
            detail::restoreGroupTransforms(m_doc, m_parentStates);
        }
    }

private:
    bool canMove(const LayerId& expectedParentId,
                 const LayerId& destinationId) const
    {
        if (!m_valid || !canExecute())
            return false;
        Layer* layer = m_doc->findLayer(m_layerId);
        GroupLayer* currentParent = m_doc->parentOf(m_layerId);
        GroupLayer* destination = detail::resolveGroup(m_doc, destinationId);
        if (!layer || layer == m_doc->rootGroup() || !currentParent
            || currentParent->id() != expectedParentId || !destination
            || m_doc->effectiveLocks(destination->id()).all)
            return false;
        for (GroupLayer* p = destination; p; p = m_doc->parentOf(p->id()))
            if (p == layer)
                return false;
        return true;
    }

    LayerId m_oldParentId;
    int m_oldIndex = -1;
    LayerId m_newParentId;
    int m_newIndex = -1;
    bool m_valid = false;
    bool m_changesParent = false;
    AffineTransform m_movedOldTransform;
    QTransform m_parentSpaceChange;
    std::vector<detail::GroupGeometryState> m_parentStates;
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
            if (const Layer* layer = m_doc->findLayer(id))
                info.oldTransform = layer->transform;
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

        GroupLayer* destination = detail::resolveGroup(m_doc, m_groupParentId);
        const QTransform destinationToDocument =
            detail::parentToDocumentMatrix(m_doc, destination);
        bool invertible = false;
        const QTransform documentToDestination =
            destinationToDocument.inverted(&invertible);
        m_valid = destination && invertible;
        if (!m_valid)
            return;

        // O novo grupo nasce com matriz identidade no pai de destino. Cada
        // filho é convertido do seu pai original para esse mesmo espaço; isto
        // é indispensável quando a seleção vem de G1, G2 e/ou da raiz.
        for (auto& info : m_layerInfos) {
            GroupLayer* source = detail::resolveGroup(m_doc, info.parentId);
            if (!source) {
                m_valid = false;
                return;
            }
            info.parentSpaceChange = detail::parentToDocumentMatrix(m_doc, source)
                * documentToDestination;
            detail::captureGroupAndAncestors(m_doc, source, m_parentStates);
        }
        detail::captureGroupAndAncestors(m_doc, destination, m_parentStates);
    }

    const LayerId& groupId() const { return m_groupId; }

    void collectAssetIds(QSet<LayerId>& out) const override
    {
        detail::collectLayerAssetIds(m_groupHolder.get(), out);
    }

    void redo() override
    {
        auto* group = static_cast<GroupLayer*>(m_groupHolder.get());
        GroupLayer* destination = detail::resolveGroup(m_doc, m_groupParentId);
        if (!m_valid || !group || !destination)
            return;

        QSet<LayerId> selected;
        for (const auto& info : m_layerInfos)
            selected.insert(info.id);
        if (selected.size() != m_layerIds.size())
            return;

        QHash<LayerId, AffineTransform> rebasedTransforms;
        for (const auto& info : m_layerInfos) {
            Layer* layer = m_doc->findLayer(info.id);
            GroupLayer* parent = m_doc->parentOf(info.id);
            if (!layer || layer == m_doc->rootGroup() || !parent
                || parent->id() != info.parentId)
                return;
            const auto locks = m_doc->effectiveLocks(info.id);
            if (locks.pixels || locks.all)
                return;

            // O comando direto também recusa seleções ancestral+descendente;
            // o serviço normalmente já as filtra, mas não deve haver remoções
            // parciais se for chamado fora desse caminho.
            for (GroupLayer* p = parent; p; p = m_doc->parentOf(p->id()))
                if (selected.contains(p->id()))
                    return;
            for (GroupLayer* p = destination; p; p = m_doc->parentOf(p->id()))
                if (p == layer)
                    return;
            rebasedTransforms.insert(
                info.id, detail::rebaseTransform(info.parentSpaceChange, *layer));
        }

        for (const auto& info : m_layerInfos) {
            auto taken = m_doc->takeLayer(info.id);
            if (!taken)
                return;
            taken->transform = rebasedTransforms.value(info.id);
            group->children.push_back(std::move(taken));
        }
        group->transform = AffineTransform{};
        detail::recenterGroupTransform(group);
        if (!m_doc->addLayer(std::move(m_groupHolder), destination, m_groupIndex))
            return;
        detail::preserveGroupMatrices(m_doc, m_parentStates);
    }

    void undo() override
    {
        if (!m_valid || m_groupHolder)
            return;
        Layer* layer = m_doc->findLayer(m_groupId);
        if (!layer || layer->type() != LayerType::Group
            || !m_doc->parentOf(m_groupId)
            || m_doc->parentOf(m_groupId)->id() != m_groupParentId)
            return;
        auto* group = static_cast<GroupLayer*>(layer);
        for (const auto& info : m_layerInfos) {
            const auto it = std::find_if(
                group->children.begin(), group->children.end(),
                [&](const auto& child) { return child->id() == info.id; });
            if (it == group->children.end()
                || !detail::resolveGroup(m_doc, info.parentId))
                return;
        }

        m_groupHolder = m_doc->takeLayer(m_groupId);
        if (!m_groupHolder)
            return;
        group = static_cast<GroupLayer*>(m_groupHolder.get());
        for (const auto& info : m_layerInfos) {
            auto& children = group->children;
            auto it = std::find_if(children.begin(), children.end(),
                                   [&](const auto& child) { return child->id() == info.id; });
            auto child = std::move(*it);
            children.erase(it);
            child->transform = info.oldTransform;
            m_doc->addLayer(std::move(child),
                            detail::resolveGroup(m_doc, info.parentId), info.index);
        }
        detail::restoreGroupTransforms(m_doc, m_parentStates);
    }

private:
    struct LayerInfo {
        LayerId id;
        LayerId parentId;
        int index = -1;
        AffineTransform oldTransform;
        QTransform parentSpaceChange;
    };

    Document* m_doc;
    QList<LayerId> m_layerIds;
    QString m_groupName;
    LayerId m_groupId;
    LayerId m_groupParentId;
    int m_groupIndex = -1;
    bool m_valid = false;
    std::vector<LayerInfo> m_layerInfos;
    std::vector<detail::GroupGeometryState> m_parentStates;
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
        auto* group = static_cast<GroupLayer*>(layer);
        m_groupOldTransform = group->transform;
        m_groupMatrixBefore = group->transform.matrix(group->contentBounds());
        detail::captureGroupAndAncestors(&doc, parent, m_parentStates);
        for (const auto& child : group->children) {
            m_childIds.append(child->id());
            m_childOldTransforms.insert(child->id(), child->transform);
        }
    }

    void redo() override
    {
        if (!m_valid || m_groupHolder)
            return;
        GroupLayer* parent = detail::resolveGroup(m_doc, m_parentId);
        Layer* current = m_doc->findLayer(m_groupId);
        if (!parent || !current || current->type() != LayerType::Group
            || m_doc->parentOf(m_groupId) != parent
            || m_doc->effectiveLocks(m_groupId).all)
            return;

        m_groupHolder = m_doc->takeLayer(m_groupId);
        if (!m_groupHolder)
            return;
        auto* group = static_cast<GroupLayer*>(m_groupHolder.get());
        // Ao sair do grupo, o transform de cada filho passa do espaço local do
        // grupo para o espaço do pai: filho_pai = filho_local · G (ordem do
        // QPainter). A decomposição exata conserva flip/shear e rotação.
        const QTransform g = m_groupMatrixBefore;
        int insertion = m_groupIndex;
        while (!group->children.empty()) {
            auto child = std::move(group->children.front());
            group->children.erase(group->children.begin());
            if (child)
                child->transform = detail::rebaseTransform(g, *child);
            m_doc->addLayer(std::move(child), parent, insertion++);
        }
        detail::preserveGroupMatrices(m_doc, m_parentStates);
    }

    void undo() override
    {
        if (!m_valid || !m_groupHolder)
            return;
        GroupLayer* parent = detail::resolveGroup(m_doc, m_parentId);
        if (!parent)
            return;
        for (const LayerId& childId : m_childIds)
            if (m_doc->parentOf(childId) != parent)
                return;

        auto* group = static_cast<GroupLayer*>(m_groupHolder.get());
        group->transform = m_groupOldTransform;
        for (const LayerId& childId : m_childIds) {
            auto child = m_doc->takeLayer(childId);
            if (!child)
                return;
            auto it = m_childOldTransforms.constFind(childId);
            if (it != m_childOldTransforms.constEnd())
                child->transform = it.value();
            group->children.push_back(std::move(child));
        }
        if (!m_doc->addLayer(std::move(m_groupHolder), parent, m_groupIndex))
            return;
        detail::restoreGroupTransforms(m_doc, m_parentStates);
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
    QHash<LayerId, AffineTransform> m_childOldTransforms;
    AffineTransform m_groupOldTransform;
    QTransform m_groupMatrixBefore;
    std::vector<detail::GroupGeometryState> m_parentStates;
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
