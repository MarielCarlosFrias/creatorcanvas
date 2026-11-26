#pragma once

#include <QDockWidget>
#include <QList>
#include <QPair>

namespace cc {

/// Preserva a escolha do usuário ao esconder inspetores em janelas muito
/// estreitas e restaura-a quando o espaço volta a ser suficiente.
class DockVisibilityPolicy final
{
public:
    void setDocks(QDockWidget* layers, QDockWidget* text,
                  QDockWidget* shape, QDockWidget* image,
                  QDockWidget* history,
                  QDockWidget* navigator = nullptr,
                  QDockWidget* swatches = nullptr);
    void apply(bool ultraCompact);

private:
    QDockWidget* m_layers = nullptr;
    QList<QDockWidget*> m_secondary;
    QList<QPair<QDockWidget*, bool>> m_savedVisibility;
    bool m_autoHidden = false;
};

} // namespace cc
