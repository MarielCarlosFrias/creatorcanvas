#pragma once

#include "core/history/Command.h"
#include "core/Document.h"

#include <QtGlobal>
#include <QString>
#include <QSet>
#include <vector>
#include <memory>

namespace cc {

enum class LockRequirement {
    None,
    Position,
    Pixels,
    Transparency,
    All
};

inline bool isLayerLockedFor(const Document* doc, const LayerId& id, LockRequirement req)
{
    if (!doc || id.isNull() || req == LockRequirement::None)
        return false;

    const LayerLocks locks = doc->effectiveLocks(id);
    if (locks.all)
        return true;

    switch (req) {
    case LockRequirement::Position:
        return locks.position;
    case LockRequirement::Pixels:
        return locks.pixels;
    case LockRequirement::Transparency:
        return locks.transparency;
    case LockRequirement::All:
        return locks.all;
    case LockRequirement::None:
    default:
        return false;
    }
}

class LayerCommand : public Command
{
public:
    LayerCommand(QString name, Document& doc, LayerId layerId,
                 LockRequirement lockReq = LockRequirement::All)
        : Command(std::move(name))
        , m_doc(&doc)
        , m_layerId(std::move(layerId))
        , m_lockReq(lockReq)
    {
    }

    ~LayerCommand() override = default;

    Document* document() const { return m_doc; }
    const LayerId& layerId() const { return m_layerId; }
    LockRequirement lockRequirement() const { return m_lockReq; }

    virtual bool canExecute() const
    {
        if (!m_doc || m_layerId.isNull())
            return false;
        return !isLayerLockedFor(m_doc, m_layerId, m_lockReq);
    }

protected:
    Document* m_doc;
    LayerId m_layerId;
    LockRequirement m_lockReq;
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

namespace detail {

inline GroupLayer* resolveGroup(Document* doc, const LayerId& id)
{
    Layer* p = doc ? doc->findLayer(id) : nullptr;
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

} // namespace cc
