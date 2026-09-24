#pragma once

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

#include "Command.h"

namespace cc {

class CommandStack final : public QObject
{
    Q_OBJECT
public:
    explicit CommandStack(int limit = 100, QObject* parent = nullptr);

    void execute(std::unique_ptr<Command> command);

    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }

    void undo();
    void redo();
    void clear();

    int undoCount() const { return static_cast<int>(m_undoStack.size()); }
    int redoCount() const { return static_cast<int>(m_redoStack.size()); }
    int limit() const { return m_limit; }

    QString nextUndoName() const;
    QString nextRedoName() const;

    int totalCount() const { return undoCount() + redoCount(); }
    int currentIndex() const { return undoCount(); }
    QString commandNameAt(int index) const;
    void jumpToState(int targetIndex);

signals:
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);
    void stateChanged();

private:
    void emitStateChanged(bool beforeUndoable, bool beforeRedoable);

    std::vector<std::unique_ptr<Command>> m_undoStack;
    std::vector<std::unique_ptr<Command>> m_redoStack;
    int m_limit;
};

} // namespace cc
