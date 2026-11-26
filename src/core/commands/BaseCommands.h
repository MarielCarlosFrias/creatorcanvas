#pragma once

#include "core/history/Command.h"
#include "core/Document.h"

#include <QtGlobal>
#include <QString>
#include <QSet>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <vector>
#include <memory>

namespace cc {

enum class LockRequirement {
    None,
    Position,
    Pixels,
    Transparency,
    All
};

inline bool isLayerLockedFor(const Document* doc, const LayerId& id, LockRequirement req)
{
    if (!doc || id.isNull() || req == LockRequirement::None)
        return false;

    const LayerLocks locks = doc->effectiveLocks(id);
    if (locks.all)
        return true;

    switch (req) {
    case LockRequirement::Position:
        return locks.position;
    case LockRequirement::Pixels:
        return locks.pixels;
    case LockRequirement::Transparency:
        return locks.transparency;
    case LockRequirement::All:
        return locks.all;
    case LockRequirement::None:
    default:
        return false;
    }
}

class LayerCommand : public Command
{
public:
    LayerCommand(QString name, Document& doc, LayerId layerId,
                 LockRequirement lockReq = LockRequirement::All)
        : Command(std::move(name))
        , m_doc(&doc)
        , m_layerId(std::move(layerId))
        , m_lockReq(lockReq)
    {
    }

    ~LayerCommand() override = default;

    Document* document() const { return m_doc; }
    const LayerId& layerId() const { return m_layerId; }
    LockRequirement lockRequirement() const { return m_lockReq; }

    virtual bool canExecute() const
    {
        if (!m_doc || m_layerId.isNull())
            return false;
        return !isLayerLockedFor(m_doc, m_layerId, m_lockReq);
    }

protected:
    Document* m_doc;
    LayerId m_layerId;
    LockRequirement m_lockReq;
};

/// Macro command that executes multiple commands sequentially as a single undo/redo step.
class CompoundCommand final : public Command
{
public:
    explicit CompoundCommand(QString name = QStringLiteral("compound"))
        : Command(std::move(name))
    {
    }

    void addCommand(std::unique_ptr<Command> cmd)
    {
        if (cmd)
            m_commands.push_back(std::move(cmd));
    }

    bool isEmpty() const { return m_commands.empty(); }
    std::size_t size() const { return m_commands.size(); }

    void collectAssetIds(QSet<LayerId>& out) const override
    {
        for (const auto& cmd : m_commands)
            cmd->collectAssetIds(out);
    }

    void redo() override
    {
        for (auto& cmd : m_commands)
            cmd->redo();
    }

    void undo() override
    {
        for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it)
            (*it)->undo();
    }

private:
    std::vector<std::unique_ptr<Command>> m_commands;
};

