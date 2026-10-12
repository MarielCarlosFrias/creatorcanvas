#include "darktheme.h"
#include "designtokens.h"

#include <QApplication>
#include <QColor>
#include <QPalette>

namespace cc {

namespace { inline QString hx(const QColor &c){ return c.name(); } }

void applyDarkTheme(QApplication &app)
{
    app.setStyle(QStringLiteral("Fusion"));

    const QColor window = Design::DElevated();
    const QColor base = Design::DSurface();
    const QColor alternate = Design::DCard();
    const QColor text = Design::DText();
    const QColor dimmed = Design::DTextDim();

    QPalette pal;
    pal.setColor(QPalette::Window, window);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Base, base);
    pal.setColor(QPalette::AlternateBase, alternate);
    pal.setColor(QPalette::ToolTipBase, QColor(0x10, 0x11, 0x14));
    pal.setColor(QPalette::ToolTipText, text);
    pal.setColor(QPalette::PlaceholderText, dimmed);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::Button, QColor(0x2b, 0x2c, 0x31));
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Link, QColor(0x5c, 0x9b, 0xff));
    pal.setColor(QPalette::LinkVisited, QColor(0x9a, 0x7b, 0xff));
    pal.setColor(QPalette::Highlight, QColor(0x2f, 0x6f, 0xed));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::Disabled, QPalette::Text, dimmed);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, dimmed);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, dimmed);
    pal.setColor(QPalette::Disabled, QPalette::Base, window);
    app.setPalette(pal);

    const QString cElev = hx(Design::DElevated());
    const QString cSurf = hx(Design::DSurface());
    const QString cText = hx(Design::DText());
    const QString cMuted = hx(Design::DTextMuted());
    const QString cBorder = hx(Design::DBorder());
    const QString cAccent = hx(Design::Accent());
    const QString cWindow = hx(Design::DWindow());
    const QString cCard = hx(Design::DCard());

    QString s;
    s += QStringLiteral("QMenuBar { background-color: ") + cElev + QStringLiteral("; color: ") + cText + QStringLiteral("; border-bottom: 1px solid ") + cBorder + QStringLiteral("; }");
    s += QStringLiteral("QMenuBar::item { background: transparent; padding: 4px 8px; color: ") + cText + QStringLiteral("; }");
    s += QStringLiteral("QMenuBar::item:selected { background-color: ") + cBorder + QStringLiteral("; border-radius: 4px; }");
    s += QStringLiteral("QMenu { background-color: ") + cElev + QStringLiteral("; color: ") + cText + QStringLiteral("; border: 1px solid ") + cBorder + QStringLiteral("; padding: 4px; }");
    s += QStringLiteral("QMenu::item:selected { background-color: ") + cAccent + QStringLiteral("; color: #ffffff; border-radius: 4px; }");
    s += QStringLiteral("QMenu::separator { height: 1px; background: ") + cBorder + QStringLiteral("; margin: 4px 0px; }");
    s += QStringLiteral("QStatusBar { background-color: ") + cSurf + QStringLiteral("; color: ") + cMuted + QStringLiteral("; border-top: 1px solid ") + cBorder + QStringLiteral("; }");
    s += QStringLiteral("QToolBar { background-color: ") + cSurf + QStringLiteral("; border: none; }");
    s += QStringLiteral("QToolBar#ToolsToolBar { background-color: ") + cSurf + QStringLiteral("; border-right: 1px solid ") + cBorder + QStringLiteral("; padding: 4px 2px; spacing: 4px; }");
    s += QStringLiteral("QToolBar#QuickAccessToolBar { background-color: ") + cSurf + QStringLiteral("; border-bottom: 1px solid ") + cBorder + QStringLiteral("; padding: 3px 6px; spacing: 3px; }");
    s += QStringLiteral("QToolBar#ToolOptionsBar { background-color: ") + cSurf + QStringLiteral("; border-bottom: 1px solid ") + cBorder + QStringLiteral("; padding: 3px 6px; spacing: 3px; }");
    s += QStringLiteral("QToolButton { background-color: transparent; color: ") + cText + QStringLiteral("; border: 1px solid transparent; border-radius: 4px; padding: 4px; }");
    s += QStringLiteral("QToolButton:hover { background-color: ") + cBorder + QStringLiteral("; border-color: #3b4252; }");
    s += QStringLiteral("QToolButton:pressed { background-color: #1a1d24; }");
    s += QStringLiteral("QToolButton:checked { background-color: ") + cAccent + QStringLiteral("; color: #ffffff; border-color: #4b8df2; }");
    s += QStringLiteral("QToolButton#CollapsibleHeaderButton { border: none; background-color: ") + cBorder + QStringLiteral("; color: #e0e0e0; font-weight: 600; font-size: 11px; text-align: left; padding: 6px 8px; border-radius: 4px; }");
    s += QStringLiteral("QToolButton#CollapsibleHeaderButton:hover { background-color: #353b49; color: #ffffff; }");
    s += QStringLiteral("QToolButton#CollapsibleHeaderButton:pressed { background-color: #242832; }");
    s += QStringLiteral("QToolButton:disabled { opacity: 0.35; }");
    s += QStringLiteral("QTabBar::tab { min-width: 68px; padding: 5px 12px; background: #282a2d; color: ") + cMuted + QStringLiteral("; border: 1px solid #3b4252; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; margin-right: 2px; }");
    s += QStringLiteral("QTabBar::tab:selected { background: ") + cAccent + QStringLiteral("; color: #ffffff; border-color: ") + cAccent + QStringLiteral("; font-weight: bold; }");
    s += QStringLiteral("QTabBar::tab:hover:!selected { background: #34373d; color: ") + cText + QStringLiteral("; }");
    s += QStringLiteral("QDockWidget { background-color: ") + cElev + QStringLiteral("; color: ") + cText + QStringLiteral("; border: 1px solid ") + cBorder + QStringLiteral("; }");
    s += QStringLiteral("QDockWidget::title { background-color: ") + cSurf + QStringLiteral("; padding: 5px 8px; border-bottom: 1px solid ") + cBorder + QStringLiteral("; text-align: left; color: ") + cText + QStringLiteral("; }");
    s += QStringLiteral("QListWidget#LayerListWidget { background-color: ") + cSurf + QStringLiteral("; border: 1px solid #2d333f; border-radius: 6px; padding: 2px; }");
    s += QStringLiteral("QListWidget#LayerListWidget::item { border-radius: 4px; border: 1px solid transparent; padding: 2px; margin: 1px 0px; }");
    s += QStringLiteral("QListWidget#LayerListWidget::item:hover { background-color: #282d38; }");
    s += QStringLiteral("QListWidget#LayerListWidget::item:selected { background-color: #2b3a52; border: 1px solid ") + cAccent + QStringLiteral("; }");
    s += QStringLiteral("QListWidget#HistoryListWidget { background-color: #1a1d24; border: 1px solid ") + cBorder + QStringLiteral("; border-radius: 6px; outline: none; padding: 4px; }");
    s += QStringLiteral("QListWidget#HistoryListWidget::item { border-radius: 4px; padding: 6px 8px; margin-bottom: 2px; color: #dcdcdc; }");
    s += QStringLiteral("QListWidget#HistoryListWidget::item:hover { background-color: #262b36; }");
    s += QStringLiteral("QListWidget#HistoryListWidget::item:selected { background-color: ") + cAccent + QStringLiteral("; color: #ffffff; font-weight: bold; }");
    s += QStringLiteral("QMainWindow { background-color: ") + cWindow + QStringLiteral("; }");
    s += QStringLiteral("QToolBar { min-height: 34px; }");
    s += QStringLiteral("QToolBar#QuickAccessToolBar { background-color: ") + cSurf + QStringLiteral("; padding: 5px 10px; spacing: 5px; }");
    s += QStringLiteral("QToolBar#ToolsToolBar { background-color: ") + cSurf + QStringLiteral("; padding: 8px 5px; spacing: 6px; }");
    s += QStringLiteral("QToolBar#ToolsToolBar QToolButton { min-width: 34px; min-height: 34px; border-radius: 6px; }");
    s += QStringLiteral("QToolBar#ToolOptionsBar { background-color: #20242c; border-bottom: 1px solid #343b48; min-height: 38px; }");
    s += QStringLiteral("QDockWidget { background-color: ") + cSurf + QStringLiteral("; border: 1px solid #303744; }");
    s += QStringLiteral("QDockWidget::title { background-color: #20242c; padding: 8px 10px; font-weight: 600; }");
    s += QStringLiteral("QTabBar { background-color: ") + cSurf + QStringLiteral("; qproperty-drawBase: 0; }");
    s += QStringLiteral("QTabBar::tab { min-width: 78px; padding: 8px 14px; background: #20242c; border: none; border-right: 1px solid #303744; border-radius: 0px; margin: 0px; }");
    s += QStringLiteral("QTabBar::tab:selected { background: ") + cAccent + QStringLiteral("; color: #ffffff; border-bottom: 2px solid #79b4ff; }");
    s += QStringLiteral("QStatusBar { min-height: 26px; padding: 0 8px; }");
    app.setStyleSheet(s);
}

