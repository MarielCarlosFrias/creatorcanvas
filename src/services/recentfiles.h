#pragma once
#include <QString>
#include <QStringList>

namespace cc {

class RecentFiles
{
public:
    explicit RecentFiles(QString storageFilePath);
    QStringList paths() const { return m_paths; }
    void push(const QString& filePath);
    void remove(const QString& filePath);
    void clear();

private:
    void save();
    QString m_storageFilePath;
    QStringList m_paths;
    static constexpr int kMaxEntries = 8;
};

} // namespace cc
