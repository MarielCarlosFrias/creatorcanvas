#include "packstore.h"

#include "localization/i18nservice.h"

#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>

// Força o linker a puxar o objeto de recurso packs.qrc da biblioteca
// estática (o mesmo truque de i18nservice.cpp para :/locales).
extern int qInitResources_packs();

namespace cc {

namespace {
// Registado na primeira utilização: PackStore::packs() chama registerPacks().
const int packsResourceRegistered = []() {
    qInitResources_packs();
    return 0;
}();
}

QString PackItem::label(const I18nService* i18n) const
{
    if (!i18n)
        return file;
    return i18n->t("editor", labelKey);
}

QSize PackItem::naturalSize() const
{
    QImageReader reader(resource);
    return reader.size();
}

const QVector<PackInfo>& PackStore::packs()
{
    static QMutex mutex;
    static QVector<PackInfo> cache;
    static bool loaded = false;

    QMutexLocker locker(&mutex);
    if (loaded)
        return cache;
    loaded = true;
    qInitResources_packs(); // idempotente: garante :/packs mesmo sem o TU

    // Um pacote = uma pasta com pack.json dentro de :/packs. A lista vem do
    // próprio sistema de recursos: QDir(":/packs") enumera as subpastas.
    QDir root(QStringLiteral(":/packs"));
    const QStringList packIds = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                              QDir::Name);
    for (const QString& id : packIds) {
        const QString manifestPath =
            QStringLiteral(":/packs/%1/pack.json").arg(id);
        QFile file(manifestPath);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        const QJsonObject obj = doc.object();
        PackInfo info;
        info.id = obj.value(QStringLiteral("id")).toString(id);
        info.version = obj.value(QStringLiteral("version")).toInt(1);

        const QJsonArray items = obj.value(QStringLiteral("items")).toArray();
        for (const QJsonValue& value : items) {
            const QJsonObject item = value.toObject();
            PackItem entry;
            entry.file = item.value(QStringLiteral("file")).toString();
            entry.labelKey = item.value(QStringLiteral("key")).toString();
            entry.category = item.value(QStringLiteral("category")).toString();
            if (entry.file.isEmpty())
                continue;
            entry.resource = QStringLiteral(":/packs/%1/%2").arg(info.id, entry.file);
            info.items.push_back(entry);
        }
        if (!info.items.isEmpty())
            cache.push_back(info);
    }
    return cache;
}

QVector<PackItem> PackStore::allItems()
{
    QVector<PackItem> all;
    for (const PackInfo& pack : packs())
        all.append(pack.items);
    return all;
}

QStringList PackStore::categories()
{
    QStringList out;
    const QVector<PackItem> items = allItems();
    QSet<QString> seen;
    for (const PackItem& item : items) {
        if (!item.category.isEmpty() && !seen.contains(item.category)) {
            seen.insert(item.category);
            out.push_back(item.category);
        }
    }
    return out;
}

} // namespace cc
