#pragma once

#include <QImage>
#include <QRect>

#include "core/Document.h"

namespace cc {

class CommandStack;

/// Single entry point for non-destructive operations on the active pixel
/// selection. UI widgets and tools must not compose layer masks themselves.
class SelectionOperations final
{
public:
    static QImage inverted(const QImage& selection);
    static bool extract(const QImage& source, const QImage& selection,
                        QImage* image, QRect* bounds);
    static bool extractToLayer(Document& document, CommandStack* history,
                               const LayerId& sourceId, bool cut,
                               LayerId* createdId = nullptr);
    static bool applyMask(Document& document, CommandStack* history,
                          const LayerId& layerId, bool keepInside);
};

} // namespace cc
