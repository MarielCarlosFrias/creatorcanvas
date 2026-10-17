#include "CommandStack.h"

#include <QSet>

#include <iterator>
#include <utility>

namespace cc {

CommandStack::CommandStack(int limit, QObject* parent)
    : QObject(parent)
    , m_limit(qMax(1, limit))
{
}

void CommandStack::setLimit(int limit)
{
    m_limit = qMax(1, limit);
    if (m_undoStack.size() > static_cast<std::size_t>(m_limit)) {
        const auto dropCount = m_undoStack.size() - static_cast<std::size_t>(m_limit);
        std::vector<std::unique_ptr<Command>> dropped(
            std::make_move_iterator(m_undoStack.begin()),
            std::make_move_iterator(m_undoStack.begin() + dropCount));
        m_undoStack.erase(m_undoStack.begin(), m_undoStack.begin() + dropCount);
        reportOrphanedAssets(std::move(dropped));
    }
    emit stateChanged();
}

void CommandStack::execute(std::unique_ptr<Command> command)
{
    if (!command)
        return;

    const bool wasUndoable = canUndo();
    const bool wasRedoable = canRedo();

    command->redo();

    // A continuous edit should not flood the history with intermediate
    // slider values. The existing command keeps its original value and lets
    // the incoming command provide the final value.
    if (!m_undoStack.empty() && m_undoStack.back()->mergeWith(*command)) {
        reportOrphanedAssets(std::move(m_redoStack));
        emitStateChanged(wasUndoable, wasRedoable);
        return;
    }

    reportOrphanedAssets(std::move(m_redoStack));
    m_undoStack.push_back(std::move(command));
    if (m_undoStack.size() > static_cast<std::size_t>(m_limit)) {
        auto dropped = std::move(m_undoStack.front());
        m_undoStack.erase(m_undoStack.begin());
        std::vector<std::unique_ptr<Command>> vec;
        vec.push_back(std::move(dropped));
        reportOrphanedAssets(std::move(vec));
    }

    emitStateChanged(wasUndoable, wasRedoable);
}

void CommandStack::undo()
{
    if (m_undoStack.empty())
        return;

    const bool wasUndoable = canUndo();
    const bool wasRedoable = canRedo();

    auto command = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    command->undo();
    m_redoStack.push_back(std::move(command));

    emitStateChanged(wasUndoable, wasRedoable);
}

void CommandStack::redo()
{
    if (m_redoStack.empty())
        return;

    const bool wasUndoable = canUndo();
    const bool wasRedoable = canRedo();

    auto command = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    command->redo();
    m_undoStack.push_back(std::move(command));

    emitStateChanged(wasUndoable, wasRedoable);
}

void CommandStack::clear()
{
    if (m_undoStack.empty() && m_redoStack.empty())
        return;

    const bool wasUndoable = canUndo();
    const bool wasRedoable = canRedo();

    std::vector<std::unique_ptr<Command>> all;
    all.reserve(m_undoStack.size() + m_redoStack.size());
    std::move(m_undoStack.begin(), m_undoStack.end(), std::back_inserter(all));
    std::move(m_redoStack.begin(), m_redoStack.end(), std::back_inserter(all));
    m_undoStack.clear();
    m_redoStack.clear();
    reportOrphanedAssets(std::move(all));

    emitStateChanged(wasUndoable, wasRedoable);
}

QString CommandStack::nextUndoName() const
{
    return m_undoStack.empty() ? QString() : m_undoStack.back()->name();
}

QString CommandStack::nextRedoName() const
{
    return m_redoStack.empty() ? QString() : m_redoStack.back()->name();
}

QString CommandStack::commandNameAt(int index) const
{
    if (index < 0 || index >= totalCount())
        return QString();

    if (index < static_cast<int>(m_undoStack.size())) {
        return m_undoStack[index]->name();
    } else {
        // Redo stack is stored in LIFO order (m_redoStack.back() is the next redo, i.e., index == undoCount())
        int redoOffset = index - static_cast<int>(m_undoStack.size());
        int redoIdx = static_cast<int>(m_redoStack.size()) - 1 - redoOffset;
        if (redoIdx >= 0 && redoIdx < static_cast<int>(m_redoStack.size()))
            return m_redoStack[redoIdx]->name();
    }
    return QString();
}

void CommandStack::jumpToState(int targetIndex)
{
    if (targetIndex < 0)
        targetIndex = 0;
    if (targetIndex > totalCount())
        targetIndex = totalCount();

    int current = currentIndex();
    if (targetIndex == current)
        return;

    const bool wasUndoable = canUndo();
    const bool wasRedoable = canRedo();

    if (targetIndex < current) {
        int steps = current - targetIndex;
        for (int i = 0; i < steps && !m_undoStack.empty(); ++i) {
            auto command = std::move(m_undoStack.back());
            m_undoStack.pop_back();
            command->undo();
            m_redoStack.push_back(std::move(command));
        }
    } else {
        int steps = targetIndex - current;
        for (int i = 0; i < steps && !m_redoStack.empty(); ++i) {
            auto command = std::move(m_redoStack.back());
            m_redoStack.pop_back();
            command->redo();
            m_undoStack.push_back(std::move(command));
        }
    }

    emitStateChanged(wasUndoable, wasRedoable);
}

void CommandStack::reportOrphanedAssets(std::vector<std::unique_ptr<Command>> dropped)
{
    if (dropped.empty())
        return;

    QSet<QUuid> candidates;
    for (const auto& cmd : dropped)
        cmd->collectAssetIds(candidates);
    if (candidates.isEmpty())
        return;

    // Ids que comandos RESTANTES ainda mantêm vivos não são órfãos.
    QSet<QUuid> stillAlive;
    for (const auto& cmd : m_undoStack)
        cmd->collectAssetIds(stillAlive);
    for (const auto& cmd : m_redoStack)
        cmd->collectAssetIds(stillAlive);
    candidates.subtract(stillAlive);
    if (candidates.isEmpty())
        return;

    emit assetsOrphaned(candidates);
}

void CommandStack::emitStateChanged(bool beforeUndoable, bool beforeRedoable)
{
    if (beforeUndoable != canUndo())
        emit canUndoChanged(canUndo());
    if (beforeRedoable != canRedo())
        emit canRedoChanged(canRedo());
    emit stateChanged();
}

} // namespace cc
