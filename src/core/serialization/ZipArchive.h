#pragma once

#include <QByteArray>
#include <QString>
#include <QMap>
#include <memory>
#include <vector>

struct mz_zip_archive;

namespace cc {

class ZipArchive
{
public:
    ZipArchive();
    ~ZipArchive();

    bool openForWriting(const QString& filePath);
    bool addFile(const QString& name, const QByteArray& data, bool compress = true);
    bool close();

    bool openForReading(const QString& filePath);
    bool hasFile(const QString& name) const;
    QByteArray readFile(const QString& name) const;
    QStringList fileList() const;
    void closeRead();

private:
    mz_zip_archive* m_zip;
    bool m_writing;
    bool m_reading;
    QStringList m_fileNames;
};
} // namespace cc
