#pragma once

#include <QDockWidget>

#include <initializer_list>

namespace cc::ResponsiveLayout {

/// Applies the minimum width required by a dock's actual content while
/// preserving the responsive breakpoint selected by MainWindow.
inline void setDockMinimumWidths(std::initializer_list<QDockWidget*> docks,
                                 int preferredWidth,
                                 bool ultraCompact)
{
    for (QDockWidget* dock : docks) {
        if (!dock)
            continue;
        const int contentWidth = dock->widget()
            ? dock->widget()->minimumSizeHint().width() : 0;
        dock->setMinimumWidth(ultraCompact ? 180
                                           : qMax(preferredWidth, contentWidth));
    }
}

} // namespace cc::ResponsiveLayout
