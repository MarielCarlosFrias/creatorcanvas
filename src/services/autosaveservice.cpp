#include "autosaveservice.h"

#include "core/Document.h"
#include "core/serialization/ProjectFile.h"
#include "services/settingsservice.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDateTime>
#include <QLoggingCategory>

#include <utility>

namespace cc {
namespace {

Q_LOGGING_CATEGORY(lcAutosave, "cc.autosave")

} // namespace

AutosaveService::AutosaveService(SettingsService* settings,
                                 QString recoveryFilePath, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_recoveryPath(std::move(recoveryFilePath))
{
    connect(&m_timer, &QTimer::timeout, this, &AutosaveService::onTimeout);
    if (m_settings)
        connect(m_settings, &SettingsService::changed,
                this, &AutosaveService::onSettingsChanged);
    updateTimer();
}

void AutosaveService::setDocument(Document* document)
{
    m_document = document;
}

bool AutosaveService::hasRecoveryFile() const
{
    return QFile::exists(m_recoveryPath);
}

void AutosaveService::discardRecovery()
{
    if (QFile::exists(m_recoveryPath))
        QFile::remove(m_recoveryPath);
    QFile::remove(m_recoveryPath + QStringLiteral(".meta"));
}

void AutosaveService::quarantineRecovery()
{
    if (!QFile::exists(m_recoveryPath))
        return;
    QFile::remove(m_recoveryPath + QStringLiteral(".corrupt"));
    if (!QFile::rename(m_recoveryPath, m_recoveryPath + QStringLiteral(".corrupt")))
        qCWarning(lcAutosave) << "Could not quarantine unreadable recovery file";
    QFile::remove(m_recoveryPath + QStringLiteral(".meta"));
}

QString AutosaveService::recoverySourcePath() const
{
    QFile f(m_recoveryPath + QStringLiteral(".meta"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    return doc.object().value(QStringLiteral("source")).toString();
}

void AutosaveService::saveNow()
{
    if (m_document.isNull())
        return;

    // Se o DocumentController indicar que o documento não foi modificado,
    // não há necessidade de reescrever o ficheiro de recuperação
    if (m_docController && !m_docController->isModified())
        return;

    QDir().mkpath(QFileInfo(m_recoveryPath).absolutePath());
    const QString tempPath = m_recoveryPath + QStringLiteral(".tmp");

    QString error;
    if (!cc::saveDocument(*m_document, tempPath, &error)) {
        qCWarning(lcAutosave) << "Autosave failed:" << error;
        return;
    }

    // Substituição segura: rename sobre destino existente é atômico em
    // POSIX. O remove+rename antigo deixava uma janela em que um crash
    // entre as duas chamadas apagava a recuperação sem deixar vestígio.
    if (QFile::exists(m_recoveryPath) && !QFile::rename(m_recoveryPath, m_recoveryPath + QStringLiteral(".bak"))) {
        qCWarning(lcAutosave) << "Autosave backup of previous recovery failed";
    }
    if (!QFile::rename(tempPath, m_recoveryPath)) {
        qCWarning(lcAutosave) << "Autosave rename failed; keeping temp file" << tempPath;
        // Mantém o .bak como cópia válida; o .tmp sobrevive para desastre.
        QFile::rename(m_recoveryPath + QStringLiteral(".bak"), m_recoveryPath);
        return;
    }
    QFile::remove(m_recoveryPath + QStringLiteral(".bak"));

    // Procedência: sidecar com o projeto de origem e o momento da gravação,
    // para o diálogo de recuperação avisar se o arquivo original é mais novo.
    {
        QJsonObject meta;
        meta.insert(QStringLiteral("source"), m_sourcePath);
        meta.insert(QStringLiteral("savedAt"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        QFile metaFile(m_recoveryPath + QStringLiteral(".meta"));
        if (metaFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
            metaFile.write(QJsonDocument(meta).toJson(QJsonDocument::Compact));
    }

    qCInfo(lcAutosave) << "Autosave written safely:" << m_recoveryPath;
}

void AutosaveService::onSettingsChanged()
{
    updateTimer();
}

void AutosaveService::updateTimer()
{
    const int minutes = m_settings ? m_settings->autosaveIntervalMinutes() : 5;
    if (minutes <= 0) {
        m_timer.stop();
        return;
    }
    // qint64 evita overflow de int para intervalos grandes vindo de um
    // settings.ini editado à mão (>35791 min estourava int).
    m_timer.start(qBound<qint64>(1, qint64(minutes) * 60 * 1000, qint64(1440) * 60 * 1000));
}

void AutosaveService::onTimeout()
{
    saveNow();
}

} // namespace cc
