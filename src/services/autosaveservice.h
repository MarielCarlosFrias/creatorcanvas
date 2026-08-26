#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

namespace cc {

class Document;
class SettingsService;

/// Periodically saves the working document to a recovery file. The
/// recovery file is removed by MainWindow on successful save/open/new.
class AutosaveService final : public QObject
{
    Q_OBJECT
public:
    explicit AutosaveService(SettingsService* settings,
                             QString recoveryFilePath,
                             QObject* parent = nullptr);

    void setDocument(Document* document);
    QString recoveryFilePath() const { return m_recoveryPath; }

    bool hasRecoveryFile() const;
    void discardRecovery();
    void saveNow();

private slots:
    void onSettingsChanged();
    void onTimeout();

private:
    void updateTimer();

    SettingsService* m_settings = nullptr;
    QPointer<Document> m_document;
    QString m_recoveryPath;
    QTimer m_timer;
};

} // namespace cc
