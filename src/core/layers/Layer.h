#pragma once

#include <QColor>
#include <QPolygonF>
#include <QString>
#include <QUuid>

#include <memory>
#include <vector>

namespace cc {
class ProjectReader; // defined in serialization/ProjectFile.cpp

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
    friend class ProjectReader;
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