void applyLightTheme(QApplication &app)
{
    app.setStyle(QStringLiteral("Fusion"));
    const QColor window = Design::LWindow();
    const QColor base = Design::LSurface();
    const QColor alternate(0xe8, 0xec, 0xf1);
    const QColor text = Design::LText();
    const QColor dimmed = Design::LTextMuted();
    QPalette pal;
    pal.setColor(QPalette::Window, window);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Base, base);
    pal.setColor(QPalette::AlternateBase, alternate);
    pal.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xff));
    pal.setColor(QPalette::ToolTipText, text);
    pal.setColor(QPalette::PlaceholderText, dimmed);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::Button, QColor(0xe5, 0xe7, 0xeb));
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Link, QColor(0x25, 0x63, 0xeb));
    pal.setColor(QPalette::LinkVisited, QColor(0x7c, 0x3a, 0xed));
    pal.setColor(QPalette::Highlight, QColor(0x2b, 0x78, 0xe4));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::Disabled, QPalette::Text, dimmed);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, dimmed);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, dimmed);
    pal.setColor(QPalette::Disabled, QPalette::Base, window);
    app.setPalette(pal);
    app.setStyleSheet(QStringLiteral(
        "QMenuBar { background-color: #f1f3f6; color: #1e222b; border-bottom: 1px solid #d1d5db; }"
        "QMenuBar::item { background: transparent; padding: 4px 8px; color: #1e222b; }"
        "QMenuBar::item:selected { background-color: #e2e8f0; border-radius: 4px; }"
        "QMenu { background-color: #ffffff; color: #1e222b; border: 1px solid #d1d5db; padding: 4px; }"
        "QMenu::item:selected { background-color: #2b78e4; color: #ffffff; border-radius: 4px; }"
        "QMenu::separator { height: 1px; background: #e2e8f0; margin: 4px 0px; }"
        "QStatusBar { background-color: #e8ecf1; color: #4b5563; border-top: 1px solid #d1d5db; }"
        "QToolBar { background-color: #f1f3f6; border: none; }"
        "QToolBar#ToolsToolBar { background-color: #f1f3f6; border-right: 1px solid #d1d5db; padding: 4px 2px; spacing: 4px; }"
        "QToolBar#QuickAccessToolBar { background-color: #f1f3f6; border-bottom: 1px solid #d1d5db; padding: 3px 6px; spacing: 3px; }"
        "QToolBar#ToolOptionsBar { background-color: #f1f3f6; border-bottom: 1px solid #d1d5db; padding: 3px 6px; spacing: 3px; }"
        "QToolButton { background-color: transparent; color: #1e222b; border: 1px solid transparent; border-radius: 4px; padding: 4px; }"
        "QToolButton:hover { background-color: #e2e8f0; border-color: #cbd5e1; }"
        "QToolButton:pressed { background-color: #cbd5e1; }"
        "QToolButton:checked { background-color: #2b78e4; color: #ffffff; border-color: #1d4ed8; }"
        "QToolButton#CollapsibleHeaderButton { border: 1px solid #d1d5db; background-color: #f1f5f9; color: #1e293b; font-weight: 600; font-size: 11px; text-align: left; padding: 6px 8px; border-radius: 4px; }"
        "QToolButton#CollapsibleHeaderButton:hover { background-color: #e2e8f0; color: #0f172a; }"
        "QToolButton#CollapsibleHeaderButton:pressed { background-color: #cbd5e1; }"
        "QToolButton:disabled { opacity: 0.35; }"
        "QTabBar::tab { min-width: 68px; padding: 5px 12px; background: #e2e8f0; color: #4b5563; border: 1px solid #cbd5e1; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #2b78e4; color: #ffffff; border-color: #2b78e4; font-weight: bold; }"
        "QTabBar::tab:hover:!selected { background: #f1f5f9; color: #1e222b; }"
        "QDockWidget { background-color: #ffffff; color: #1e222b; border: 1px solid #d1d5db; }"
        "QDockWidget::title { background-color: #e8ecf1; padding: 5px 8px; border-bottom: 1px solid #d1d5db; text-align: left; color: #1e222b; }"
        "QListWidget#LayerListWidget { background-color: #ffffff; border: 1px solid #cbd5e1; border-radius: 6px; padding: 2px; }"
        "QListWidget#LayerListWidget::item { border-radius: 4px; border: 1px solid transparent; padding: 2px; margin: 1px 0px; }"
        "QListWidget#LayerListWidget::item:hover { background-color: #f1f5f9; }"
        "QListWidget#LayerListWidget::item:selected { background-color: #dbeafe; border: 1px solid #2b78e4; }"
        "QListWidget#HistoryListWidget { background-color: #ffffff; border: 1px solid #cbd5e1; border-radius: 6px; outline: none; padding: 4px; }"
        "QListWidget#HistoryListWidget::item { border-radius: 4px; padding: 6px 8px; margin-bottom: 2px; color: #1e222b; }"
        "QListWidget#HistoryListWidget::item:hover { background-color: #f1f5f9; }"
        "QListWidget#HistoryListWidget::item:selected { background-color: #2b78e4; color: #ffffff; font-weight: bold; }"
        "QMainWindow { background-color: #e5e9ef; }"
        "QToolBar { min-height: 34px; }"
        "QToolBar#QuickAccessToolBar { padding: 5px 10px; spacing: 5px; }"
        "QToolBar#ToolsToolBar { padding: 8px 5px; spacing: 6px; }"
        "QToolBar#ToolsToolBar QToolButton { min-width: 34px; min-height: 34px; border-radius: 6px; }"
        "QToolBar#ToolOptionsBar { background-color: #f8fafc; min-height: 38px; border-bottom: 1px solid #cbd5e1; }"
        "QDockWidget::title { padding: 8px 10px; font-weight: 600; }"
        "QTabBar::tab { min-width: 78px; padding: 8px 14px; border: none; border-right: 1px solid #cbd5e1; border-radius: 0px; margin: 0px; }"
        "QTabBar::tab:selected { border-bottom: 2px solid #79b4ff; }"
        "QStatusBar { min-height: 26px; padding: 0 8px; }"
    ));
}

void applyTheme(QApplication &app, bool darkMode)
{
    if (darkMode) applyDarkTheme(app);
    else applyLightTheme(app);
}

} // namespace cc
