#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

class QSettings;

namespace cc {

/// Typed wrapper over persistent application settings (INI format).
///
/// When constructed without |filePath|, QSettings resolves the platform
/// location from the organisation/application names that main() sets BEFORE
/// constructing this service. Tests inject an explicit file path.
class SettingsService final : public QObject
{
    Q_OBJECT
public:
    explicit SettingsService(const QString &filePath = {},
                             QObject *parent = nullptr);

    // General -------------------------------------------------------------
    QString language() const;              // BCP-47-ish code: "en", "pt-BR"
    void setLanguage(const QString &code); // English is the default/fallback

    int autosaveIntervalMinutes() const;   // 0 = autosave disabled; default 5
    void setAutosaveIntervalMinutes(int minutes);

    // Performance ----------------------------------------------------------
    bool gpuAccelerationEnabled() const;   // default false (software renderer)
    void setGpuAccelerationEnabled(bool enabled);

signals:
    /// Emitted once after any value actually changes.
    void changed();

private:
    QVariant value(const QString &key, const QVariant &fallback) const;
    void setValue(const QString &key, const QVariant &value);

    QSettings *m_settings = nullptr;
};

} // namespace cc
