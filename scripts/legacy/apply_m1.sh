#!/usr/bin/env bash
# ============================================================================
# CreatorCanvas — M1 applier (Document & Layer Model)
# Usage:  bash apply_m1.sh [project-root]   (default: current directory)
# ============================================================================
set -euo pipefail

cd "${1:-.}"
[ -f CMakeLists.txt ] || { echo "ERROR: run this script inside the creatorcanvas folder"; exit 1; }

echo ">> Applying M1 to: $(pwd)"
mkdir -p src/core/layers tests/unit

# ------------------------------------------------------------------ Layer.h
cat > src/core/layers/Layer.h <<'CC_LAYER_H'
#pragma once

#include <QColor>
#include <QPolygonF>
#include <QString>
#include <QUuid>

#include <memory>
#include <vector>

namespace cc {

/// Stable, serialization-friendly unique layer identifier.
using LayerId = QUuid;

inline LayerId newLayerId() { return QUuid::createUuid(); }

enum class LayerType {
    Group,
    Image,
    Text,
    Shape,
    Background
};

enum class BlendMode {
    Normal,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    Add
};

enum class TextAlignment { Left, Center, Right };

enum class ShapeKind { Rectangle, RoundedRect, Ellipse, Line, Polygon };

/// Base class of every layer.
///
/// Contract:
///  - Layers are owned by their parent GroupLayer (unique_ptr); the Document
///    owns the root. Non-owning raw pointers handed out by Document remain
///    valid until the next structural mutation.
///  - UI/commands mutate layers ONLY through Document methods, which enforce
///    invariants and emit notifications. Direct field access is reserved for
///    the read path (renderer, serializer).
///  - deepCopy() produces a deep copy with FRESH ids on every node (used by
///    duplicate). Serialization (M3) reads/writes fields directly.
class Layer
{
public:
    explicit Layer(LayerType type);
    virtual ~Layer();

    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;

    LayerType type() const { return m_type; }
    const LayerId& id() const { return m_id; }

    QString name;
    bool visible = true;
    bool locked = false;
    BlendMode blendMode = BlendMode::Normal;

    float opacity() const { return m_opacity; }
    /// Clamps to [0, 1].
    void setOpacity(float value);

    virtual std::unique_ptr<Layer> deepCopy() const = 0;

protected:
    /// Copies the common properties from |other| and assigns a FRESH id.
    void copyCommonFrom(const Layer& other);

private:
    LayerType m_type;
    LayerId m_id;
    float m_opacity = 1.0f;
};

class GroupLayer final : public Layer
{
public:
    GroupLayer();

    std::unique_ptr<Layer> deepCopy() const override;

    /// Owned children. Index 0 = bottom of this group, last = top.
    std::vector<std::unique_ptr<Layer>> children;
};

class ImageLayer final : public Layer
{
public:
    ImageLayer();

    std::unique_ptr<Layer> deepCopy() const override;

    LayerId assetId;        // resolved through the asset store (M3+)
    int naturalWidth = 0;   // pixels of the source image
    int naturalHeight = 0;
};

class TextLayer final : public Layer
{
public:
    TextLayer();

    std::unique_ptr<Layer> deepCopy() const override;

    QString content;
    QString fontFamily = QStringLiteral("Sans Serif");
    double sizePt = 48.0;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    QColor color = Qt::white;
    double letterSpacingPx = 0.0;
    double lineHeightMult = 1.0;
    TextAlignment align = TextAlignment::Center;
};

class ShapeLayer final : public Layer
{
public:
    ShapeLayer();

    std::unique_ptr<Layer> deepCopy() const override;

    ShapeKind kind = ShapeKind::Rectangle;
    QColor fill = Qt::white;
    QColor stroke = Qt::transparent;   // alpha 0 = no stroke
    double strokeWidth = 0.0;
    double cornerRadius = 0.0;         // RoundedRect only
    QPolygonF points;                  // Line / Polygon vertices (document px)
};

class BackgroundLayer final : public Layer
{
public:
    BackgroundLayer();

    std::unique_ptr<Layer> deepCopy() const override;

    QColor fill = Qt::white;           // fully transparent = transparent canvas
};

/// Allocates the concrete layer for |type|.
std::unique_ptr<Layer> makeLayer(LayerType type);

} // namespace cc
CC_LAYER_H

# ----------------------------------------------------------------- Layer.cpp
cat > src/core/layers/Layer.cpp <<'CC_LAYER_CPP'
#include "layers/Layer.h"

#include <algorithm>

