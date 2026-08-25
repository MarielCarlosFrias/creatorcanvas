#include "Layer.h"

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
