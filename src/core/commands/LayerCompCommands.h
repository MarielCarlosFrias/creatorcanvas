#pragma once

#include "BaseCommands.h"
#include "core/history/CommandStack.h"
#include "core/Document.h"
#include "core/document/LayerComps.h"
#include "core/document/WebSlice.h"

#include <optional>

namespace cc {

/// Captura o estado actual do documento como uma composição de camadas
/// (Photoshop: "New Layer Comp from Current"). Só as camadas com estado
/// relevante entram; `applyX` escolhe que grupos de propriedades a comp
/// guarda — o utilizador marca isso ao criar.
inline LayerComp compFromDocument(const Document& doc, const QString& name,
                                  bool applyVisibility, bool applyPosition,
                                  bool applyAppearance,
                                  const QString& comment = QString())
{
    LayerComp comp;
    comp.name = name;
    comp.comment = comment;
    comp.applyVisibility = applyVisibility;
    comp.applyPosition = applyPosition;
    comp.applyAppearance = applyAppearance;

    // Percorre a árvore inteira (grupos incluídos) e grava o estado de
    // cada camada segundo os grupos activos.
    std::vector<const GroupLayer*> stack;
    if (const GroupLayer* root = doc.rootGroup()) {
        stack.push_back(root);
    }
    while (!stack.empty()) {
        const GroupLayer* group = stack.back();
        stack.pop_back();
        for (const auto& child : group->children) {
            if (!child)
                continue;
            CompLayerState state;
            state.layerId = child->id();
            if (applyVisibility)
                state.visible = child->visible;
            if (applyPosition)
                state.position = child->transform.position;
            if (applyAppearance) {
                state.opacity = child->opacity();
                state.blendMode = child->blendMode;
            }
            comp.states.push_back(state);
            if (child->type() == LayerType::Group)
                stack.push_back(static_cast<const GroupLayer*>(child.get()));
        }
    }
    return comp;
}

/// Aplica uma composição de camadas e lembra o estado anterior de cada
/// propriedade tocada, para que o undo restaure exactamente o que estava
/// (Photoshop: trocar de comp é um passo de histórico, não vários).
class ApplyLayerCompCommand final : public Command
{
public:
    ApplyLayerCompCommand(Document& doc, const LayerComp& comp,
                          QString name = QStringLiteral("comps.apply"))
        : Command(std::move(name))
        , m_doc(&doc)
        , m_compId(comp.id)
    {
        // Snapshot do estado actual apenas das camadas que a comp toca.
        for (const auto& state : comp.states) {
            const Layer* layer = doc.findLayer(state.layerId);
            if (!layer)
                continue;
            UndoState undo;
            undo.visible = layer->visible;
            undo.position = layer->transform.position;
            undo.opacity = layer->opacity();
            undo.blendMode = layer->blendMode;
            m_undoStates.emplace_back(state.layerId, undo);
        }
    }

    void redo() override
    {
        if (m_doc)
            m_doc->applyLayerComp(m_compId);
    }

    void undo() override
    {
        if (!m_doc)
            return;
        for (const auto& [layerId, undo] : m_undoStates) {
            Layer* layer = m_doc->findLayer(layerId);
            if (!layer)
                continue;
            layer->visible = undo.visible;
            layer->transform.position = undo.position;
            layer->setOpacity(undo.opacity);
            layer->blendMode = undo.blendMode;
            m_doc->touchLayer(layerId);
        }
    }

private:
    struct UndoState {
        bool visible = true;
        QPointF position;
        float opacity = 1.0f;
        BlendMode blendMode = BlendMode::Normal;
    };

    Document* m_doc = nullptr;
    QString m_compId;
    std::vector<std::pair<LayerId, UndoState>> m_undoStates;
};

// ---------------------------------------------------------------------------
// Helpers: comando quando há histórico, edição directa quando não há (a
// edição nunca se perde — mesmo padrão dos outros execute*).
// ---------------------------------------------------------------------------

inline void executeAddLayerComp(Document& doc, CommandStack* history, LayerComp comp)
{
    if (history) {
        struct AddComp final : Command {
            AddComp(Document& d, LayerComp c) : Command(QStringLiteral("comps.add")), m_doc(&d), m_comp(std::move(c)) {}
            void redo() override { m_doc->addLayerComp(m_comp); }
            void undo() override { m_doc->removeLayerComp(m_comp.id); }
            Document* m_doc;
            LayerComp m_comp;
        };
        history->execute(std::make_unique<AddComp>(doc, std::move(comp)));
    } else {
        doc.addLayerComp(std::move(comp));
    }
}

inline void executeRemoveLayerComp(Document& doc, CommandStack* history, const QString& compId)
{
    // O comp é guardado para o undo poder repô-lo.
    LayerComp removed;
    bool found = false;
    for (const auto& c : doc.layerComps()) {
        if (c.id == compId) {
            removed = c;
            found = true;
            break;
        }
    }
    if (!found)
        return;

    if (history) {
        struct RemoveComp final : Command {
            RemoveComp(Document& d, LayerComp c)
                : Command(QStringLiteral("comps.remove")), m_doc(&d), m_comp(std::move(c)) {}
            void redo() override { m_doc->removeLayerComp(m_comp.id); }
            void undo() override { m_doc->addLayerComp(m_comp); }
            Document* m_doc;
            LayerComp m_comp;
        };
        history->execute(std::make_unique<RemoveComp>(doc, std::move(removed)));
    } else {
        doc.removeLayerComp(compId);
    }
}

inline bool executeApplyLayerComp(Document& doc, CommandStack* history, const QString& compId)
{
    for (const auto& c : doc.layerComps()) {
        if (c.id != compId)
            continue;
        if (history)
            history->execute(std::make_unique<ApplyLayerCompCommand>(doc, c));
        else
            doc.applyLayerComp(compId);
        return true;
    }
    return false;
}

inline void executeAddWebSlice(Document& doc, CommandStack* history, WebSlice slice)
{
    if (history) {
        struct AddSlice final : Command {
            AddSlice(Document& d, WebSlice s) : Command(QStringLiteral("slices.add")), m_doc(&d), m_slice(std::move(s)) {}
            void redo() override { m_doc->addWebSlice(m_slice); }
            void undo() override { m_doc->removeWebSlice(m_slice.id); }
            Document* m_doc;
            WebSlice m_slice;
        };
        history->execute(std::make_unique<AddSlice>(doc, std::move(slice)));
    } else {
        doc.addWebSlice(std::move(slice));
    }
}

inline void executeRemoveWebSlice(Document& doc, CommandStack* history, const QString& sliceId)
{
    WebSlice removed;
    bool found = false;
    for (const auto& s : doc.webSlices()) {
        if (s.id == sliceId) {
            removed = s;
            found = true;
            break;
        }
    }
    if (!found)
        return;

    if (history) {
        struct RemoveSlice final : Command {
            RemoveSlice(Document& d, WebSlice s)
                : Command(QStringLiteral("slices.remove")), m_doc(&d), m_slice(std::move(s)) {}
            void redo() override { m_doc->removeWebSlice(m_slice.id); }
            void undo() override { m_doc->addWebSlice(m_slice); }
            Document* m_doc;
            WebSlice m_slice;
        };
        history->execute(std::make_unique<RemoveSlice>(doc, std::move(removed)));
    } else {
        doc.removeWebSlice(sliceId);
    }
}

} // namespace cc
