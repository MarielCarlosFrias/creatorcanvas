#pragma once

#include <QSize>
#include <QString>
#include <QVector>

namespace cc {

class I18nService;

/// Um item de um pacote de recursos (fundo, textura ou modelo pronto).
struct PackItem {
    QString file;      ///< nome do ficheiro dentro da pasta do pacote
    QString resource;  ///< caminho Qt do recurso (:/packs/<id>/<file>)
    QString labelKey;  ///< chave i18n (namespace "editor")
    QString category;  ///< "background", "texture" ou "template"

    QString label(const I18nService* i18n) const;
    QSize naturalSize() const;
    bool isNull() const { return resource.isEmpty(); }
};

/// Um pacote de recursos (pasta em resources/packs/ com pack.json).
struct PackInfo {
    QString id;
    int version = 1;
    QVector<PackItem> items;
};

/// Carrega os pacotes embutidos em :/packs (recursos Qt, leitura única).
/// A enumeração é feita a partir do manifesto: adicionar ficheiros a uma pasta
/// exige apenas listá-los no .qrc e no pack.json.
class PackStore
{
public:
    /// Pacotes disponíveis (carregados na primeira chamada).
    static const QVector<PackInfo>& packs();

    /// Itens de todos os pacotes, pela ordem dos manifestos.
    static QVector<PackItem> allItems();

    /// Categorias conhecidas, para filtros de UI.
    static QStringList categories();
};

} // namespace cc
