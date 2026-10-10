#include "autosaveservice.h"

#include "core/Document.h"
#include "core/serialization/ProjectFile.h"
#include "services/settingsservice.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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

    // Atomic replacement para nunca corromper a última versão válida
    if (QFile::exists(m_recoveryPath)) {
        QFile::remove(m_recoveryPath);
    }
    if (!QFile::rename(tempPath, m_recoveryPath)) {
        qCWarning(lcAutosave) << "Autosave atomic rename failed from" << tempPath << "to" << m_recoveryPath;
        return;
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
    m_timer.start(minutes * 60 * 1000);
}

void AutosaveService::onTimeout()
{
    saveNow();
}

} // namespace cc
