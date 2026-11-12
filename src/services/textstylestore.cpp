#include "textstylestore.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

namespace cc {

namespace {

constexpr auto kUserStylesFile = "text_styles.json";

// ---------------------------------------------------------------------------
// TextEffects <-> JSON
// ---------------------------------------------------------------------------

QJsonObject outlineToJson(const TextOutline& o)
{
    QJsonObject j;
    j.insert(QStringLiteral("enabled"), o.enabled);
    j.insert(QStringLiteral("color"), o.color.name(QColor::HexArgb));
    j.insert(QStringLiteral("width"), o.width);
    return j;
}

QJsonObject shadowToJson(const TextShadow& s)
{
    QJsonObject j;
    j.insert(QStringLiteral("enabled"), s.enabled);
    j.insert(QStringLiteral("color"), s.color.name(QColor::HexArgb));
    j.insert(QStringLiteral("offsetX"), s.offsetX);
    j.insert(QStringLiteral("offsetY"), s.offsetY);
    j.insert(QStringLiteral("blur"), s.blur);
    return j;
}

QJsonObject gradientToJson(const TextGradient& g)
{
    QJsonObject j;
    j.insert(QStringLiteral("enabled"), g.enabled);
    j.insert(QStringLiteral("type"), g.type);
    j.insert(QStringLiteral("color1"), g.startColor.name(QColor::HexArgb));
    j.insert(QStringLiteral("color2"), g.endColor.name(QColor::HexArgb));
    j.insert(QStringLiteral("angle"), g.angleDeg);
    return j;
}

TextOutline outlineFromJson(const QJsonObject& j)
{
    TextOutline o;
    o.enabled = j.value(QStringLiteral("enabled")).toBool(o.enabled);
    o.color = QColor(j.value(QStringLiteral("color")).toString(o.color.name()));
    o.width = j.value(QStringLiteral("width")).toDouble(o.width);
    return o;
}

TextShadow shadowFromJson(const QJsonObject& j)
{
    TextShadow s;
    s.enabled = j.value(QStringLiteral("enabled")).toBool(s.enabled);
    s.color = QColor(j.value(QStringLiteral("color")).toString(s.color.name()));
    s.offsetX = j.value(QStringLiteral("offsetX")).toDouble(s.offsetX);
    s.offsetY = j.value(QStringLiteral("offsetY")).toDouble(s.offsetY);
    s.blur = j.value(QStringLiteral("blur")).toDouble(s.blur);
    return s;
}

TextGradient gradientFromJson(const QJsonObject& j)
{
    TextGradient g;
    g.enabled = j.value(QStringLiteral("enabled")).toBool(g.enabled);
    g.type = j.value(QStringLiteral("type")).toInt(0);
    g.startColor = QColor(j.value(QStringLiteral("color1")).toString(g.startColor.name()));
    g.endColor = QColor(j.value(QStringLiteral("color2")).toString(g.endColor.name()));
    g.angleDeg = j.value(QStringLiteral("angle")).toDouble(g.angleDeg);
    return g;
}

} // namespace

TextStyleStore::TextStyleStore(QString userFilePath)
    : m_userFilePath(std::move(userFilePath))
{
    reload();
}

void TextStyleStore::reload()
{
    m_styles.clear();

    // Built-ins do recurso embarcido.
    QFile builtin(QStringLiteral(":/presets/text_styles.json"));
    if (builtin.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(builtin.readAll());
        if (doc.isObject()) {
            const QJsonArray arr = doc.object().value(QStringLiteral("styles")).toArray();
            for (const auto& value : arr) {
                if (auto style = fromJson(value.toObject()))
                    m_styles.push_back(*style);
            }
        }
    }
    m_builtinCount = static_cast<int>(m_styles.size());

    // Estilos do utilizador.
    QFile user(m_userFilePath);
    if (user.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(user.readAll());
        if (doc.isObject()) {
            const QJsonArray arr = doc.object().value(QStringLiteral("styles")).toArray();
            for (const auto& value : arr) {
                if (auto style = fromJson(value.toObject()))
                    m_styles.push_back(*style);
            }
        }
    }
}

std::optional<TextStyle> TextStyleStore::fromJson(const QJsonObject& object)
{
    if (object.isEmpty())
        return std::nullopt;

    TextStyle style;
    style.name = object.value(QStringLiteral("name")).toString();
    if (style.name.isEmpty())
        return std::nullopt;
    style.fontFamily = object.value(QStringLiteral("fontFamily")).toString(style.fontFamily);
    style.sizePt = object.value(QStringLiteral("sizePt")).toDouble(style.sizePt);
    style.bold = object.value(QStringLiteral("bold")).toBool(style.bold);
    style.italic = object.value(QStringLiteral("italic")).toBool(style.italic);
    style.color = QColor(object.value(QStringLiteral("color")).toString(style.color.name()));
    style.letterSpacingPx =
        object.value(QStringLiteral("letterSpacing")).toDouble(style.letterSpacingPx);
    style.lineHeightMult = object.value(QStringLiteral("lineHeight")).toDouble(style.lineHeightMult);

    const QJsonObject fx = object.value(QStringLiteral("effects")).toObject();
    if (!fx.isEmpty()) {
        style.effects.outline = outlineFromJson(fx.value(QStringLiteral("outline")).toObject());
        style.effects.shadow = shadowFromJson(fx.value(QStringLiteral("shadow")).toObject());
        style.effects.gradient = gradientFromJson(fx.value(QStringLiteral("gradient")).toObject());
    }
    return style;
}

QJsonObject TextStyleStore::toJson(const TextStyle& style)
{
    QJsonObject j;
    j.insert(QStringLiteral("name"), style.name);
    j.insert(QStringLiteral("fontFamily"), style.fontFamily);
    j.insert(QStringLiteral("sizePt"), style.sizePt);
    j.insert(QStringLiteral("bold"), style.bold);
    j.insert(QStringLiteral("italic"), style.italic);
    j.insert(QStringLiteral("color"), style.color.name(QColor::HexArgb));
    j.insert(QStringLiteral("letterSpacing"), style.letterSpacingPx);
    j.insert(QStringLiteral("lineHeight"), style.lineHeightMult);

    QJsonObject fx;
    fx.insert(QStringLiteral("outline"), outlineToJson(style.effects.outline));
    fx.insert(QStringLiteral("shadow"), shadowToJson(style.effects.shadow));
    fx.insert(QStringLiteral("gradient"), gradientToJson(style.effects.gradient));
    j.insert(QStringLiteral("effects"), fx);
    return j;
}

bool TextStyleStore::addUserStyle(const TextStyle& style)
{
    if (style.name.trimmed().isEmpty())
        return false;
    for (const auto& existing : m_styles) {
        if (existing.name == style.name)
            return false;
    }

    // Persistir TODOS os estilos do utilizador (os built-ins vêm do recurso).
    QVector<TextStyle> userStyles;
    for (int i = m_builtinCount; i < m_styles.size(); ++i)
        userStyles.push_back(m_styles[i]);
    userStyles.push_back(style);

    QJsonArray arr;
    for (const auto& s : userStyles)
        arr.append(toJson(s));
    QJsonObject root;
    root.insert(QStringLiteral("styles"), arr);

    QSaveFile file(m_userFilePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson());
    if (!file.commit())
        return false;

    m_styles.push_back(style);
    return true;
}

bool TextStyleStore::removeUserStyle(const QString& name)
{
    const auto it = std::find_if(m_styles.begin(), m_styles.end(),
                                 [&name](const TextStyle& s) { return s.name == name; });
    if (it == m_styles.end())
        return false;
    const int index = static_cast<int>(it - m_styles.begin());
    if (index < m_builtinCount)
        return false;   // built-in

    QVector<TextStyle> userStyles;
    for (int i = m_builtinCount; i < m_styles.size(); ++i) {
        if (i == index)
            continue;
        userStyles.push_back(m_styles[i]);
    }
    QJsonArray arr;
    for (const auto& s : userStyles)
        arr.append(toJson(s));
    QJsonObject root;
    root.insert(QStringLiteral("styles"), arr);

    QSaveFile file(m_userFilePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson());
    if (!file.commit())
        return false;

    m_styles.erase(it);
    return true;
}

bool TextStyleStore::isUserStyle(const QString& name) const
{
    for (int i = m_builtinCount; i < m_styles.size(); ++i)
        if (m_styles[i].name == name)
            return true;
    return false;
}

std::optional<TextStyle> TextStyleStore::find(const QString& name) const
{
    for (const auto& style : m_styles)
        if (style.name == name)
            return style;
    return std::nullopt;
}

} // namespace cc
