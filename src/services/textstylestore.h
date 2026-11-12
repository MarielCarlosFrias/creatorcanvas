#pragma once

#include <QString>
#include <QVector>

#include "core/layers/Layer.h"

#include <QJsonObject>

#include <optional>

namespace cc {

/// Estilo de texto guardado (subconjunto editável de um TextLayer):
/// fonte, tamanho, cor, espaçamento e efeitos (contorno, sombra, gradiente).
/// Presets são o atalho que um criador precisa: definir um estilo uma vez
/// e reaplicá-lo em segundos.
struct TextStyle
{
    QString name;

    QString fontFamily = QStringLiteral("Sans Serif");
    double sizePt = 48.0;
    bool bold = false;
    bool italic = false;
    QColor color = QColor(17, 17, 17);
    double letterSpacingPx = 0.0;
    double lineHeightMult = 1.0;

    TextEffects effects;

    bool operator==(const TextStyle& o) const { return name == o.name; }
};

/// Catálogo de estilos de texto: built-ins (recurso embarcido) + estilos do
/// utilizador persistidos num JSON no directório de configuração.
class TextStyleStore
{
public:
    explicit TextStyleStore(QString userFilePath);

    /// Built-ins primeiro (ordem estável), depois os do utilizador.
    const QVector<TextStyle>& all() const { return m_styles; }

    /// Adiciona e persiste um estilo do utilizador. Falha com nome vazio ou
    /// duplicado, ou se a escrita falhar.
    bool addUserStyle(const TextStyle& style);

    /// Remove um estilo DO UTILIZADOR por nome. Built-ins não se removem.
    bool removeUserStyle(const QString& name);

    bool isUserStyle(const QString& name) const;

    /// Estilo por nome (built-in ou utilizador), ou nullopt.
    std::optional<TextStyle> find(const QString& name) const;

private:
    void reload();
    static std::optional<TextStyle> fromJson(const QJsonObject& object);
    static QJsonObject toJson(const TextStyle& style);

    QString m_userFilePath;
    QVector<TextStyle> m_styles;
    int m_builtinCount = 0;
};

} // namespace cc
