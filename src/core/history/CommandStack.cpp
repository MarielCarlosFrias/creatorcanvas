#include "CommandStack.h"

#include <utility>

namespace cc {

CommandStack::CommandStack(int limit, QObject* parent)
    : QObject(parent)
    , m_limit(qMax(1, limit))
{
}

void CommandStack::execute(std::unique_ptr<Command> command)
{
    if (!command)
        return;

    const bool wasUndoable = canUndo();
    const bool wasRedoable = canRedo();

    command->redo();
    m_redoStack.clear();
    m_undoStack.push_back(std::move(command));
    if (m_undoStack.size() > static_cast<std::size_t>(m_limit))
        m_undoStack.erase(m_undoStack.begin());

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

    m_undoStack.clear();
    m_redoStack.clear();

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

void CommandStack::emitStateChanged(bool beforeUndoable, bool beforeRedoable)
{
    if (beforeUndoable != canUndo())
        emit canUndoChanged(canUndo());
    if (beforeRedoable != canRedo())
        emit canRedoChanged(canRedo());
    emit stateChanged();
}

} // namespace cc
