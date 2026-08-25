#include "Layer.h"

#include <QFontMetrics>
#include <QGuiApplication>

#include <algorithm>

namespace cc {

Layer::Layer(LayerType type)
    : m_type(type), m_id(newLayerId()) {}

Layer::~Layer() = default;

void Layer::setOpacity(float value)
{
    m_opacity = std::clamp(value, 0.0f, 1.0f);
}

QRectF Layer::contentBounds() const
{
    return QRectF(); // no intrinsic geometry by default
}

void Layer::copyCommonFrom(const Layer& other)
{
    m_id = newLayerId(); // duplicates always get a fresh identity
    name = other.name;
    visible = other.visible;
    locked = other.locked;
    blendMode = other.blendMode;
    transform = other.transform;
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

QRectF GroupLayer::contentBounds() const
{
    QRectF united;
    for (const auto& child : children) {
        const QRectF bounds = child->contentBounds();
        if (bounds.isEmpty())
            continue;
        const QRectF mapped = child->transform.matrix(bounds).mapRect(bounds);
        united = united.isNull() ? mapped : united.united(mapped);
    }
    return united;
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

QRectF ImageLayer::contentBounds() const
{
    return QRectF(0, 0, naturalWidth, naturalHeight);
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

QRectF TextLayer::contentBounds() const
{
    // Font metrics need the GUI toolkit; headless contexts get an estimate.
    if (!QGuiApplication::instance())
        return QRectF(0, 0, 100, 50);
    QFont font(fontFamily);
    font.setBold(bold);
    font.setItalic(italic);
    font.setPointSizeF(sizePt > 0 ? sizePt : 1.0);
    const QFontMetrics metrics(font);
    return metrics.boundingRect(content);
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

QRectF ShapeLayer::contentBounds() const
{
    return points.size() >= 2 ? points.boundingRect() : QRectF();
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
