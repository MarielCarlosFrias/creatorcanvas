#include "ZipArchive.h"

#include <QDebug>
#include <QFile>
#include <QDir>

extern "C" {
#include <miniz.h>
}

#include <cstring>
#include <algorithm>

namespace cc {

ZipArchive::ZipArchive() : m_zip(nullptr), m_writing(false), m_reading(false) {}
ZipArchive::~ZipArchive() { close(); }

bool ZipArchive::openForWriting(const QString& filePath)
{
    close();
    m_zip = new mz_zip_archive();
    std::memset(m_zip, 0, sizeof(mz_zip_archive));
    m_writing = true;
    return mz_zip_writer_init_file(m_zip, filePath.toUtf8().constData(), 0) != 0;
}

bool ZipArchive::addFile(const QString& name, const QByteArray& data, bool compress)
{
    if (!m_zip || !m_writing) return false;
    int level = compress ? MZ_DEFAULT_LEVEL : 0;
    return mz_zip_writer_add_mem(m_zip, name.toUtf8().constData(),
                                 data.constData(), data.size(), level) != 0;
}

bool ZipArchive::close()
{
    if (!m_zip) return true;
    bool ok = false;
    if (m_writing) {
        ok = mz_zip_writer_finalize_archive(m_zip) != 0;
        mz_zip_writer_end(m_zip);
    } else if (m_reading) {
        mz_zip_reader_end(m_zip);
    }
    delete m_zip;
    m_zip = nullptr;
    m_writing = m_reading = false;
    return ok;
}

bool ZipArchive::openForReading(const QString& filePath)
{
    close();
    m_zip = new mz_zip_archive();
    std::memset(m_zip, 0, sizeof(mz_zip_archive));
    m_reading = true;
    if (!mz_zip_reader_init_file(m_zip, filePath.toUtf8().constData(), 0)) {
        delete m_zip;
        m_zip = nullptr;
        m_reading = false;
        return false;
    }
    int count = mz_zip_reader_get_num_files(m_zip);
    for (int i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        if (mz_zip_reader_file_stat(m_zip, i, &stat)) {
            m_fileNames << QString::fromUtf8(stat.m_filename);
        }
    }
    return true;
}

bool ZipArchive::hasFile(const QString& name) const
{
    return m_fileNames.contains(name);
}

QByteArray ZipArchive::readFile(const QString& name) const
{
    if (!m_zip || !m_reading) return QByteArray();

    int fileIndex = mz_zip_reader_locate_file(m_zip, name.toUtf8().constData(), nullptr, 0);
    if (fileIndex < 0) return QByteArray();

    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(m_zip, static_cast<mz_uint>(fileIndex), &stat))
        return QByteArray();

    constexpr mz_uint64 kMaxSafeFileSize = 64ULL * 1024ULL * 1024ULL; // 64 MB
    if (stat.m_uncomp_size > kMaxSafeFileSize)
        return QByteArray();

    size_t size = 0;
    void* data = mz_zip_reader_extract_file_to_heap(m_zip,
                                                    name.toUtf8().constData(),
                                                    &size, 0);
    if (!data) return QByteArray();
    QByteArray result(static_cast<const char*>(data), static_cast<int>(size));
    free(data);
    return result;
}

QStringList ZipArchive::fileList() const { return m_fileNames; }

void ZipArchive::closeRead() { close(); }

} // namespace cc