namespace cc {

Layer::Layer(LayerType type)
    : m_type(type), m_id(newLayerId()) {}

Layer::~Layer() = default;

void Layer::setOpacity(float value)
{
    m_opacity = std::clamp(value, 0.0f, 1.0f);
}

void Layer::copyCommonFrom(const Layer& other)
{
    m_id = newLayerId();          // duplicates always get a fresh identity
    name = other.name;
    visible = other.visible;
    locked = other.locked;
    blendMode = other.blendMode;
    m_opacity = other.m_opacity;
}

GroupLayer::GroupLayer()
    : Layer(LayerType::Group) {}

std::unique_ptr<Layer> GroupLayer::deepCopy() const
{
    auto copy = std::make_unique<GroupLayer>();
    copy->copyCommonFrom(*this);
    copy->children.reserve(children.size());
    for (const auto& child : children)
        copy->children.push_back(child->deepCopy());
    return copy;
}

ImageLayer::ImageLayer()
    : Layer(LayerType::Image) {}

std::unique_ptr<Layer> ImageLayer::deepCopy() const
{
    auto copy = std::make_unique<ImageLayer>();
    copy->copyCommonFrom(*this);
    copy->assetId = assetId;
    copy->naturalWidth = naturalWidth;
    copy->naturalHeight = naturalHeight;
    return copy;
}

TextLayer::TextLayer()
    : Layer(LayerType::Text) {}

std::unique_ptr<Layer> TextLayer::deepCopy() const
{
    auto copy = std::make_unique<TextLayer>();
    copy->copyCommonFrom(*this);
    copy->content = content;
    copy->fontFamily = fontFamily;
    copy->sizePt = sizePt;
    copy->bold = bold;
    copy->italic = italic;
    copy->underline = underline;
    copy->color = color;
    copy->letterSpacingPx = letterSpacingPx;
    copy->lineHeightMult = lineHeightMult;
    copy->align = align;
    return copy;
}

ShapeLayer::ShapeLayer()
    : Layer(LayerType::Shape) {}

std::unique_ptr<Layer> ShapeLayer::deepCopy() const
{
    auto copy = std::make_unique<ShapeLayer>();
    copy->copyCommonFrom(*this);
    copy->kind = kind;
    copy->fill = fill;
    copy->stroke = stroke;
    copy->strokeWidth = strokeWidth;
    copy->cornerRadius = cornerRadius;
    copy->points = points;
    return copy;
}

BackgroundLayer::BackgroundLayer()
    : Layer(LayerType::Background) {}

std::unique_ptr<Layer> BackgroundLayer::deepCopy() const
{
    auto copy = std::make_unique<BackgroundLayer>();
    copy->copyCommonFrom(*this);
    copy->fill = fill;
    return copy;
}

std::unique_ptr<Layer> makeLayer(LayerType type)
{
    switch (type) {
    case LayerType::Group:      return std::make_unique<GroupLayer>();
    case LayerType::Image:      return std::make_unique<ImageLayer>();
    case LayerType::Text:       return std::make_unique<TextLayer>();
    case LayerType::Shape:      return std::make_unique<ShapeLayer>();
    case LayerType::Background: return std::make_unique<BackgroundLayer>();
    }
    return nullptr;
}

} // namespace cc
CC_LAYER_CPP

# --------------------------------------------------------------- Document.h
cat > src/core/Document.h <<'CC_DOC_H'
#pragma once

#include <QObject>
#include <QtGlobal>

#include <memory>

#include "layers/Layer.h"

namespace cc {

/// Owns the layer tree and document-level state. ALL structural mutations
/// go through this class: it maintains invariants (membership, acyclicity,
/// opacity range) and emits change notifications.
///
/// Z-order contract: rootGroup()->children[0] is the BOTTOM layer and the
/// last element is the TOP layer, matching paint order (bottom -> top).
///
/// reorderLayer() semantics: after a successful call the layer sits at
/// exactly |newIndex| within |newParent| (final-position semantics).
///
/// addLayer() consumes ownership of |layer| even on failure; failures
/// indicate programming errors (null layer, or a parent that does not
/// belong to this document).
class Document final : public QObject
{
    Q_OBJECT
public:
    explicit Document(int width, int height, int dpi = 96,
                      QObject* parent = nullptr);
    ~Document() override;

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    // Canvas -----------------------------------------------------------------
    int width() const { return m_width; }
    int height() const { return m_height; }
    int dpi() const { return m_dpi; }

    // Tree access (non-owning pointers) ----------------------------------------
    GroupLayer* rootGroup() const { return m_root.get(); }  // never null
    Layer* findLayer(const LayerId& id) const;              // null if absent
    GroupLayer* parentOf(const LayerId& id) const;          // null for root/absent
    int indexOf(const LayerId& id) const;                   // -1 for root/absent

