#pragma once

#include <QString>
#include <QSet>
#include <QUuid>
#include "core/layers/Layer.h"

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

    /// Coleta os ids de asset que ESTE comando mantém vivos (bitmaps que só
    /// ele pode devolver ao documento num undo/redo). Usado pelo GC: quando
    /// o comando é expulso da pilha, esses ids viram candidatos a remoção
    /// do AssetStore — se nenhuma camada nem comando restante os referir.
    virtual void collectAssetIds(QSet<LayerId>& out) const { Q_UNUSED(out); }

    // Allows rapid edits of the same property (for example a slider drag) to
    // appear as one logical history step. Commands that cannot be combined
    // simply keep the default false result.
    virtual bool mergeWith(const Command&) { return false; }

    const QString& name() const { return m_name; }

private:
    QString m_name;
};

} // namespace cc
