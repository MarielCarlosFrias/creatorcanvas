#include "DocumentController.h"

#include <algorithm>
#include <QPainter>
#include "serialization/ProjectFile.h"

namespace cc {

DocumentController::DocumentController(QObject* parent)
    : QObject(parent)
{
}

DocumentController::~DocumentController() = default;

void DocumentController::setCurrentFilePath(const QString& path)
{
    if (m_currentFilePath != path) {
        m_currentFilePath = path;
        emit documentFilePathChanged(m_currentFilePath);
    }
}

void DocumentController::setModified(bool modified)
{
    if (m_modified != modified) {
        m_modified = modified;
        emit documentModifiedChanged(m_modified);
    }
}

void DocumentController::registerObserver(IDocumentObserver* observer)
{
    if (!observer) return;
    if (std::find(m_observers.begin(), m_observers.end(), observer) == m_observers.end()) {
        m_observers.push_back(observer);
        // Atualiza imediatamente o observador com o documento atual
        observer->onDocumentChanged(m_document.get());
    }
}

void DocumentController::unregisterObserver(IDocumentObserver* observer)
{
    auto it = std::remove(m_observers.begin(), m_observers.end(), observer);
    m_observers.erase(it, m_observers.end());
}

void DocumentController::notifyObservers()
{
    for (auto* obs : m_observers) {
        if (obs) {
            obs->onDocumentChanged(m_document.get());
        }
    }
    emit documentChanged(m_document.get());
}

void DocumentController::wireDocumentSignals()
{
    if (!m_document) return;

    connect(m_document.get(), &Document::structureChanged, this, [this]() {
        setModified(true);
    });
    connect(m_document.get(), &Document::layerPropertyChanged, this, [this]() {
        setModified(true);
    });
}

void DocumentController::setDocument(std::unique_ptr<Document> newDoc, const QString& filePath)
{
    m_document = std::move(newDoc);
    m_currentFilePath = filePath;
    m_modified = false;

    wireDocumentSignals();
    notifyObservers();

    emit documentFilePathChanged(m_currentFilePath);
    emit documentModifiedChanged(m_modified);
}

bool DocumentController::createNewDocument(const NewDocumentSpec& spec)
{
    auto doc = createDocument(spec);
    if (!doc) return false;

    setDocument(std::move(doc), QString());
    return true;
}

bool DocumentController::openDocument(const QString& filePath, QString* errorMessage)
{
    auto doc = cc::loadDocument(filePath, errorMessage);
    if (!doc) return false;

    setDocument(std::move(doc), filePath);
    return true;
}

bool DocumentController::saveDocument(QString* errorMessage)
{
    if (m_currentFilePath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No file path specified for saving");
        }
        return false;
    }
    return saveDocumentAs(m_currentFilePath, errorMessage);
}

bool DocumentController::saveDocumentAs(const QString& filePath, QString* errorMessage)
{
    if (!m_document) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No active document to save");
        }
        return false;
    }

    const bool ok = cc::saveDocument(*m_document, filePath, errorMessage);
    if (ok) {
        m_currentFilePath = filePath;
        setModified(false);
        emit documentFilePathChanged(m_currentFilePath);
        emit documentSaved(m_currentFilePath);
    }
    return ok;
}

void DocumentController::closeDocument()
{
    m_document.reset();
    m_currentFilePath.clear();
    setModified(false);

    notifyObservers();
    emit documentClosed();
}

} // namespace cc
