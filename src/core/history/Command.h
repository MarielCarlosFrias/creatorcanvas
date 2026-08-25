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

    const QString& name() const { return m_name; }

private:
    QString m_name;
};

} // namespace cc
