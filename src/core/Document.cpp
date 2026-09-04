#include "Document.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace cc {

Document::Document(int width, int height, int dpi, QObject* parent)
    : QObject(parent)
    , m_width(qMax(1, width))
    , m_height(qMax(1, height))
    , m_dpi(qMax(1, dpi))
{
    m_root = std::make_unique<GroupLayer>();
    m_root->name = QStringLiteral("Root");
}

Document::~Document() = default;

Layer* Document::findLayer(const LayerId& id) const
{
    if (m_root->id() == id)
        return m_root.get();
    return findRecursive(m_root.get(), id);
}

Layer* Document::findRecursive(GroupLayer* group, const LayerId& id) const
{
    for (const auto& child : group->children) {
        if (child->id() == id)
            return child.get();
        if (child->type() == LayerType::Group) {
            if (Layer* found =
                    findRecursive(static_cast<GroupLayer*>(child.get()), id))
                return found;
        }
    }
    return nullptr;
}

GroupLayer* Document::parentOf(const LayerId& id) const
{
    if (m_root->id() == id)
        return nullptr;
    return findParentRecursive(m_root.get(), id);
}

GroupLayer* Document::findParentRecursive(GroupLayer* group, const LayerId& id) const
{
    for (const auto& child : group->children) {
        if (child->id() == id)
            return group;
        if (child->type() == LayerType::Group) {
            if (GroupLayer* found = findParentRecursive(
                    static_cast<GroupLayer*>(child.get()), id))
                return found;
        }
    }
    return nullptr;
}

int Document::indexOf(const LayerId& id) const
{
    GroupLayer* parent = parentOf(id);
    if (!parent)
        return -1;
    for (std::size_t i = 0; i < parent->children.size(); ++i)
        if (parent->children[i]->id() == id)
            return static_cast<int>(i);
    return -1;
}

bool Document::addLayer(std::unique_ptr<Layer> layer, GroupLayer* parent, int index)
{
    if (!layer)
        return false;
    if (!parent)
        parent = m_root.get();
    if (findLayer(parent->id()) != parent)
        return false;

    auto& children = parent->children;
    if (index < 0)
        index = static_cast<int>(children.size());
    else
        index = qBound(0, index, static_cast<int>(children.size()));

    children.insert(children.begin() + index, std::move(layer));
    bumpRevision();
    emit structureChanged();
    return true;
}

std::unique_ptr<Layer> Document::detachLayer(const LayerId& id)
{
    GroupLayer* parent = parentOf(id);
    if (!parent)
        return nullptr;
    auto& children = parent->children;
    auto it = std::find_if(children.begin(), children.end(),
                           [&](const auto& l) { return l->id() == id; });
    if (it == children.end())
        return nullptr;
    auto layer = std::move(*it);
    children.erase(it);
    return layer;
}

std::unique_ptr<Layer> Document::takeLayer(const LayerId& id)
{
    auto layer = detachLayer(id);
    if (!layer)
        return nullptr;
    bumpRevision();
    emit structureChanged();
    return layer;
}

bool Document::removeLayer(const LayerId& id)
{
    return takeLayer(id) != nullptr;
}

bool Document::reorderLayer(const LayerId& id, GroupLayer* newParent, int newIndex)
{
    Layer* layer = findLayer(id);
    if (!layer || id == m_root->id())
        return false;
    if (!newParent)
        newParent = m_root.get();
    if (findLayer(newParent->id()) != newParent)
        return false;

    for (GroupLayer* p = newParent; p; p = parentOf(p->id())) {
        if (p == layer)
            return false;
    }

    auto moved = detachLayer(id);
    if (!moved)
        return false;

    auto& children = newParent->children;
    if (newIndex < 0)
        newIndex = static_cast<int>(children.size());
    else
        newIndex = qBound(0, newIndex, static_cast<int>(children.size()));
    children.insert(children.begin() + newIndex, std::move(moved));

    bumpRevision();
    emit structureChanged();
    return true;
}

LayerId Document::duplicateLayer(const LayerId& id)
{
    Layer* original = findLayer(id);
    if (!original || id == m_root->id())
        return {};

    GroupLayer* parent = parentOf(id);
    const int insertAt = indexOf(id) + 1;

    auto copy = original->deepCopy();
    copy->name = original->name + QStringLiteral(" copy");
    const LayerId newId = copy->id();

    if (!addLayer(std::move(copy), parent, insertAt))
        return {};
    return newId;
}

bool Document::setLayerName(const LayerId& id, QString name)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->name == name)
        return layer != nullptr;
    layer->name = std::move(name);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerVisible(const LayerId& id, bool visible)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->visible == visible)
        return layer != nullptr;
    layer->visible = visible;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerLocked(const LayerId& id, bool locked)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->locked == locked)
        return layer != nullptr;
    layer->locked = locked;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerOpacity(const LayerId& id, float opacity)
{
    Layer* layer = findLayer(id);
    if (!layer)
        return false;
    const float old = layer->opacity();
    layer->setOpacity(opacity);
    if (layer->opacity() == old)
        return true;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerBlendMode(const LayerId& id, BlendMode mode)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->blendMode == mode)
        return layer != nullptr;
    layer->blendMode = mode;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerTransform(const LayerId& id, const AffineTransform& transform)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->transform == transform)
        return layer != nullptr;
    layer->transform = transform;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerTextBox(const LayerId& id, const QSizeF& box)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (text->box == box)
        return true;
    text->box = box;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerTextEffects(const LayerId& id, const TextEffects& effects)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (text->effects == effects)
        return true;
    text->effects = effects;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerTextContent(const LayerId& id, QString content)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (text->content == content)
        return true;
    text->content = std::move(content);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

void Document::touchLayer(const LayerId& id)
{
    if (findLayer(id)) {
        bumpRevision();
        emit layerPropertyChanged(id);
    }
}

// Altera a cor de preenchimento da forma geométrica
bool Document::setShapeFill(const LayerId& id, QColor fill)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;
    auto* shape = static_cast<ShapeLayer*>(layer);
    if (shape->fill == fill)
        return true; // Sem alteração, evita notificações redundantes
    shape->fill = std::move(fill);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

// Altera a cor do contorno (stroke) da forma geométrica
bool Document::setShapeStroke(const LayerId& id, QColor stroke)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;
    auto* shape = static_cast<ShapeLayer*>(layer);
    if (shape->stroke == stroke)
        return true;
    shape->stroke = std::move(stroke);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

// Altera a espessura do contorno da forma geométrica (clamped em >= 0.0)
bool Document::setShapeStrokeWidth(const LayerId& id, double width)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;
    auto* shape = static_cast<ShapeLayer*>(layer);
    const double clamped = std::max(0.0, width);
    if (qFuzzyCompare(shape->strokeWidth, clamped))
        return true;
    shape->strokeWidth = clamped;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

// Altera o raio de arredondamento dos cantos (para RoundedRect, clamped em >= 0.0)
bool Document::setShapeCornerRadius(const LayerId& id, double radius)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;
    auto* shape = static_cast<ShapeLayer*>(layer);
    const double clamped = std::max(0.0, radius);
    if (qFuzzyCompare(shape->cornerRadius, clamped))
        return true;
    shape->cornerRadius = clamped;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

} // namespace cc
