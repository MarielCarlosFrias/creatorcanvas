#pragma once

#include "core/Document.h"
#include "core/history/CommandStack.h"

#include <QImage>
#include <QString>
#include <utility>

namespace cc {

/// Snapshot reversível de uma operação Transform Selection. Os dois espaços
/// são guardados explicitamente: inferir pelo tamanho faria uma selecção Layer
/// com as dimensões do documento mudar de espaço depois de undo/redo.
class TransformSelectionCommand final : public Command
{
public:
    TransformSelectionCommand(Document& document,
                              QImage oldSelection, SelectionSpace oldSpace,
                              QImage newSelection, SelectionSpace newSpace,
                              QString name = QStringLiteral("selection.transform"))
        : Command(std::move(name))
        , m_document(&document)
        , m_oldSelection(std::move(oldSelection))
        , m_oldSpace(oldSpace)
        , m_newSelection(std::move(newSelection))
        , m_newSpace(newSpace)
    {
    }

    void redo() override { apply(m_newSelection, m_newSpace); }
    void undo() override { apply(m_oldSelection, m_oldSpace); }

private:
    void apply(const QImage& selection, SelectionSpace space)
    {
        if (selection.isNull())
            m_document->clearPixelSelection();
        else
            m_document->setPixelSelection(selection, space);
    }

    qint64 estimatedBytes() const override
    {
        return m_oldSelection.sizeInBytes() + m_newSelection.sizeInBytes();
    }

    Document* m_document;
    QImage m_oldSelection;
    SelectionSpace m_oldSpace;
    QImage m_newSelection;
    SelectionSpace m_newSpace;
};

} // namespace cc
