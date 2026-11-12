#pragma once

#include "BaseCommands.h"
#include "core/layers/Layer.h"

#include <optional>

namespace cc {

/// Vincula (ou desvincla) um texto a um caminho vectorial — um passo de
/// histórico, como qualquer outra propriedade.
class SetTextPathLayerCommand final : public Command
{
public:
    SetTextPathLayerCommand(Document& doc, const LayerId& textId,
                            std::optional<LayerId> oldPath, std::optional<LayerId> newPath,
                            QString name = QStringLiteral("text.pathLink"))
        : Command(std::move(name))
        , m_doc(&doc)
        , m_textId(textId)
        , m_oldPath(std::move(oldPath))
        , m_newPath(std::move(newPath))
    {
    }

    void redo() override
    {
        if (m_doc)
            m_doc->setTextPathLayerId(m_textId, m_newPath);
    }

    void undo() override
    {
        if (m_doc)
            m_doc->setTextPathLayerId(m_textId, m_oldPath);
    }

private:
    Document* m_doc = nullptr;
    LayerId m_textId;
    std::optional<LayerId> m_oldPath;
    std::optional<LayerId> m_newPath;
};

/// Desloca o ponto de partida do texto ao longo do caminho (0..1).
class SetTextPathOffsetCommand final : public Command
{
public:
    SetTextPathOffsetCommand(Document& doc, const LayerId& textId,
                             double oldOffset, double newOffset,
                             QString name = QStringLiteral("text.pathOffset"))
        : Command(std::move(name))
        , m_doc(&doc)
        , m_textId(textId)
        , m_oldOffset(oldOffset)
        , m_newOffset(newOffset)
    {
    }

    void redo() override
    {
        if (m_doc)
            m_doc->setTextPathOffset(m_textId, m_newOffset);
    }

    void undo() override
    {
        if (m_doc)
            m_doc->setTextPathOffset(m_textId, m_oldOffset);
    }

    bool mergeWith(const Command& other) override
    {
        const auto* rhs = dynamic_cast<const SetTextPathOffsetCommand*>(&other);
        if (!rhs || rhs->m_doc != m_doc || rhs->m_textId != m_textId)
            return false;
        m_newOffset = rhs->m_newOffset;
        return true;
    }

private:
    Document* m_doc = nullptr;
    LayerId m_textId;
    double m_oldOffset = 0.0;
    double m_newOffset = 0.0;
};

} // namespace cc
