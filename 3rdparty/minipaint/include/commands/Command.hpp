#pragma once

namespace minipaint
{
/// Command interface used by the undo/redo history.
class Command
{
public:
    virtual ~Command() = default;

    virtual void execute() = 0;
    virtual void undo() = 0;
};

} // namespace minipaint
