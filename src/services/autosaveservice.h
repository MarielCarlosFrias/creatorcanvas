#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include "core/DocumentController.h"

namespace cc {

class Document;
class SettingsService;

/// Periodically saves the working document to a recovery file. The
/// recovery file is removed by MainWindow on successful save/open/new.
class AutosaveService final : public QObject, public IDocumentObserver
{
    Q_OBJECT
public:
    explicit AutosaveService(SettingsService* settings,
                             QString recoveryFilePath,
                             QObject* parent = nullptr);

    void onDocumentChanged(Document* doc) override { setDocument(doc); }
    void setDocument(Document* document);
    void setDocumentController(DocumentController* controller) { m_docController = controller; }
    /// Caminho do projeto atualmente aberto — gravado junto à recuperação
    /// para dar procedência ao diálogo de recuperação.
    void setSourcePath(const QString& path) { m_sourcePath = path; }
    QString recoveryFilePath() const { return m_recoveryPath; }
    /// Caminho do projeto de origem gravado no sidecar .meta da última
    /// gravação (vazio se desconhecido). Usado para avisar quando o arquivo
    /// original é mais NOVO que a recuperação.
    QString recoverySourcePath() const;

    bool hasRecoveryFile() const;
    void discardRecovery();
    /// Renomeia a recuperação ilegível para "<caminho>.corrupt" em vez de
    /// apagá-la — preserva os bytes para um resgate manual do usuário.
    void quarantineRecovery();
    void saveNow();

private slots:
    void onSettingsChanged();
    void onTimeout();

private:
    void updateTimer();

    SettingsService* m_settings = nullptr;
    QPointer<Document> m_document;
    QPointer<DocumentController> m_docController = nullptr;
    QString m_recoveryPath;
    QString m_sourcePath; ///< caminho do projeto aberto (procedência)
    QTimer m_timer;
};

} // namespace cc
