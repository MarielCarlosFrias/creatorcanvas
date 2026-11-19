#pragma once

#include <QString>

namespace cc {

/// Minimal persistence contract needed by the localization service.
/// Keeping it here avoids making localization depend on all application
/// services merely to remember the selected language.
class LanguagePreferenceStore
{
public:
    virtual ~LanguagePreferenceStore() = default;
    virtual QString language() const = 0;
    virtual void setLanguage(const QString& code) = 0;
};

} // namespace cc
