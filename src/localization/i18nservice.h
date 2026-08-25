#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace cc {

class SettingsService;

/// Loads JSON translation catalogs and resolves user-facing strings with a
/// guaranteed fallback chain: current language -> English -> logged miss.
///
/// Catalog layout on disk (each file is one namespace):
///   <searchPath>/<langCode>/(common|editor|templates|settings).json
/// Keys may nest; they are flattened to dotted paths prefixed by namespace,
/// e.g. common.json {"menu":{"file":"File"}} -> "common.menu.file".
class I18nService final : public QObject
{
    Q_OBJECT
public:
    explicit I18nService(SettingsService *settings,
                         const QStringList &searchPaths = {QStringLiteral(":/locales")},
                         QObject *parent = nullptr);

    /// Scans search paths, loads every discovered language.
    /// Returns true iff at least English ("en") was found.
    bool loadAvailableLanguages();

    QStringList availableLanguages() const;
    QString currentLanguage() const;

    /// Name of |code| in its own language (e.g. "Português"); falls back to
    /// the English name, then to the raw code.
    QString displayName(const QString &code) const;

    /// Resolves ns.key for the current language. Positional arguments replace
    /// %1, %2, ... A total miss logs a warning and returns the dotted key.
    QString t(const QString &ns, const QString &key,
              const QVariantList &args = {}) const;

    /// Switches language, persists the choice, emits languageChanged().
    /// Returns false for unknown codes (state unchanged).
    bool setLanguage(const QString &code);

signals:
    void languageChanged(const QString &code);

private:
    QMap<QString, QString> loadCatalog(const QString &root,
                                       const QString &code) const;

    SettingsService *m_settings = nullptr;
    QStringList m_searchPaths;
    QStringList m_languages;                          // sorted, "en" first
    QString m_current = QStringLiteral("en");
    QMap<QString, QMap<QString, QString>> m_catalogs; // code -> flatKey -> text
};

} // namespace cc