    // Structural operations ------------------------------------------------------
    /// |index| -1 appends (top). Returns false on invalid input.
    bool addLayer(std::unique_ptr<Layer> layer,
                  GroupLayer* parent = nullptr,
                  int index = -1);
    /// Removes and returns ownership (for undo). Null if absent or root.
    std::unique_ptr<Layer> takeLayer(const LayerId& id);
    bool removeLayer(const LayerId& id);
    bool reorderLayer(const LayerId& id, GroupLayer* newParent, int newIndex);
    /// Deep copy with fresh ids, inserted directly ABOVE the original,
    /// named "<original> copy". Returns a null id on failure.
    LayerId duplicateLayer(const LayerId& id);

    // Property operations (return true if the layer exists;
    // emit layerPropertyChanged only on actual change) ----------------------------
    bool setLayerName(const LayerId& id, const QString& name);
    bool setLayerVisible(const LayerId& id, bool visible);
    bool setLayerLocked(const LayerId& id, bool locked);
    bool setLayerOpacity(const LayerId& id, float opacity);  // clamped [0,1]
    bool setLayerBlendMode(const LayerId& id, BlendMode mode);

    // Change tracking ---------------------------------------------------------
    /// Bumped on every effective mutation; render caches key off this.
    quint64 revision() const { return m_revision; }

signals:
    /// Tree topology changed (add / remove / reorder / duplicate).
    void structureChanged();
    /// A layer's name / visibility / lock / opacity / blend mode changed.
    void layerPropertyChanged(const LayerId& id);

private:
    Layer* findRecursive(GroupLayer* group, const LayerId& id) const;
    GroupLayer* findParentRecursive(GroupLayer* group, const LayerId& id) const;
    /// Removes the layer from its parent WITHOUT notifying. Null if absent/root.
    std::unique_ptr<Layer> detachLayer(const LayerId& id);
    void bumpRevision() { ++m_revision; }

    int m_width;
    int m_height;
    int m_dpi;
    std::unique_ptr<GroupLayer> m_root;
    quint64 m_revision = 0;
};

} // namespace cc
CC_DOC_H

# -------------------------------------------------------------- Document.cpp
cat > src/core/Document.cpp <<'CC_DOC_CPP'
#include "Document.h"

#include <algorithm>
#include <cstddef>

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
    if (m_root->id == id)
        return m_root.get();
    return findRecursive(m_root.get(), id);
}

Layer* Document::findRecursive(GroupLayer* group, const LayerId& id) const
{
    for (const auto& child : group->children) {
        if (child->id == id)
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
    if (m_root->id == id)
        return nullptr;
    return findParentRecursive(m_root.get(), id);
}

GroupLayer* Document::findParentRecursive(GroupLayer* group, const LayerId& id) const
{
    for (const auto& child : group->children) {
        if (child->id == id)
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
        if (parent->children[i]->id == id)
            return static_cast<int>(i);
    return -1;
}

bool Document::addLayer(std::unique_ptr<Layer> layer, GroupLayer* parent, int index)
{
    if (!layer)
        return false;
    if (!parent)
        parent = m_root.get();
    if (findLayer(parent->id) != parent)
        return false; // parent must belong to THIS document

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
                           [&](const auto& l) { return l->id == id; });
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
    if (!layer || id == m_root->id)
        return false;
    if (!newParent)
        newParent = m_root.get();
    if (findLayer(newParent->id) != newParent)
        return false;

    // Reject moves into the layer itself or into its own subtree (cycle).
    for (GroupLayer* p = newParent; p; p = parentOf(p->id)) {
        if (p == layer)
            return false;
    }

    auto moved = detachLayer(id);
    if (!moved)
        return false; // unreachable given the checks above

    // The layer is already detached, so newIndex is its FINAL position.
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
    if (!original || id == m_root->id)
        return {};

    GroupLayer* parent = parentOf(id);
    const int insertAt = indexOf(id) + 1; // directly above the original

    auto copy = original->deepCopy();
    copy->name = original->name + QStringLiteral(" copy");
    const LayerId newId = copy->id();

    if (!addLayer(std::move(copy), parent, insertAt))
        return {};
    return newId;
}

bool Document::setLayerName(const LayerId& id, const QString& name)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->name == name)
        return layer != nullptr;
    layer->name = name;
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
    layer->setOpacity(opacity); // clamps
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

} // namespace cc
CC_DOC_CPP

# ------------------------------------------------------------ test_layers.cpp
cat > tests/unit/test_layers.cpp <<'CC_T_LAYERS'
#include "core/layers/Layer.h"

#include <QtTest>

using namespace cc;

class TestLayers final : public QObject
{
    Q_OBJECT

private slots:
    void factoryCreatesAllTypes()
    {
        const LayerType types[] = {
            LayerType::Group, LayerType::Image, LayerType::Text,
            LayerType::Shape, LayerType::Background
        };
        for (LayerType type : types) {
            auto layer = makeLayer(type);
            QVERIFY(layer != nullptr);
            QCOMPARE(static_cast<int>(layer->type()), static_cast<int>(type));
            QVERIFY(!layer->id().isNull());
        }
    }

