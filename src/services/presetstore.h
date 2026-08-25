#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

#include "core/document/NewDocumentSpec.h"

namespace cc {

struct DocumentPreset
{
    QString name;
    NewDocumentSpec spec;
};

/// Catalog of document presets: built-ins shipped as resources plus user
/// presets persisted to a JSON file in the config directory.
class PresetStore
{
public:
    /// |userFilePath| is where user presets persist. Built-ins load from
    /// :/presets/builtin.json.
    explicit PresetStore(QString userFilePath);

    /// Built-ins first (stable order), then user presets.
    const QVector<DocumentPreset>& all() const { return m_presets; }

    /// Adds and persists a user preset. Fails on empty/duplicate name or
    /// write failure.
    bool addUserPreset(const DocumentPreset& preset);

    /// Removes a USER preset by name. Built-ins cannot be removed.
    bool removeUserPreset(const QString& name);

    bool isUserPreset(const QString& name) const;

private:
    void reload();

    QString m_userFilePath;
    QVector<DocumentPreset> m_presets;
    int m_builtinCount = 0;
};

} // namespace cc

Q_DECLARE_METATYPE(cc::DocumentPreset)
