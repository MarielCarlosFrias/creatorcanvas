#pragma once

#include "BaseCommands.h"
#include "core/history/CommandStack.h"

#include <optional>

namespace cc {

/// Torna qualquer mudança da seleção ativa um passo de histórico
/// (Photocraft: seleção é cidadã de primeira classe — criar/limpar/inverter
/// são desfeitos com Ctrl+Z como qualquer edição).
/// A seleção pode ficar nula (nenhuma); desfazer/restaurar trata os dois
/// casos: null = clearPixelSelection, não-null = setPixelSelection.
class SetPixelSelectionCommand final : public Command
{
public:
    SetPixelSelectionCommand(Document& doc, QImage oldSelection,
                             SelectionSpace oldSpace, QImage newSelection,
                             SelectionSpace newSpace,
                             QString name = QStringLiteral("selection.set"))
        : Command(std::move(name))
        , m_doc(&doc)
        , m_old(std::move(oldSelection))
        , m_oldSpace(oldSpace)
        , m_new(std::move(newSelection))
        , m_newSpace(newSpace)
    {
    }

    void redo() override { apply(m_new, m_newSpace); }
    void undo() override { apply(m_old, m_oldSpace); }

private:
    void apply(const QImage& selection, SelectionSpace space)
    {
        if (selection.isNull())
            m_doc->clearPixelSelection();
        else
            m_doc->setPixelSelection(selection, space);
    }

    Document* m_doc;
    qint64 estimatedBytes() const override
    {
        return m_old.sizeInBytes() + m_new.sizeInBytes();
    }

    QImage m_old;
    SelectionSpace m_oldSpace;
    QImage m_new;
    SelectionSpace m_newSpace;
};

/// Helper único: aplica a nova seleção através do histórico quando
/// disponível, ou direto no documento quando não há CommandStack
/// (a edição nunca se perde — mesmo padrão de todos os outros comandos).
inline void executeSetPixelSelection(
    Document& doc, CommandStack* history, QImage newSelection,
    const QString& name = QStringLiteral("selection.set"),
    std::optional<SelectionSpace> space = std::nullopt)
{
    const QImage old = doc.pixelSelection();
    const SelectionSpace oldSpace = doc.pixelSelectionSpace();

    // A inversão é uma transformação da seleção atual, não uma seleção nova.
    // O seu comando UI legacy não passa metadados; preservar o espaço aqui
    // evita que uma camada com dimensões iguais ao canvas vire Document.
    const bool inheritsSpace = !old.isNull() && !newSelection.isNull()
        && old.size() == newSelection.size()
        && name == QStringLiteral("selection.invert");
    const SelectionSpace newSpace = space.value_or(
        inheritsSpace ? oldSpace
                      : (!newSelection.isNull()
                                 && newSelection.size() != QSize(doc.width(), doc.height())
                             ? SelectionSpace::Layer
                             : SelectionSpace::Document));

    if (history) {
        history->execute(std::make_unique<SetPixelSelectionCommand>(
            doc, old, oldSpace, std::move(newSelection), newSpace, name));
    } else {
        if (newSelection.isNull())
            doc.clearPixelSelection();
        else
            doc.setPixelSelection(newSelection, newSpace);
    }
}

} // namespace cc