    void freshLayersHaveUniqueIds()
    {
        auto a = makeLayer(LayerType::Text);
        auto b = makeLayer(LayerType::Text);
        QVERIFY(a->id() != b->id());
    }

    void opacityIsClamped()
    {
        auto layer = makeLayer(LayerType::Text);
        layer->setOpacity(1.7f);
        QCOMPARE(layer->opacity(), 1.0f);
        layer->setOpacity(-0.25f);
        QCOMPARE(layer->opacity(), 0.0f);
        layer->setOpacity(0.35f);
        QCOMPARE(layer->opacity(), 0.35f);
    }

    void deepCopyOfTextCopiesPayloadWithFreshId()
    {
        TextLayer original;
        original.name = QStringLiteral("Main Title");
        original.visible = false;
        original.locked = true;
        original.setOpacity(0.4f);
        original.blendMode = BlendMode::Multiply;
        original.content = QStringLiteral("HELLO");
        original.fontFamily = QStringLiteral("Impact");
        original.sizePt = 72.0;
        original.bold = true;
        original.italic = true;
        original.underline = true;
        original.color = QColor(255, 128, 0);
        original.letterSpacingPx = 2.5;
        original.lineHeightMult = 1.2;
        original.align = TextAlignment::Right;

        auto copy = original.deepCopy();
        QVERIFY(copy != nullptr);
        QCOMPARE(static_cast<int>(copy->type()), static_cast<int>(LayerType::Text));
        QVERIFY(copy->id() != original.id());

        auto* text = static_cast<TextLayer*>(copy.get());
        QCOMPARE(text->name, original.name);
        QCOMPARE(text->visible, original.visible);
        QCOMPARE(text->locked, original.locked);
        QCOMPARE(text->opacity(), original.opacity());
        QCOMPARE(static_cast<int>(text->blendMode),
                 static_cast<int>(original.blendMode));
        QCOMPARE(text->content, original.content);
        QCOMPARE(text->fontFamily, original.fontFamily);
        QCOMPARE(text->sizePt, original.sizePt);
        QCOMPARE(text->bold, original.bold);
        QCOMPARE(text->italic, original.italic);
        QCOMPARE(text->underline, original.underline);
        QCOMPARE(text->color, original.color);
        QCOMPARE(text->letterSpacingPx, original.letterSpacingPx);
        QCOMPARE(text->lineHeightMult, original.lineHeightMult);
        QCOMPARE(static_cast<int>(text->align), static_cast<int>(original.align));
    }

    void deepCopyOfGroupIsRecursiveWithFreshIds()
    {
        auto original = std::make_unique<GroupLayer>();
        original->name = QStringLiteral("Characters");
        original->visible = false;
        original->setOpacity(0.5f);

        auto text = std::make_unique<TextLayer>();
        text->name = QStringLiteral("A");
        text->content = QStringLiteral("Hello");
        const LayerId textId = text->id();

        auto shape = std::make_unique<ShapeLayer>();
        shape->name = QStringLiteral("B");
        shape->kind = ShapeKind::Ellipse;
        shape->fill = QColor(200, 10, 10);
        const LayerId shapeId = shape->id();

        original->children.push_back(std::move(text));
        original->children.push_back(std::move(shape));

        auto copy = original->deepCopy();
        auto* copyGroup = static_cast<GroupLayer*>(copy.get());
        QVERIFY(copyGroup->id() != original->id());
        QCOMPARE(copyGroup->children.size(), std::size_t(2));
        QCOMPARE(copyGroup->name, QStringLiteral("Characters"));
        QCOMPARE(copyGroup->visible, false);
        QCOMPARE(copyGroup->opacity(), 0.5f);

        const Layer& copiedText = *copyGroup->children[0];
        const Layer& copiedShape = *copyGroup->children[1];
        QVERIFY(copiedText.id() != textId);
        QVERIFY(copiedShape.id() != shapeId);
        QVERIFY(copiedText.id() != copiedShape.id());

        QCOMPARE(static_cast<int>(copiedText.type()),
                 static_cast<int>(LayerType::Text));
        QCOMPARE(static_cast<int>(copiedShape.type()),
                 static_cast<int>(LayerType::Shape));
        QCOMPARE(static_cast<const TextLayer&>(copiedText).content,
                 QStringLiteral("Hello"));
        QCOMPARE(static_cast<int>(static_cast<const ShapeLayer&>(copiedShape).kind),
                 static_cast<int>(ShapeKind::Ellipse));
        QCOMPARE(static_cast<const ShapeLayer&>(copiedShape).fill,
                 QColor(200, 10, 10));
        // Child names preserved (suffix only on the duplicated top level).
        QCOMPARE(copiedText.name, QStringLiteral("A"));
    }

