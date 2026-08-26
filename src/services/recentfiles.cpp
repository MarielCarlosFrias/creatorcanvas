#include "recentfiles.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <utility>

namespace cc {

RecentFiles::RecentFiles(QString storageFilePath)
    : m_storageFilePath(std::move(storageFilePath))
{
    QFile file(m_storageFilePath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const auto doc = QJsonDocument::fromJson(file.readAll());
    for (const auto& value : doc.array())
        m_paths.append(value.toString());
}

void RecentFiles::save()
{
    QDir().mkpath(QFileInfo(m_storageFilePath).absolutePath());
    QJsonArray array;
    for (const QString& path : m_paths)
        array.append(path);
    QJsonObject root;
    root.insert(QStringLiteral("recent"), array);
    QFile file(m_storageFilePath);
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void RecentFiles::push(const QString& filePath)
{
    m_paths.removeAll(filePath);
    m_paths.prepend(filePath);
    while (m_paths.size() > kMaxEntries)
        m_paths.removeLast();
    save();
}

void RecentFiles::remove(const QString& filePath)
{
    m_paths.removeAll(filePath);
    save();
}

void RecentFiles::clear()
{
    m_paths.clear();
    save();
}

} // namespace cc
