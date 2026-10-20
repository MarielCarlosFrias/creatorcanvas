#pragma once
#include <QColor>
#include <QFont>
#include <QString>
#include <QtGlobal>

namespace cc::Design {

// ===== Espacamento (grid 4/8pt) =====
constexpr int S2 = 2, S4 = 4, S6 = 6, S8 = 8, S12 = 12, S16 = 16;
constexpr int S24 = 24, S32 = 32, S48 = 48, S60 = 60;

// ===== Radius (maximo 6px — apps pro usam radius pequeno) =====
constexpr int R2 = 2, R4 = 4, R6 = 6;

// ===== Layout =====
constexpr int DockMin = 240, DockMax = 360, DockIdeal = 280;
constexpr int ToolbarH = 40, ToolOptionsH = 38, StatusH = 26;
constexpr int CanvasPadding = 32;
constexpr int BreakXS = 620, BreakSM = 980, BreakMD = 1440;
/// Largura mínima da janela do editor e limite para mostrar dicas longas.
constexpr int EditorMinWidth = 800;
constexpr int EditorMinHeight = 520;
constexpr int HintBreakWidth = 1380;
/// Abaixo disto os docks são auto-ocultados (layout ultra-compacto).
constexpr int UltraCompactWidth = 840;

inline int responsiveMargin(int width) { return qBound(16, width / 20, 60); }
inline int dockWidth(int width) { return qBound(DockMin, width / 5, DockMax); }
inline int iconSize(int width) { return width < BreakXS ? 20 : width < BreakMD ? 24 : 28; }
inline int fontSizeTitle(int width) { return width < BreakXS ? 18 : width < BreakSM ? 20 : 24; }
inline int fontSizeBody(int width) { return width < BreakXS ? 10 : 11; }

// ===== ELEVACAO DARK — 5 superficies + 1 hover + 1 UNICA borda =====
inline QColor Base0()  { return {0x0e,0x10,0x13}; }  // 0: fundo da janela
inline QColor Base1()  { return {0x14,0x16,0x1a}; }  // 1: chrome (toolbars)
inline QColor Base2()  { return {0x1a,0x1d,0x21}; }  // 2: paineis, listas
inline QColor Base3()  { return {0x22,0x25,0x2b}; }  // 3: cartoes, inputs
inline QColor Hover()  { return {0x2a,0x2e,0x36}; }  // UNICO hover do app
inline QColor Press()  { return {0x22,0x25,0x2b}; }
inline QColor Line()   { return {0x2e,0x32,0x3b}; }  // UNICA borda do app
inline QColor TextHi() { return {0xe8,0xea,0xee}; }
inline QColor TextMd() { return {0x9a,0xa2,0xae}; }
inline QColor TextLo() { return {0x64,0x6b,0x76}; }

// ===== ELEVACAO LIGHT =====
inline QColor LBase0() { return {0xe9,0xec,0xf1}; }  // 0: fundo da janela
inline QColor LBase1() { return {0xf3,0xf5,0xf8}; }  // 1: chrome
inline QColor LBase2() { return {0xff,0xff,0xff}; }  // 2: paineis, listas
inline QColor LBase3() { return {0xff,0xff,0xff}; }  // 3: cartoes, inputs
inline QColor LHover() { return {0xe9,0xee,0xf6}; }
inline QColor LPress() { return {0xdf,0xe4,0xec}; }
inline QColor LLine()  { return {0xd4,0xd9,0xe0}; }
inline QColor LTextHi(){ return {0x1a,0x1e,0x24}; }
inline QColor LTextMd(){ return {0x4f,0x57,0x63}; }
inline QColor LTextLo(){ return {0x98,0xa0,0xab}; }

// ===== ESTADOS (ações destrutivas/confirmadas) =====
inline QColor Success()      { return {0x2e,0x7d,0x32}; }
inline QColor SuccessHover() { return {0x38,0x8e,0x3c}; }
inline QColor Danger()       { return {0xc6,0x28,0x28}; }

// ===== ACCENT (compartilhado dark/light) =====
inline QColor Accent()      { return {0x3d,0x7e,0xff}; }
inline QColor AccentHover() { return {0x55,0x8e,0xff}; }
inline QColor AccentPress() { return {0x2f,0x66,0xd4}; }

// ===== ALIASES de compatibilidade (codigo existente continua compilando) =====
inline QColor DWindow()      { return Base0(); }
inline QColor DSurface()     { return Base1(); }
inline QColor DElevated()    { return Base2(); }
inline QColor DCard()        { return Base3(); }
inline QColor DBorder()      { return Line(); }
inline QColor DBorderLight() { return Line(); }
inline QColor DText()        { return TextHi(); }
inline QColor DTextMuted()   { return TextMd(); }
inline QColor DTextDim()     { return TextLo(); }
inline QColor LWindow()      { return LBase0(); }
inline QColor LSurface()     { return LBase2(); }
inline QColor LBorder()      { return LLine(); }
inline QColor LText()        { return LTextHi(); }
inline QColor LTextMuted()   { return LTextMd(); }

// ===== Tipografia =====
inline QFont fontCaption() { QFont f; f.setPointSize(10); return f; }
inline QFont fontBody(int pt = 11) { QFont f; f.setPointSize(pt); return f; }
inline QFont fontTitle(int pt = 14, bool bold = true)
{ QFont f; f.setPointSize(pt); f.setBold(bold); return f; }

} // namespace cc::Design