    void deepCopyOfShapeAndImageCopiesPayload()
    {
        ShapeLayer shape;
        shape.kind = ShapeKind::Polygon;
        shape.stroke = QColor(0, 255, 0);
        shape.strokeWidth = 3.0;
        shape.cornerRadius = 4.0;
        shape.points = QPolygonF({ QPointF(0, 0), QPointF(10, 0), QPointF(5, 8) });

        auto shapeCopy = shape.deepCopy();
        auto* s = static_cast<ShapeLayer*>(shapeCopy.get());
        QCOMPARE(static_cast<int>(s->kind), static_cast<int>(ShapeKind::Polygon));
        QCOMPARE(s->stroke, QColor(0, 255, 0));
        QCOMPARE(s->strokeWidth, 3.0);
        QCOMPARE(s->cornerRadius, 4.0);
        QCOMPARE(s->points, shape.points);

        ImageLayer image;
        image.assetId = newLayerId();
        image.naturalWidth = 640;
        image.naturalHeight = 480;
        auto imageCopy = image.deepCopy();
        auto* im = static_cast<ImageLayer*>(imageCopy.get());
        QCOMPARE(im->assetId, image.assetId);
        QCOMPARE(im->naturalWidth, 640);
        QCOMPARE(im->naturalHeight, 480);
    }
};

QTEST_GUILESS_MAIN(TestLayers)
#include "test_layers.moc"
CC_T_LAYERS

# ---------------------------------------------------------- test_document.cpp
cat > tests/unit/test_document.cpp <<'CC_T_DOC'
#include "core/Document.h"

#include <QSignalSpy>
#include <QtTest>

using namespace cc;

namespace {

std::unique_ptr<TextLayer> makeText(const QString& name)
{
    auto layer = std::make_unique<TextLayer>();
    layer->name = name;
    return layer;
}

std::unique_ptr<GroupLayer> makeGroup(const QString& name)
{
    auto layer = std::make_unique<GroupLayer>();
    layer->name = name;
    return layer;
}

} // namespace

class TestDocument final : public QObject
{
    Q_OBJECT

private slots:
    void constructorCreatesRootGroup()
    {
        Document doc(1280, 720, 96);
        QVERIFY(doc.rootGroup() != nullptr);
        QCOMPARE(doc.rootGroup()->type(), LayerType::Group);
        QCOMPARE(doc.width(), 1280);
        QCOMPARE(doc.height(), 720);
        QCOMPARE(doc.dpi(), 96);
        QVERIFY(!doc.rootGroup()->id().isNull());

        const LayerId rootId = doc.rootGroup()->id();
        QVERIFY(doc.findLayer(rootId) == doc.rootGroup());
        QVERIFY(doc.parentOf(rootId) == nullptr);
        QCOMPARE(doc.indexOf(rootId), -1);
    }

    void addAppendsToTopByDefault()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        auto b = makeText("B");
        Layer* pa = a.get();
        Layer* pb = b.get();

        QVERIFY(doc.addLayer(std::move(a)));
        QVERIFY(doc.addLayer(std::move(b)));

