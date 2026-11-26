#pragma once

#include "BaseCommands.h"

#include <QPointF>
#include <utility>
#include <vector>

namespace cc {

namespace canvas_command_detail {

/// Estado auxiliar cuja transformação pode ser destrutiva (recorte) e, por
/// isso, não pode ser recuperada aplicando apenas a geometria inversa.
struct StateSnapshot {
    explicit StateSnapshot(const Document& doc)
        : selection(doc.selectionSnapshot())
        , guides(doc.guides())
        , webSlices(doc.webSlices())
    {
    }

    void restore(Document& doc) const
    {
        doc.restoreSelectionSnapshot(selection);
        doc.setGuides(guides);
        doc.setWebSlices(webSlices);
    }

    SelectionState selection;
    std::vector<Guide> guides;
    std::vector<WebSlice> webSlices;
};

} // namespace canvas_command_detail

/// Redimensiona a tela (canvas) com undo/redo. |layerOffset| desloca as
/// camadas do grupo raiz (âncora do diálogo) — no undo aplica-se o inverso.
class ResizeCanvasCommand final : public Command
{
public:
    ResizeCanvasCommand(Document& doc, int newWidth, int newHeight,
                        QPointF layerOffset)
        : Command(QStringLiteral("canvas.resize"))
        , m_doc(&doc)
        , m_oldW(doc.width())
        , m_oldH(doc.height())
        , m_newW(qMax(1, newWidth))
        , m_newH(qMax(1, newHeight))
        , m_offset(layerOffset)
        , m_oldState(doc)
    {
    }

    void redo() override { m_doc->setCanvasSize(m_newW, m_newH, m_offset); }
    void undo()
    {
        m_doc->setCanvasSize(m_oldW, m_oldH, -m_offset);
        m_oldState.restore(*m_doc);
    }

private:
    Document* m_doc;
    int m_oldW;
    int m_oldH;
    int m_newW;
    int m_newH;
    QPointF m_offset;
    canvas_command_detail::StateSnapshot m_oldState;
};

/// Rotaciona a tela (90/-90/180 graus) com undo/redo. Guarda as transforms
/// originais das camadas do grupo raiz e as dimensões antigas; o redo chama
/// Document::rotateCanvas (que roda a partir do estado atual — após o undo
/// esse estado é exatamente o original, logo o redo reproduz o resultado).
class RotateCanvasCommand final : public Command
{
public:
    RotateCanvasCommand(Document& doc, double degrees)
        : Command(QStringLiteral("canvas.rotate"))
        , m_doc(&doc)
        , m_degrees(degrees)
        , m_oldW(doc.width())
        , m_oldH(doc.height())
        , m_oldState(doc)
    {
        for (const auto& child : doc.rootGroup()->children)
            m_oldTransforms.emplace_back(child->id(), child->transform);
    }

    void redo() override { m_doc->rotateCanvas(m_degrees); }

    void undo() override
    {
        m_doc->setCanvasSize(m_oldW, m_oldH, QPointF(0, 0));
        for (const auto& [id, t] : m_oldTransforms)
            m_doc->setLayerTransform(id, t);
        m_oldState.restore(*m_doc);
    }

private:
    Document* m_doc;
    double m_degrees;
    int m_oldW;
    int m_oldH;
    canvas_command_detail::StateSnapshot m_oldState;
    std::vector<std::pair<LayerId, AffineTransform>> m_oldTransforms;
};

} // namespace cc
