#include "settingsservice.h"

#include <QSettings>

namespace cc {
namespace {
constexpr auto kLanguage         = "general/language";
constexpr auto kAutosaveInterval = "general/autosaveIntervalMinutes";
constexpr auto kGpuAcceleration  = "performance/gpuAcceleration";

constexpr int kDefaultAutosaveInterval = 5;
} // namespace

SettingsService::SettingsService(const QString &filePath, QObject *parent)
    : QObject(parent)
{
    m_settings = filePath.isEmpty()
        ? new QSettings(this)
        : new QSettings(filePath, QSettings::IniFormat, this);
}

QString SettingsService::language() const
{
    return value(kLanguage, QStringLiteral("en")).toString();
}

void SettingsService::setLanguage(const QString &code)
{
    setValue(kLanguage, code);
}

int SettingsService::autosaveIntervalMinutes() const
{
    bool ok = false;
    const int v = value(kAutosaveInterval, kDefaultAutosaveInterval).toInt(&ok);
    return ok ? v : kDefaultAutosaveInterval;
}

void SettingsService::setAutosaveIntervalMinutes(int minutes)
{
    setValue(kAutosaveInterval, minutes);
}

bool SettingsService::gpuAccelerationEnabled() const
{
    return value(kGpuAcceleration, false).toBool();
}

void SettingsService::setGpuAccelerationEnabled(bool enabled)
{
    setValue(kGpuAcceleration, enabled);
}

QVariant SettingsService::value(const QString &key, const QVariant &fallback) const
{
    return m_settings->value(key, fallback);
}

void SettingsService::setValue(const QString &key, const QVariant &value)
{
    if (m_settings->value(key) == value)
        return;
    m_settings->setValue(key, value);
    emit changed();
}

} // namespace cc
