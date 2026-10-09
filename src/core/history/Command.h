#pragma once

#include <QString>

namespace cc {

class Command
{
public:
    explicit Command(QString name);
    virtual ~Command();

    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;

    virtual void redo() = 0;
    virtual void undo() = 0;

    // Allows rapid edits of the same property (for example a slider drag) to
    // appear as one logical history step. Commands that cannot be combined
    // simply keep the default false result.
    virtual bool mergeWith(const Command&) { return false; }

    const QString& name() const { return m_name; }

private:
    QString m_name;
};

} // namespace cc
