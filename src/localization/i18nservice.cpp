#include "i18nservice.h"

#include "services/settingsservice.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLoggingCategory>

#include <algorithm>
#include <utility>
// Força o linker a puxar o objeto de recurso locales.qrc da biblioteca estática.
extern int qInitResources_locales();

namespace cc {
namespace {

Q_LOGGING_CATEGORY(lcI18n, "cc.i18n")

constexpr auto kNamespaces = {
    "common",
    "editor",
    "templates",
    "settings"
};

void flattenObject(const QJsonObject &obj,
                   const QString &prefix,
                   QMap<QString, QString> &out)
{
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QString key = prefix.isEmpty()
            ? it.key()
            : prefix + QLatin1Char('.') + it.key();

        const QJsonValue value = it.value();

        if (value.isObject()) {
            flattenObject(value.toObject(), key, out);
        } else if (value.isArray()) {
            qCWarning(lcI18n)
                << "Arrays are not supported in catalogs at:" << key;
        } else {
            out.insert(key, value.toVariant().toString());
        }
    }
}

bool readJsonObject(const QString &path, QJsonObject *out)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError error{};
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &error);

    if (error.error != QJsonParseError::NoError ||
        !document.isObject()) {
        qCWarning(lcI18n)
            << "Failed to parse catalog:" << path
            << error.errorString();
        return false;
    }

    *out = document.object();
    return true;
}

} // namespace

I18nService::I18nService(SettingsService *settings,
                         const QStringList &searchPaths,
                         QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_searchPaths(searchPaths)
{
}

bool I18nService::loadAvailableLanguages()
{
qInitResources_locales();   // registra :/locales/ (símbolo global)
    m_catalogs.clear();
    m_languages.clear();

    for (const QString &root : std::as_const(m_searchPaths)) {
        const QDir directory(root);

        if (!directory.exists()) {
            qCWarning(lcI18n)
                << "Search path does not exist:" << root;
            continue;
        }

        const QStringList codes = directory.entryList(
            QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::Name);

        for (const QString &code : codes) {
            if (m_catalogs.contains(code))
                continue;

            auto catalog = loadCatalog(root, code);

            // A language is valid when at least one namespace file
            // exists, even if that JSON file contains an empty object.
            bool hasCatalog = false;

            for (const auto ns : kNamespaces) {
                const QString path =
                    QStringLiteral("%1/%2/%3.json")
                        .arg(root, code, ns);

                if (QFile::exists(path)) {
                    hasCatalog = true;
                    break;
                }
            }

            if (hasCatalog)
                m_catalogs.insert(code, std::move(catalog));
        }
    }

    m_languages = m_catalogs.keys();

    std::sort(
        m_languages.begin(),
        m_languages.end(),
        [](const QString &a, const QString &b) {
            if (a == QLatin1String("en"))
                return true;

            if (b == QLatin1String("en"))
                return false;

            return a < b;
        });

    if (!m_languages.contains(QLatin1String("en"))) {
        qCWarning(lcI18n)
            << "English catalog not found in any search path;"
            << " UI strings will fall back to raw keys.";
        return false;
    }

    QString wanted =
        m_settings
            ? m_settings->language()
            : QStringLiteral("en");

    if (!m_languages.contains(wanted))
        wanted = QStringLiteral("en");

    m_current = wanted;

    return true;
}

QMap<QString, QString> I18nService::loadCatalog(
    const QString &root,
    const QString &code) const
{
    QMap<QString, QString> out;

    for (const auto ns : kNamespaces) {
        QJsonObject object;

        const QString path =
            QStringLiteral("%1/%2/%3.json")
                .arg(root, code, ns);

        if (!readJsonObject(path, &object))
            continue;

        flattenObject(
            object,
            QLatin1String(ns),
            out);
    }

    return out;
}

QStringList I18nService::availableLanguages() const
{
    return m_languages;
}

QString I18nService::currentLanguage() const
{
    return m_current;
}

QString I18nService::displayName(const QString &code) const
{
    const auto it = m_catalogs.constFind(code);

    if (it != m_catalogs.cend()) {
        const QString native =
            it->value(QStringLiteral("common._meta.nativeName"));

        if (!native.isEmpty())
            return native;

        const QString english =
            it->value(QStringLiteral("common._meta.englishName"));

        if (!english.isEmpty())
            return english;
    }

    return code;
}

QString I18nService::t(const QString &ns,
                       const QString &key,
                       const QVariantList &args) const
{
    const QString full =
        ns + QLatin1Char('.') + key;

    QString text;

    const auto current =
        m_catalogs.constFind(m_current);

    if (current != m_catalogs.cend() &&
        current->contains(full)) {
        text = current->value(full);
    } else {
        const auto english =
            m_catalogs.constFind(QLatin1String("en"));

        if (english != m_catalogs.cend() &&
            english->contains(full)) {
            text = english->value(full);
        } else {
            qCWarning(lcI18n)
                << "Missing translation:" << full;

            text = full;
        }
    }

    for (const QVariant &arg : args)
        text = text.arg(arg.toString());

    return text;
}

bool I18nService::setLanguage(const QString &code)
{
    if (!m_languages.contains(code)) {
        qCWarning(lcI18n)
            << "Unknown language requested:" << code;
        return false;
    }

    if (code == m_current)
        return true;

    m_current = code;

    if (m_settings)
        m_settings->setLanguage(code);

    emit languageChanged(code);

    return true;
}

} // namespace cc