        QVERIFY(doc.findLayer(pa->id()) == pa);
        QVERIFY(doc.parentOf(pa->id()) == doc.rootGroup());
        QCOMPARE(doc.indexOf(pa->id()), 0);
        QCOMPARE(doc.indexOf(pb->id()), 1);
    }

    void addAtBottomIndex()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        auto b = makeText("B");
        const LayerId ia = a->id();
        const LayerId ib = b->id();
        QVERIFY(doc.addLayer(std::move(a)));
        QVERIFY(doc.addLayer(std::move(b)));

        auto c = makeText("C");
        const LayerId ic = c->id();
        QVERIFY(doc.addLayer(std::move(c), nullptr, 0));

        QCOMPARE(doc.indexOf(ic), 0);
        QCOMPARE(doc.indexOf(ia), 1);
        QCOMPARE(doc.indexOf(ib), 2);
    }

    void addRejectsNullLayer()
    {
        Document doc(100, 100);
        QVERIFY(!doc.addLayer(nullptr));
    }

    void addRejectsParentOutsideTree()
    {
        Document doc(100, 100);
        auto foreign = makeGroup("foreign");
        QVERIFY(!doc.addLayer(makeText("x"), foreign.get()));
        QCOMPARE(doc.rootGroup()->children.size(), std::size_t(0));
    }

    void takeLayerRemovesAndReturnsOwnership()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        auto b = makeText("B");
        const LayerId ib = b->id();
        QVERIFY(doc.addLayer(std::move(a)));
        QVERIFY(doc.addLayer(std::move(b)));

        auto taken = doc.takeLayer(ib);
        QVERIFY(taken != nullptr);
        QCOMPARE(taken->id(), ib);
        QVERIFY(doc.findLayer(ib) == nullptr);
        QCOMPARE(doc.rootGroup()->children.size(), std::size_t(1));

        QVERIFY(doc.takeLayer(ib) == nullptr);                    // already gone
        QVERIFY(doc.takeLayer(doc.rootGroup()->id()) == nullptr); // root protected
    }

    void removeLayerDeletes()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        const LayerId id = a->id();
        QVERIFY(doc.addLayer(std::move(a)));

        QVERIFY(doc.removeLayer(id));
        QVERIFY(doc.findLayer(id) == nullptr);
        QVERIFY(!doc.removeLayer(id));
        QVERIFY(!doc.removeLayer(newLayerId()));
    }

    void reorderWithinParent()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        auto b = makeText("B");
        auto c = makeText("C");
        const LayerId ia = a->id(), ib = b->id(), ic = c->id();
        QVERIFY(doc.addLayer(std::move(a)));
        QVERIFY(doc.addLayer(std::move(b)));
        QVERIFY(doc.addLayer(std::move(c)));

        QVERIFY(doc.reorderLayer(ic, nullptr, 0)); // C to bottom
        QCOMPARE(doc.indexOf(ic), 0);
        QCOMPARE(doc.indexOf(ia), 1);
        QCOMPARE(doc.indexOf(ib), 2);
    }

    void reorderUsesFinalIndexSemantics()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        auto b = makeText("B");
        auto c = makeText("C");
        const LayerId ia = a->id(), ib = b->id(), ic = c->id();
        QVERIFY(doc.addLayer(std::move(a)));
        QVERIFY(doc.addLayer(std::move(b)));
        QVERIFY(doc.addLayer(std::move(c)));

        // Move A (index 0) to index 2 -> [B, C, A]
        QVERIFY(doc.reorderLayer(ia, nullptr, 2));
        QCOMPARE(doc.indexOf(ib), 0);
        QCOMPARE(doc.indexOf(ic), 1);
        QCOMPARE(doc.indexOf(ia), 2);
    }

    void reorderAcrossParents()
    {
        Document doc(100, 100);
        auto group = makeGroup("G");
        GroupLayer* groupPtr = group.get();
        QVERIFY(doc.addLayer(std::move(group)));

        auto text = makeText("T");
        const LayerId it = text->id();
        QVERIFY(doc.addLayer(std::move(text)));

        QVERIFY(doc.reorderLayer(it, groupPtr, -1));
        QVERIFY(doc.parentOf(it) == groupPtr);
        QCOMPARE(doc.indexOf(it), 0);
        QCOMPARE(groupPtr->children.size(), std::size_t(1));
    }

    void reorderRejectsCycles()
    {
        Document doc(100, 100);
        auto group = makeGroup("G");
        GroupLayer* groupPtr = group.get();
        QVERIFY(doc.addLayer(std::move(group)));

        auto sub = makeGroup("S");
        GroupLayer* subPtr = sub.get();
        QVERIFY(doc.addLayer(std::move(sub), groupPtr));

        // A group cannot be moved into its own subtree...
        QVERIFY(!doc.reorderLayer(groupPtr->id(), subPtr, 0));
        // ...nor into itself.
        QVERIFY(!doc.reorderLayer(subPtr->id(), subPtr, 0));
        // Unknown layers and the root are rejected too.
        QVERIFY(!doc.reorderLayer(newLayerId(), nullptr, 0));
        QVERIFY(!doc.reorderLayer(doc.rootGroup()->id(), nullptr, 0));

        // Tree untouched.
        QCOMPARE(doc.indexOf(groupPtr->id()), 0);
        QCOMPARE(doc.indexOf(subPtr->id()), 0);
    }

    void duplicateTextLayer()
    {
        Document doc(100, 100);
        auto text = makeText("Title");
        text->content = QStringLiteral("Hello");
        text->sizePt = 64.0;
        const LayerId originalId = text->id();
        QVERIFY(doc.addLayer(std::move(text)));

        const LayerId copyId = doc.duplicateLayer(originalId);
        QVERIFY(!copyId.isNull());
        QVERIFY(copyId != originalId);

        auto* copy = static_cast<TextLayer*>(doc.findLayer(copyId));
        QVERIFY(copy != nullptr);
        QCOMPARE(copy->name, QStringLiteral("Title copy"));
        QCOMPARE(copy->content, QStringLiteral("Hello"));
        QCOMPARE(copy->sizePt, 64.0);
        QVERIFY(doc.parentOf(copyId) == doc.parentOf(originalId));
        QCOMPARE(doc.indexOf(copyId), doc.indexOf(originalId) + 1);

        // Original untouched, still below the copy.
        auto* original = static_cast<TextLayer*>(doc.findLayer(originalId));
        QVERIFY(original != nullptr);
        QCOMPARE(original->name, QStringLiteral("Title"));
        QCOMPARE(doc.indexOf(originalId), 0);
    }

    void duplicateGroupDeepWithFreshIds()
    {
        Document doc(100, 100);
        auto group = makeGroup("Chars");
        GroupLayer* groupPtr = group.get();

        auto c1 = makeText("C1");
        auto c2 = makeText("C2");
        const LayerId c1Id = c1->id();
        const LayerId c2Id = c2->id();
        group->children.push_back(std::move(c1));
        group->children.push_back(std::move(c2));
        QVERIFY(doc.addLayer(std::move(group)));

        const LayerId copyId = doc.duplicateLayer(groupPtr->id());
        QVERIFY(!copyId.isNull());
        auto* copyGroup = static_cast<GroupLayer*>(doc.findLayer(copyId));
        QVERIFY(copyGroup != nullptr);
        QCOMPARE(copyGroup->children.size(), std::size_t(2));

        // Every copied node has a fresh id.
        QVERIFY(copyGroup->id() != groupPtr->id());
        QVERIFY(copyGroup->children[0]->id() != c1Id);
        QVERIFY(copyGroup->children[1]->id() != c2Id);
        // Child names preserved (suffix only on the duplicated top level).
        QCOMPARE(copyGroup->children[0]->name, QStringLiteral("C1"));
        QCOMPARE(copyGroup->children[1]->name, QStringLiteral("C2"));
    }

    void duplicateRejectsRootAndUnknown()
    {
        Document doc(100, 100);
        QVERIFY(doc.duplicateLayer(doc.rootGroup()->id()).isNull());
        QVERIFY(doc.duplicateLayer(newLayerId()).isNull());
    }

    void propertySettersEmitOnceOnChange()
    {
        Document doc(100, 100);
        auto text = makeText("T");
        const LayerId id = text->id();
        QVERIFY(doc.addLayer(std::move(text)));

        QSignalSpy spy(&doc, &Document::layerPropertyChanged);
        QVERIFY(spy.isValid());

        QVERIFY(doc.setLayerVisible(id, false));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<QUuid>(), id);

        QVERIFY(doc.setLayerVisible(id, false)); // unchanged -> no signal
        QCOMPARE(spy.count(), 1);

        QVERIFY(doc.setLayerOpacity(id, 2.0f)); // clamps to 1.0
        QCOMPARE(spy.count(), 2);
        QCOMPARE(static_cast<TextLayer*>(doc.findLayer(id))->opacity(), 1.0f);

        QVERIFY(doc.setLayerOpacity(id, 1.0f)); // unchanged -> no signal
        QCOMPARE(spy.count(), 2);

        QVERIFY(doc.setLayerName(id, QStringLiteral("Renamed")));
        QCOMPARE(spy.count(), 3);

        QVERIFY(doc.setLayerLocked(id, true));
        QCOMPARE(spy.count(), 4);

        QVERIFY(doc.setLayerBlendMode(id, BlendMode::Multiply));
        QCOMPARE(spy.count(), 5);

        // Unknown ids: rejected, no signals.
        QVERIFY(!doc.setLayerVisible(newLayerId(), true));
        QVERIFY(!doc.setLayerName(newLayerId(), QStringLiteral("x")));
        QCOMPARE(spy.count(), 5);
    }

    void structureChangedSignalPerOperation()
    {
        Document doc(100, 100);
        QSignalSpy spy(&doc, &Document::structureChanged);
        QVERIFY(spy.isValid());

        auto a = makeText("A");
        const LayerId ia = a->id();
        QVERIFY(doc.addLayer(std::move(a)));
        QCOMPARE(spy.count(), 1);

        QVERIFY(doc.duplicateLayer(newLayerId()).isNull()); // failed -> no signal
        QCOMPARE(spy.count(), 1);

        const LayerId copyId = doc.duplicateLayer(ia);
        QVERIFY(!copyId.isNull());
        QCOMPARE(spy.count(), 2);

        QVERIFY(doc.reorderLayer(ia, nullptr, 0));
        QCOMPARE(spy.count(), 3);

        QVERIFY(doc.removeLayer(copyId));
        QCOMPARE(spy.count(), 4);
    }

    void revisionBumpsOnlyOnEffectiveChange()
    {
        Document doc(100, 100);
        auto a = makeText("A");
        const LayerId id = a->id();
        QVERIFY(doc.addLayer(std::move(a)));
        const quint64 revAfterAdd = doc.revision();

        QVERIFY(doc.setLayerName(id, QStringLiteral("A"))); // same value
        QCOMPARE(doc.revision(), revAfterAdd);

        QVERIFY(doc.setLayerName(id, QStringLiteral("B")));
        QVERIFY(doc.revision() > revAfterAdd);
    }
};

