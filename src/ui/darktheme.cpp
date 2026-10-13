#include "darktheme.h"
#include "designtokens.h"

#include <QApplication>
#include <QColor>
#include <QPalette>

namespace cc {
namespace {

struct Skin {
    QString base0, base1, base2, base3, hov, press, line;
    QString textHi, textMd, textLo;
    QString accent, accentHover, accentPress;
};

QString hx(const QColor& c) { return c.name(); }

Skin darkSkin()
{
    Skin k;
    k.base0 = hx(Design::Base0());   k.base1 = hx(Design::Base1());
    k.base2 = hx(Design::Base2());  k.base3 = hx(Design::Base3());
    k.hov   = hx(Design::Hover());  k.press = hx(Design::Press());
    k.line  = hx(Design::Line());
    k.textHi = hx(Design::TextHi()); k.textMd = hx(Design::TextMd());
    k.textLo = hx(Design::TextLo());
    k.accent      = hx(Design::Accent());
    k.accentHover = hx(Design::AccentHover());
    k.accentPress = hx(Design::AccentPress());
    return k;
}

Skin lightSkin()
{
    Skin k;
    k.base0 = hx(Design::LBase0());  k.base1 = hx(Design::LBase1());
    k.base2 = hx(Design::LBase2());  k.base3 = hx(Design::LBase3());
    k.hov   = hx(Design::LHover());  k.press = hx(Design::LPress());
    k.line  = hx(Design::LLine());
    k.textHi = hx(Design::LTextHi()); k.textMd = hx(Design::LTextMd());
    k.textLo = hx(Design::LTextLo());
    k.accent      = hx(Design::Accent());
    k.accentHover = hx(Design::AccentHover());
    k.accentPress = hx(Design::AccentPress());
    return k;
}

QString buildQss(const Skin& k)
{
    QString s;

    // -- Janela e chrome --
    s += "QMainWindow { background-color: " + k.base0 + "; }";
    s += "QMainWindow::separator { background-color: " + k.line + "; width: 1px; height: 1px; }";
    s += "QMenuBar { background-color: " + k.base1 + "; color: " + k.textHi + "; border-bottom: 1px solid " + k.line + "; }";
    s += "QMenuBar::item { background: transparent; padding: 5px 9px; color: " + k.textHi + "; border-radius: 4px; }";
    s += "QMenuBar::item:selected { background-color: " + k.hov + "; }";
    s += "QMenu { background-color: " + k.base2 + "; color: " + k.textHi + "; border: 1px solid " + k.line + "; padding: 6px; border-radius: 6px; }";
    s += "QMenu::item { padding: 6px 24px 6px 12px; border-radius: 4px; }";
    s += "QMenu::item:selected { background-color: " + k.accent + "; color: #ffffff; }";
    s += "QMenu::item:disabled { color: " + k.textLo + "; }";
    s += "QMenu::separator { height: 1px; background: " + k.line + "; margin: 5px 8px; }";
    s += "QStatusBar { background-color: " + k.base1 + "; color: " + k.textMd + "; border-top: 1px solid " + k.line + "; min-height: 26px; }";

    // -- Toolbars --
    s += "QToolBar { background-color: " + k.base1 + "; border: none; min-height: 40px; }";
    s += "QToolBar#ToolsToolBar { border-right: 1px solid " + k.line + "; padding: 6px 4px; spacing: 4px; }";
    s += "QToolBar#ToolsToolBar QToolButton { min-width: 34px; min-height: 34px; border-radius: 6px; }";
    s += "QToolBar#QuickAccessToolBar { border-bottom: 1px solid " + k.line + "; padding: 4px 10px; spacing: 4px; }";
    s += "QToolBar#ToolOptionsBar { background-color: " + k.base2 + "; border-bottom: 1px solid " + k.line + "; min-height: 38px; }";

    // -- ToolButtons --
    s += "QToolButton { background: transparent; color: " + k.textHi + "; border: 1px solid transparent; border-radius: 5px; padding: 4px; }";
    s += "QToolButton:hover { background-color: " + k.hov + "; }";
    s += "QToolButton:pressed { background-color: " + k.press + "; }";
    s += "QToolButton:checked { background-color: " + k.accent + "; color: #ffffff; }";
    s += "QToolButton:disabled { color: " + k.textLo + "; }";
    s += "QToolButton#CollapsibleHeaderButton { background-color: " + k.base3 + "; color: " + k.textHi + "; font-weight: 600; font-size: 11px; text-align: left; padding: 6px 8px; border-radius: 4px; }";
    s += "QToolButton#CollapsibleHeaderButton:hover { background-color: " + k.hov + "; }";
    s += "QToolButton#CollapsibleHeaderButton:pressed { background-color: " + k.press + "; }";

    // -- Tabs: estilo PRO = underline 2px (NAO fundo azul cheio) --
    s += "QTabBar { background-color: " + k.base1 + "; qproperty-drawBase: 0; qproperty-usesScrollButtons: true; }";
    s += "QTabBar::tab { background: transparent; color: " + k.textMd + "; padding: 7px 12px; margin: 0; border: none; border-bottom: 2px solid transparent; min-width: 48px; }";
    s += "QTabBar::tab:hover { color: " + k.textHi + "; }";
    s += "QTabBar::tab:selected { color: " + k.accent + "; border-bottom: 2px solid " + k.accent + "; font-weight: 600; }";
    s += "QTabWidget::pane { border-top: 1px solid " + k.line + "; }";

    // -- Docks --
    s += "QDockWidget { color: " + k.textHi + "; border: 1px solid " + k.line + "; }";
    s += "QDockWidget::title { background-color: " + k.base1 + "; padding: 7px 10px; text-align: left; color: " + k.textMd + "; font-weight: 600; font-size: 11px; border-bottom: 1px solid " + k.line + "; }";

    // -- Listas (Layers/History cobertos pelo estilo global) --
    s += "QListWidget { background-color: " + k.base2 + "; border: 1px solid " + k.line + "; border-radius: 6px; padding: 3px; outline: none; }";
    s += "QListWidget::item { border-radius: 4px; padding: 6px 8px; color: " + k.textHi + "; }";
    s += "QListWidget::item:hover { background-color: " + k.hov + "; }";
    s += "QListWidget::item:selected { background-color: " + k.base3 + "; color: " + k.textHi + "; border-left: 2px solid " + k.accent + "; }";

    // -- Scrollbars 8px transparentes --
    s += "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }";
    s += "QScrollBar::handle:vertical { background: " + k.base3 + "; border-radius: 4px; min-height: 24px; }";
    s += "QScrollBar::handle:vertical:hover { background: " + k.hov + "; }";
    s += "QScrollBar:horizontal { background: transparent; height: 8px; margin: 0; }";
    s += "QScrollBar::handle:horizontal { background: " + k.base3 + "; border-radius: 4px; min-width: 24px; }";
    s += "QScrollBar::handle:horizontal:hover { background: " + k.hov + "; }";
    s += "QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }";
    s += "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }";

    // -- Inputs + FOCUS RING (acessibilidade) --
    s += "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit { background-color: " + k.base1 + "; color: " + k.textHi + "; border: 1px solid " + k.line + "; border-radius: 5px; padding: 4px 8px; selection-background-color: " + k.accent + "; }";
    s += "QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover { border-color: " + k.hov + "; }";
    s += "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus { border: 1px solid " + k.accent + "; }";
    s += "QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled { color: " + k.textLo + "; }";
    s += "QComboBox::drop-down { border: none; width: 22px; }";
    s += "QComboBox QAbstractItemView { background-color: " + k.base2 + "; color: " + k.textHi + "; border: 1px solid " + k.line + "; selection-background-color: " + k.accent + "; selection-color: #ffffff; outline: none; border-radius: 5px; }";

    // -- Botoes padrao + variantes por objectName --
    s += "QPushButton { background-color: " + k.base3 + "; color: " + k.textHi + "; border: 1px solid " + k.line + "; border-radius: 5px; padding: 6px 12px; }";
    s += "QPushButton:hover { background-color: " + k.hov + "; }";
    s += "QPushButton:pressed { background-color: " + k.press + "; }";
    s += "QPushButton:disabled { color: " + k.textLo + "; background-color: " + k.base2 + "; }";
    s += "QPushButton#PrimaryButton { background-color: " + k.accent + "; border-color: " + k.accent + "; color: #ffffff; font-weight: 600; }";
    s += "QPushButton#PrimaryButton:hover { background-color: " + k.accentHover + "; border-color: " + k.accentHover + "; }";
    s += "QPushButton#PrimaryButton:pressed { background-color: " + k.accentPress + "; }";
    s += "QPushButton#SecondaryButton { background-color: " + k.base3 + "; color: " + k.textHi + "; }";
    s += "QPushButton#GhostButton { background: transparent; border: 1px dashed " + k.line + "; color: " + k.textMd + "; }";
    s += "QPushButton#GhostButton:hover { color: " + k.textHi + "; border-color: " + k.accent + "; }";
    s += "QPushButton#PresetCard { background-color: " + k.base3 + "; border: 1px solid " + k.line + "; border-radius: 6px; padding: 10px; text-align: left; }";
    s += "QPushButton#PresetCard:hover { background-color: " + k.hov + "; border-color: " + k.accent + "; }";
    s += "QPushButton#TemplateCard { background-color: " + k.base2 + "; border: 1px solid " + k.line + "; border-radius: 6px; padding: 8px; text-align: left; }";
    s += "QPushButton#TemplateCard:hover { border-color: " + k.accent + "; }";
    s += "QPushButton#PresetChip { background-color: " + k.base3 + "; border: 1px solid " + k.line + "; border-radius: 4px; padding: 5px 10px; font-size: 11px; color: " + k.textHi + "; }";
    s += "QPushButton#PresetChip:hover { background-color: " + k.hov + "; border-color: " + k.accent + "; }";
    s += "QPushButton#PresetChip[active=\"true\"] { background-color: " + k.accent + "; color: #ffffff; }";
    s += "QPushButton#ColorSwatch { border: 1px solid " + k.line + "; border-radius: 4px; }";

    // -- Labels de secao (des-inline-izacao da Fase C) --
    s += "QLabel#SectionLabel { font-size: 13px; font-weight: 600; color: " + k.textHi + "; }";
    s += "QLabel#HeroTitle { font-size: 24px; font-weight: 700; color: " + k.textHi + "; }";
    s += "QLabel#HeroTagline { font-size: 12px; color: " + k.textMd + "; }";
    s += "QWidget#EmptyState { background-color: " + k.base2 + "; border: 1px dashed " + k.line + "; border-radius: 6px; }";
    s += "QLabel#TemplatePreview { background-color: " + k.base0 + "; border: none; border-radius: 5px; }";
    s += "QLabel#TemplatePlatform { font-size: 10px; font-weight: 700; border: none; background: transparent; }";
    s += "QLabel#TemplateTitle { font-size: 11px; font-weight: 600; color: " + k.textHi + "; border: none; background: transparent; }";
    s += "QLabel#TemplateDims { font-size: 9px; color: " + k.textMd + "; border: none; background: transparent; }";
    s += "QLabel#HintLabel { color: " + k.textLo + "; font-size: 11px; border: none; }";
    s += "QScrollArea#StartScrollArea { background: transparent; border: none; }";

    // -- Checkbox / Slider / GroupBox / Tooltip / Progress --
    s += "QCheckBox { color: " + k.textHi + "; spacing: 7px; }";
    s += "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid " + k.line + "; border-radius: 4px; background-color: " + k.base1 + "; }";
    s += "QCheckBox::indicator:hover { border-color: " + k.accent + "; }";
    s += "QCheckBox::indicator:checked { background-color: " + k.accent + "; border-color: " + k.accent + "; }";
    s += "QSlider::groove:horizontal { height: 4px; background: " + k.base3 + "; border-radius: 2px; }";
    s += "QSlider::sub-page:horizontal { background: " + k.accent + "; border-radius: 2px; }";
    s += "QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; background: " + k.textHi + "; }";
    s += "QSlider::handle:horizontal:hover { background: " + k.accent + "; }";
    s += "QGroupBox { border: 1px solid " + k.line + "; border-radius: 6px; margin-top: 10px; padding-top: 6px; color: " + k.textMd + "; font-weight: 600; }";
    s += "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }";
    s += "QSplitter::handle { background-color: " + k.line + "; }";
    s += "QToolTip { background-color: " + k.base3 + "; color: " + k.textHi + "; border: 1px solid " + k.line + "; padding: 5px 8px; border-radius: 5px; }";
    s += "QProgressBar { background-color: " + k.base1 + "; border: 1px solid " + k.line + "; border-radius: 5px; text-align: center; color: " + k.textHi + "; min-height: 18px; }";
    s += "QProgressBar::chunk { background-color: " + k.accent + "; border-radius: 4px; }";

    return s;
}

} // namespace

void applyDarkTheme(QApplication& app)
{
    app.setStyle(QStringLiteral("Fusion"));
    QPalette pal;
    pal.setColor(QPalette::Window, Design::Base0());
    pal.setColor(QPalette::WindowText, Design::TextHi());
    pal.setColor(QPalette::Base, Design::Base2());
    pal.setColor(QPalette::AlternateBase, Design::Base1());
    pal.setColor(QPalette::ToolTipBase, Design::Base3());
    pal.setColor(QPalette::ToolTipText, Design::TextHi());
    pal.setColor(QPalette::PlaceholderText, Design::TextLo());
    pal.setColor(QPalette::Text, Design::TextHi());
    pal.setColor(QPalette::Button, Design::Base3());
    pal.setColor(QPalette::ButtonText, Design::TextHi());
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Link, Design::Accent());
    pal.setColor(QPalette::Highlight, Design::Accent());
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::Disabled, QPalette::Text, Design::TextLo());
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, Design::TextLo());
    pal.setColor(QPalette::Disabled, QPalette::WindowText, Design::TextLo());
    pal.setColor(QPalette::Disabled, QPalette::Base, Design::Base1());
    app.setPalette(pal);
    app.setStyleSheet(buildQss(darkSkin()));
}

