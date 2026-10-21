#pragma once

#include <QImage>
#include <QRect>
#include <QString>

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

    /// Photocraft/Photoshop core behavior: can this layer's pixels be
    /// edited right now? Checks effective locks (union with ancestors)
    /// AND the legacy `locked` flag. Returns false with a reason when
    /// blocked — every mutation path (filters, adjustments, tools, AI)
    /// must call this BEFORE touching pixels.
    static bool canEditPixels(const Document& doc, const LayerId& id,
                              QString* whyNot = nullptr);

    /// Photocraft/Photoshop core behavior: composite the modified image
    /// with the original using the active selection as a mask.
    /// Where selection=255 use modified, where selection=0 keep original,
    /// in between lerp. Null or size-mismatched selection = modified as-is
    /// (no active selection = the edit applies to everything, as in PS
    /// when there is no selection). This is what makes "make a selection
    /// then apply a filter" only affect the selected area.
    static QImage applySelectionMask(const QImage& original,
                                     const QImage& modified,
                                     const QImage& selection);
};

} // namespace cc