QTEST_GUILESS_MAIN(TestDocument)
#include "test_document.moc"
CC_T_DOC

# --------------------------------------------------------- src/CMakeLists.txt
cat > src/CMakeLists.txt <<'CC_SRC_CMAKE'
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --------------------------------------------------------------------- core --
# Document + layer model. QtCore/QtGui only; fully headless-testable.
add_library(cc_core STATIC
    core/layers/Layer.h
    core/layers/Layer.cpp
    core/Document.h
    core/Document.cpp
)
target_link_libraries(cc_core PUBLIC Qt6::Core Qt6::Gui)
target_include_directories(cc_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
cc_enable_warnings(cc_core)

# ---------------------------------------------------------------- services --
add_library(cc_services STATIC
    services/logservice.h
    services/logservice.cpp
    services/settingsservice.h
    services/settingsservice.cpp
)
target_link_libraries(cc_services PUBLIC Qt6::Core)
target_include_directories(cc_services PUBLIC ${PROJECT_SOURCE_DIR}/src)
cc_enable_warnings(cc_services)

# ------------------------------------------------------------ localization --
add_library(cc_localization STATIC
    localization/i18nservice.h
    localization/i18nservice.cpp
    ${PROJECT_SOURCE_DIR}/resources/locales.qrc
)
target_link_libraries(cc_localization PUBLIC Qt6::Core cc_services)
target_include_directories(cc_localization PUBLIC ${PROJECT_SOURCE_DIR}/src)
cc_enable_warnings(cc_localization)

# ------------------------------------------------------------- application --
add_executable(creatorcanvas
    main.cpp
    ui/darktheme.h
    ui/darktheme.cpp
    ui/mainwindow.h
    ui/mainwindow.cpp
)
target_link_libraries(creatorcanvas
    PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets
            cc_core cc_services cc_localization)
target_include_directories(creatorcanvas PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(creatorcanvas PRIVATE APP_VERSION="${PROJECT_VERSION}")
set_target_properties(creatorcanvas PROPERTIES
    OUTPUT_NAME "CreatorCanvas"
    WIN32_EXECUTABLE $<BOOL:WIN32>
    MACOSX_BUNDLE TRUE)
cc_enable_warnings(creatorcanvas)
CC_SRC_CMAKE

# -------------------------------------------------------- tests/CMakeLists.txt
cat > tests/CMakeLists.txt <<'CC_TESTS_CMAKE'
function(cc_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE Qt6::Test cc_core cc_services cc_localization)
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    cc_enable_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

cc_add_test(test_settings unit/test_settings.cpp)
cc_add_test(test_i18n    unit/test_i18n.cpp)
cc_add_test(test_logging unit/test_logging.cpp)
cc_add_test(test_layers  unit/test_layers.cpp)
cc_add_test(test_document unit/test_document.cpp)
CC_TESTS_CMAKE

# ------------------------------------- CMakePresets.json (inclui fix do M0)
cat > CMakePresets.json <<'CC_PRESETS'
{
  "version": 3,
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "CMAKE_BUILD_TYPE": "Debug"
      }
    },
    {
      "name": "linux-debug",
      "inherits": "base",
      "generator": "Ninja",
      "cacheVariables": { "CC_WARNINGS_AS_ERRORS": "ON" },
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Linux" }
    },
    {
      "name": "linux-asan",
      "inherits": "linux-debug",
      "cacheVariables": {
        "CMAKE_CXX_FLAGS": "-fsanitize=address,undefined -fno-omit-frame-pointer -g",
        "CC_WARNINGS_AS_ERRORS": "OFF"
      }
    },
    {
      "name": "windows-debug",
      "inherits": "base",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" }
    }
  ],
  "buildPresets": [
    { "name": "linux-debug",   "configurePreset": "linux-debug" },
    { "name": "linux-asan",    "configurePreset": "linux-asan" },
    { "name": "windows-debug", "configurePreset": "windows-debug" }
  ]
}
CC_PRESETS

# ------------------------------------------------------- README: status do M1
sed -i 's#^| M1 | Core document/layers     | Pending |$#| M1 | Core document/layers     | Code complete |#' README.md \
  && echo ">> README updated" || echo ">> README row not found (update manually if needed)"

echo ""
echo ">> M1 applied. Files created/updated:"
find src/core tests/unit -type f | sort
echo ""
echo ">> Next:"
echo "   cmake --preset linux-debug"
echo "   cmake --build --preset linux-debug"
echo "   ctest --test-dir build/linux-debug --output-on-failure"