void applyLightTheme(QApplication& app)
{
    app.setStyle(QStringLiteral("Fusion"));
    QPalette pal;
    pal.setColor(QPalette::Window, Design::LBase0());
    pal.setColor(QPalette::WindowText, Design::LTextHi());
    pal.setColor(QPalette::Base, Design::LBase2());
    pal.setColor(QPalette::AlternateBase, Design::LBase1());
    pal.setColor(QPalette::ToolTipBase, Design::LBase3());
    pal.setColor(QPalette::ToolTipText, Design::LTextHi());
    pal.setColor(QPalette::PlaceholderText, Design::LTextLo());
    pal.setColor(QPalette::Text, Design::LTextHi());
    pal.setColor(QPalette::Button, Design::LBase3());
    pal.setColor(QPalette::ButtonText, Design::LTextHi());
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Link, Design::Accent());
    pal.setColor(QPalette::Highlight, Design::Accent());
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::Disabled, QPalette::Text, Design::LTextLo());
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, Design::LTextLo());
    pal.setColor(QPalette::Disabled, QPalette::WindowText, Design::LTextLo());
    pal.setColor(QPalette::Disabled, QPalette::Base, Design::LBase1());
    app.setPalette(pal);
    app.setStyleSheet(buildQss(lightSkin()));
}

void applyTheme(QApplication& app, bool darkMode)
{
    if (darkMode) applyDarkTheme(app);
    else applyLightTheme(app);
}

} // namespace cc
