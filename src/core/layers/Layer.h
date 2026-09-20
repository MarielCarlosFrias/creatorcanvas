#pragma once

#include <QColor>
#include <QPolygonF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QUuid>

#include <memory>
#include <vector>

#include "../geometry/AffineTransform.h"
#include "ImageEffects.h"
#include "TextEffects.h"

namespace cc {

class ProjectReader; // defined in serialization/ProjectFile.cpp

using LayerId = QUuid;

inline LayerId newLayerId() { return QUuid::createUuid(); }

enum class LayerType { Group, Image, Text, Shape, Background };
enum class BlendMode { Normal, Multiply, Screen, Overlay, Darken, Lighten, Add };
enum class TextAlignment { Left, Center, Right };
enum class ShapeKind { Rectangle, RoundedRect, Ellipse, Line, Polygon, ArrowRight, ArrowCurved, Star, Badge };

/// Base class of every layer. UI/commands mutate layers ONLY through
/// Document methods; direct field access is the read path (renderer,
/// serializer) and interactive gestures (which push one command at release).
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

    AffineTransform transform;

    float opacity() const { return m_opacity; }
    void setOpacity(float value); // clamps to [0, 1]

    /// Content bounds in LOCAL (untransformed) coordinates. Empty rect for
    /// layers without intrinsic geometry (Background).
    virtual QRectF contentBounds() const;

    virtual std::unique_ptr<Layer> deepCopy() const = 0;

protected:
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
    QRectF contentBounds() const override;

    std::vector<std::unique_ptr<Layer>> children; // index 0 = bottom
};

class ImageLayer final : public Layer
{
public:
    ImageLayer();
    std::unique_ptr<Layer> deepCopy() const override;
    QRectF contentBounds() const override;

    LayerId assetId;
    int naturalWidth = 0;
    int naturalHeight = 0;
    ImageEffects effects;
};

class TextLayer final : public Layer
{
public:
    TextLayer();
    std::unique_ptr<Layer> deepCopy() const override;
    QRectF contentBounds() const override;

    QString content;
    QString fontFamily = QStringLiteral("Sans Serif");
    double sizePt = 48.0;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    QColor color = QColor(17, 17, 17);
    double letterSpacingPx = 0.0;
    double lineHeightMult = 1.0;
    QSizeF box; // wrap box; empty = auto-size from font metrics
    TextAlignment align = TextAlignment::Center;
    TextEffects effects;
};

class ShapeLayer final : public Layer
{
public:
    ShapeLayer();
    std::unique_ptr<Layer> deepCopy() const override;
    QRectF contentBounds() const override;

    ShapeKind kind = ShapeKind::Rectangle;
    QColor fill = Qt::white;
    QColor stroke = Qt::transparent;
    double strokeWidth = 0.0;
    double cornerRadius = 0.0;
    QPolygonF points;
};

class BackgroundLayer final : public Layer
{
public:
    BackgroundLayer();
    std::unique_ptr<Layer> deepCopy() const override;

    QColor fill = Qt::white;
};

std::unique_ptr<Layer> makeLayer(LayerType type);

} // namespace cc
