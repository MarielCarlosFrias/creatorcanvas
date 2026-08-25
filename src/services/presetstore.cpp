#include "presetstore.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>

#include <utility>
extern int qInitResources_presets();

namespace cc {
namespace {

Q_LOGGING_CATEGORY(lcPresets, "cc.presets")

constexpr auto kBuiltinPath = ":/presets/builtin.json";

QColor colorFromString(const QString& value)
{
    const QColor color(value);
    return color.isValid() ? color : QColor(Qt::white);
}

QVector<DocumentPreset> parsePresets(const QByteArray& bytes, QString* error)
{
    QVector<DocumentPreset> out;
    QJsonParseError parseError{};
    const auto doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = parseError.errorString();
        return out;
    }

    const QJsonArray array =
        doc.object().value(QStringLiteral("presets")).toArray();
    for (const QJsonValue& value : array) {
        const QJsonObject o = value.toObject();
        DocumentPreset preset;
        preset.name = o.value(QStringLiteral("name")).toString();
        preset.spec.width = o.value(QStringLiteral("width")).toInt(0);
        preset.spec.height = o.value(QStringLiteral("height")).toInt(0);
        preset.spec.dpi = o.value(QStringLiteral("dpi")).toInt(96);
        preset.spec.transparentBackground =
            o.value(QStringLiteral("transparent")).toBool(false);
        if (!preset.spec.transparentBackground)
            preset.spec.backgroundColor = colorFromString(
                o.value(QStringLiteral("background")).toString());

        if (preset.name.isEmpty() || preset.spec.width < 1 ||
            preset.spec.height < 1)
            continue; // skip malformed entries
        out.append(preset);
    }
    return out;
}

QByteArray serializePresets(const QVector<DocumentPreset>& presets)
{
    QJsonArray array;
    for (const DocumentPreset& preset : presets) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), preset.name);
        o.insert(QStringLiteral("width"), preset.spec.width);
        o.insert(QStringLiteral("height"), preset.spec.height);
        o.insert(QStringLiteral("dpi"), preset.spec.dpi);
        if (preset.spec.transparentBackground)
            o.insert(QStringLiteral("transparent"), true);
        else
            o.insert(QStringLiteral("background"),
                     preset.spec.backgroundColor.name(QColor::HexRgb));
        array.append(o);
    }

    QJsonObject root;
    root.insert(QStringLiteral("presets"), array);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace

PresetStore::PresetStore(QString userFilePath)
    : m_userFilePath(std::move(userFilePath))
{
    reload();
}

void PresetStore::reload()
{
qInitResources_presets();   // registra :/presets/
    m_presets.clear();
    m_builtinCount = 0;

    QFile builtinFile{QLatin1String(kBuiltinPath)};
    if (builtinFile.open(QIODevice::ReadOnly)) {
        QString error;
        m_presets += parsePresets(builtinFile.readAll(), &error);
        if (!error.isEmpty())
            qCWarning(lcPresets) << "Failed to parse built-in presets:" << error;
    } else {
        qCWarning(lcPresets) << "Built-in presets not found:" << kBuiltinPath;
    }
    m_builtinCount = m_presets.size();

    QFile userFile(m_userFilePath);
    if (userFile.open(QIODevice::ReadOnly)) {
        QString error;
        m_presets += parsePresets(userFile.readAll(), &error);
        if (!error.isEmpty())
            qCWarning(lcPresets) << "Failed to parse user presets:" << error;
    }
}

bool PresetStore::isUserPreset(const QString& name) const
{
    for (int i = 0; i < m_presets.size(); ++i)
        if (m_presets[i].name == name)
            return i >= m_builtinCount;
    return false;
}

bool PresetStore::addUserPreset(const DocumentPreset& preset)
{
    if (preset.name.isEmpty())
        return false;
    for (const DocumentPreset& existing : m_presets)
        if (existing.name == preset.name)
            return false;

    QVector<DocumentPreset> userPresets;
    for (const DocumentPreset& existing : m_presets)
        if (isUserPreset(existing.name))
            userPresets.append(existing);
    userPresets.append(preset);

    QDir().mkpath(QFileInfo(m_userFilePath).absolutePath());
    QFile file(m_userFilePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    if (file.write(serializePresets(userPresets)) < 0)
        return false;
    file.close();

    m_presets.append(preset);
    return true;
}

bool PresetStore::removeUserPreset(const QString& name)
{
    if (!isUserPreset(name))
        return false;

    QVector<DocumentPreset> userPresets;
    for (const DocumentPreset& existing : m_presets)
        if (isUserPreset(existing.name) && existing.name != name)
            userPresets.append(existing);

    QFile file(m_userFilePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(serializePresets(userPresets));
    file.close();

    reload();
    return true;
}

} // namespace cc