namespace detail {

inline GroupLayer* resolveGroup(Document* doc, const LayerId& id)
{
    Layer* p = doc ? doc->findLayer(id) : nullptr;
    return (p && p->type() == LayerType::Group)
               ? static_cast<GroupLayer*>(p)
               : nullptr;
}

/// Percorre a camada (e subgrupos) coletando os assetIds que ela mantém
/// vivos no AssetStore. Usado pelo GC de comandos expulsos da pilha.
inline void collectLayerAssetIds(const Layer* layer, QSet<LayerId>& out)
{
    if (!layer)
        return;
    if (layer->type() == LayerType::Image)
        out.insert(static_cast<const ImageLayer*>(layer)->assetId);
    if (layer->type() == LayerType::Group)
        for (const auto& child : static_cast<const GroupLayer*>(layer)->children)
            collectLayerAssetIds(child.get(), out);
}

/// Matriz que leva coordenadas locais de um pai para o documento. A raiz é
/// apenas um contentor: o seu transform nunca participa na geometria.
inline QTransform parentToDocumentMatrix(const Document* doc, const GroupLayer* parent)
{
    if (!doc || !parent || parent == doc->rootGroup())
        return QTransform();

    QTransform matrix = parent->transform.matrix(parent->contentBounds());
    for (GroupLayer* p = doc->parentOf(parent->id());
         p && p != doc->rootGroup(); p = doc->parentOf(p->id())) {
        matrix = matrix * p->transform.matrix(p->contentBounds());
    }
    return matrix;
}

/// Rebaseia o transform de `child` por uma mudança de espaço do pai.
/// AffineTransform gera S · H · R na parte linear. Qualquer matriz afim pode
/// ser representada com shearY = 0 por uma decomposição LQ; ao contrário da
/// antiga extração de comprimento/ângulo, isto preserva flip, shear e rotação.
inline AffineTransform rebaseTransform(const QTransform& parentSpaceChange,
                                       const Layer& child)
{
    const QRectF cb = child.contentBounds();
    const QTransform m = child.transform.matrix(cb) * parentSpaceChange;

    // Uma translação não exige reparametrizar o filho: mantém inclusive os
    // sinais de flip e os dois campos de shear originais.
    if (parentSpaceChange.m11() == 1.0 && parentSpaceChange.m12() == 0.0
        && parentSpaceChange.m21() == 0.0 && parentSpaceChange.m22() == 1.0) {
        AffineTransform t = child.transform;
        t.position = m.map(cb.center());
        return t;
    }

    AffineTransform t;
    t.position = m.map(cb.center());
    t.shearY = 0.0;

    const double scaleX = std::hypot(m.m11(), m.m12());
    if (scaleX != 0.0) {
        const double angle = std::atan2(m.m12(), m.m11());
        const double c = std::cos(angle);
        const double s = std::sin(angle);
        t.rotationDeg = qRadiansToDegrees(angle);
        t.scaleX = scaleX;
        t.scaleY = -m.m21() * s + m.m22() * c;
        t.shearX = t.scaleY != 0.0
                       ? (m.m21() * c + m.m22() * s) / t.scaleY
                       : 0.0;
    } else {
        const double scaleY = std::hypot(m.m21(), m.m22());
        t.scaleX = 0.0;
        t.scaleY = scaleY;
        if (scaleY != 0.0)
            t.rotationDeg = qRadiansToDegrees(std::atan2(-m.m21(), m.m22()));
    }
    return t;
}

/// Invariante de grupos: a matriz do grupo deve ser a identidade quando o
/// conteúdo não foi movido — ou seja, transform.position = centro do
/// contentBounds no espaço do pai. Chamar depois de criar o grupo.
inline void recenterGroupTransform(GroupLayer* group)
{
    if (!group)
        return;
    const QRectF b = group->contentBounds();
    if (!b.isEmpty())
        group->transform.position = b.center();
}

/// Mantém a matriz quando o centro do contentBounds muda. O delta do centro
/// tem de ser transformado pela parte linear: somar o delta diretamente só é
/// correto para grupos sem rotação, escala, flip ou shear.
inline void keepGroupMatrixOnContentChange(GroupLayer* group, const QRectF& oldBounds)
{
    if (!group)
        return;

    QTransform linear;
    linear.rotate(group->transform.rotationDeg);
    linear.shear(group->transform.shearX, group->transform.shearY);
    linear.scale(group->transform.scaleX, group->transform.scaleY);
    group->transform.position +=
        linear.map(group->contentBounds().center() - oldBounds.center());
}

/// Estado geométrico dos pais afetados por uma alteração estrutural. Guardar
/// a cadeia inteira evita que mudanças de bounds propaguem saltos aos grupos
/// ancestrais. A raiz fica deliberadamente de fora.
struct GroupGeometryState {
    LayerId id;
    AffineTransform transform;
    QRectF bounds;
    int depth = 0;
};

inline void captureGroupAndAncestors(Document* doc, GroupLayer* group,
                                     std::vector<GroupGeometryState>& states)
{
    if (!doc)
        return;
    for (GroupLayer* g = group; g && g != doc->rootGroup();
         g = doc->parentOf(g->id())) {
        const auto existing = std::find_if(
            states.begin(), states.end(),
            [&](const GroupGeometryState& s) { return s.id == g->id(); });
        if (existing != states.end())
            continue;

        GroupGeometryState state;
        state.id = g->id();
        state.transform = g->transform;
        state.bounds = g->contentBounds();
        for (GroupLayer* p = g; p && p != doc->rootGroup();
             p = doc->parentOf(p->id()))
            ++state.depth;
        states.push_back(std::move(state));
    }
}

/// Ajusta primeiro os grupos mais profundos: o ajuste de um filho pode mudar
/// os bounds do pai, que por sua vez também tem de manter a sua matriz.
inline void preserveGroupMatrices(Document* doc,
                                  std::vector<GroupGeometryState> states)
{
    std::sort(states.begin(), states.end(),
              [](const GroupGeometryState& a, const GroupGeometryState& b) {
                  return a.depth > b.depth;
              });
    for (const GroupGeometryState& state : states)
        if (auto* group = resolveGroup(doc, state.id))
            keepGroupMatrixOnContentChange(group, state.bounds);
}

inline void restoreGroupTransforms(Document* doc,
                                   const std::vector<GroupGeometryState>& states)
{
    for (const GroupGeometryState& state : states)
        if (auto* group = resolveGroup(doc, state.id))
            group->transform = state.transform;
}

} // namespace detail

} // namespace cc
