#include "dockvisibilitypolicy.h"

namespace cc {

void DockVisibilityPolicy::setDocks(QDockWidget* layers, QDockWidget* text,
                                    QDockWidget* shape, QDockWidget* image,
                                    QDockWidget* history,
                                    QDockWidget* navigator,
                                    QDockWidget* swatches)
{
    m_layers = layers;
    m_secondary = {text, shape, image, history, navigator, swatches};
}

void DockVisibilityPolicy::apply(bool ultraCompact)
{
    if (m_layers)
        m_layers->setVisible(true);

    if (ultraCompact) {
        if (!m_autoHidden) {
            m_savedVisibility.clear();
            for (QDockWidget* panel : m_secondary) {
                if (panel)
                    m_savedVisibility.append({panel, !panel->isHidden()});
            }
            m_autoHidden = true;
        }
        for (QDockWidget* panel : m_secondary) {
            if (panel && panel->isVisible())
                panel->hide();
        }
        return;
    }

    if (!m_autoHidden)
        return;

    for (const auto& entry : m_savedVisibility) {
        if (entry.first)
            entry.first->setVisible(entry.second);
    }
    m_savedVisibility.clear();
    m_autoHidden = false;
}

} // namespace cc
