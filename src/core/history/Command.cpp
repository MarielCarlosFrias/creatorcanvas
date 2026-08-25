#include "Command.h"

#include <utility>

namespace cc {

Command::Command(QString name)
    : m_name(std::move(name)) {}

Command::~Command() = default;

} // namespace cc
