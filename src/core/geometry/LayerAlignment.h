#pragma once

#include "AffineTransform.h"
#include "core/layers/Layer.h"
#include <QList>

namespace cc {

class Document;
class CommandStack;

enum class AlignTarget {
    Left,
    CenterX,
    Right,
    Top,
    CenterY,
    Bottom,
    CenterBoth
};

class LayerAlignment
{
public:
    static bool alignLayer(Document& doc, CommandStack* history, const LayerId& id, AlignTarget target);
    static bool alignLayers(Document& doc, CommandStack* history, const QList<LayerId>& ids, AlignTarget target);
    static bool distributeHorizontally(Document& doc, CommandStack* history, const QList<LayerId>& ids);
    static bool distributeVertically(Document& doc, CommandStack* history, const QList<LayerId>& ids);
};

} // namespace cc
