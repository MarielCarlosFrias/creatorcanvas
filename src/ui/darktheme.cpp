#include "darktheme.h"

#include <QApplication>
#include <QColor>
#include <QPalette>

namespace cc {

void applyDarkTheme(QApplication &app)
{
    app.setStyle(QStringLiteral("Fusion"));

    const QColor window(0x212226);
    const QColor base(0x17181b);
    const QColor alternate(0x1d1e22);
    const QColor text(0xe7e8ea);
    const QColor dimmed(0x707379);

    QPalette pal;
    pal.setColor(QPalette::Window, window);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Base, base);
    pal.setColor(QPalette::AlternateBase, alternate);
    pal.setColor(QPalette::ToolTipBase, QColor(0x101114));
    pal.setColor(QPalette::ToolTipText, text);
    pal.setColor(QPalette::PlaceholderText, dimmed);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::Button, QColor(0x2b2c31));
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Link, QColor(0x5c9bff));
    pal.setColor(QPalette::LinkVisited, QColor(0x9a7bff));
    pal.setColor(QPalette::Highlight, QColor(0x2f6fed));
    pal.setColor(QPalette::HighlightedText, Qt::white);

    pal.setColor(QPalette::Disabled, QPalette::Text, dimmed);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, dimmed);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, dimmed);
    pal.setColor(QPalette::Disabled, QPalette::Base, window);

    app.setPalette(pal);
}

} // namespace cc
