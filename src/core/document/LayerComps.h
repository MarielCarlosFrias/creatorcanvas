#pragma once

#include <QString>
#include <QPointF>
#include <QUuid>
#include <vector>
#include <optional>
#include "../layers/Layer.h"

namespace cc {

/// Ported from PhotoCraft's CompLayerState (crates/doc/src/comps.rs)
struct CompLayerState
{
    LayerId layerId;
    std::optional<bool> visible;
    std::optional<QPointF> position;
    std::optional<float> opacity;
    std::optional<BlendMode> blendMode;

    bool operator==(const CompLayerState& o) const
    {
        return layerId == o.layerId
            && visible == o.visible
            && position == o.position
            && opacity == o.opacity
            && blendMode == o.blendMode;
    }
};

/// Ported from PhotoCraft's LayerComp (crates/doc/src/comps.rs)
struct LayerComp
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString name;
    QString comment;
    bool applyVisibility = true;
    bool applyPosition = true;
    bool applyAppearance = true;
    std::vector<CompLayerState> states;

    bool operator==(const LayerComp& o) const
    {
        return id == o.id && name == o.name && states == o.states;
    }
};

} // namespace cc
