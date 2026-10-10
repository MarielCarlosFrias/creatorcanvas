#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <vector>

#include "Document.h"
#include "document/NewDocumentSpec.h"

namespace cc {

/// Interface implementada por qualquer componente ou painel (CanvasView,
/// LayersPanel, TextInspector, ShapeInspector, ImageInspector, AutosaveService, etc.)
/// que precise ser notificado quando o documento ativo mudar.
class IDocumentObserver
{
public:
    virtual ~IDocumentObserver() = default;
    virtual void onDocumentChanged(Document* doc) = 0;
};

/// Controlador central responsável por gerenciar o ciclo de vida do documento
/// ativo no CreatorCanvas e garantir que todos os componentes estejam sincronizados.
class DocumentController final : public QObject
{
    Q_OBJECT
public:
    explicit DocumentController(QObject* parent = nullptr);
    ~DocumentController() override;

    DocumentController(const DocumentController&) = delete;
    DocumentController& operator=(const DocumentController&) = delete;

    Document* currentDocument() const { return m_document.get(); }
    bool hasDocument() const { return m_document != nullptr; }

    QString currentFilePath() const { return m_currentFilePath; }
    void setCurrentFilePath(const QString& path);

    bool isModified() const { return m_modified; }
    void setModified(bool modified);

    // Registro de observadores
    void registerObserver(IDocumentObserver* observer);
    void unregisterObserver(IDocumentObserver* observer);

    // Ciclo de vida centralizado do documento
    void setDocument(std::unique_ptr<Document> newDoc, const QString& filePath = QString());
    bool createNewDocument(const NewDocumentSpec& spec);
    bool openDocument(const QString& filePath, QString* errorMessage = nullptr);
    bool saveDocument(QString* errorMessage = nullptr);
    bool saveDocumentAs(const QString& filePath, QString* errorMessage = nullptr);
    void closeDocument();

signals:
    void documentChanged(Document* doc);
    void documentModifiedChanged(bool modified);
    void documentFilePathChanged(const QString& path);
    void documentSaved(const QString& path);
    void documentClosed();

private:
    void notifyObservers();
    void wireDocumentSignals();

    std::unique_ptr<Document> m_document;
    QString m_currentFilePath;
    bool m_modified = false;
    std::vector<IDocumentObserver*> m_observers;
};

} // namespace cc
