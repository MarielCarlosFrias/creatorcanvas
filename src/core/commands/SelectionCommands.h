#pragma once

#include "BaseCommands.h"
#include "core/history/CommandStack.h"

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
                             QImage newSelection,
                             QString name = QStringLiteral("selection.set"))
        : Command(std::move(name))
        , m_doc(&doc)
        , m_old(std::move(oldSelection))
        , m_new(std::move(newSelection))
    {
    }

    void redo() override { apply(m_new); }
    void undo() override { apply(m_old); }

private:
    void apply(const QImage& selection)
    {
        if (selection.isNull())
            m_doc->clearPixelSelection();
        else
            m_doc->setPixelSelection(selection);
    }

    Document* m_doc;
    QImage m_old;
    QImage m_new;
};

/// Helper único: aplica a nova seleção através do histórico quando
/// disponível, ou direto no documento quando não há CommandStack
/// (a edição nunca se perde — mesmo padrão de todos os outros comandos).
inline void executeSetPixelSelection(Document& doc, CommandStack* history,
                                     QImage newSelection,
                                     const QString& name = QStringLiteral("selection.set"))
{
    const QImage old = doc.pixelSelection();
    if (history) {
        history->execute(std::make_unique<SetPixelSelectionCommand>(
            doc, old, std::move(newSelection), name));
    } else {
        if (newSelection.isNull())
            doc.clearPixelSelection();
        else
            doc.setPixelSelection(newSelection);
    }
}

} // namespace cc
