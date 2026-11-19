#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QUuid>

namespace cc {

/// Immutable project snapshot metadata plus its serialized payload.
struct DocumentVersion
{
    QUuid id;
    QString name;
    QString description;
    QDateTime createdAt;
    quint64 revision = 0;
    QByteArray snapshot;
};

} // namespace cc
