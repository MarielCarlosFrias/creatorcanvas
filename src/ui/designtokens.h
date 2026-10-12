#pragma once
#include <QColor>
#include <QFont>

namespace cc::Design {

// 8pt grid
constexpr int S4 = 4, S8 = 8, S12 = 12, S16 = 16, S24 = 24, S32 = 32, S60 = 60;
// Radius
constexpr int R4 = 4, R6 = 6, R8 = 8, R12 = 12;

// ── Dark palette (single source) ──
inline QColor DWindow()      { return {0x10,0x12,0x16}; } // #101216
inline QColor DSurface()     { return {0x17,0x1A,0x20}; } // #171a20
inline QColor DElevated()    { return {0x21,0x22,0x26}; } // #212226
inline QColor DCard()        { return {0x23,0x27,0x30}; } // #232730 unified
inline QColor DBorder()      { return {0x2B,0x30,0x3C}; } // #2b303c
inline QColor DBorderLight() { return {0x30,0x37,0x44}; } // #303744
inline QColor DText()        { return {0xE7,0xE8,0xEA}; }
inline QColor DTextMuted()   { return {0x8C,0x97,0xA8}; }
inline QColor DTextDim()     { return {0x70,0x73,0x79}; }
inline QColor Accent()       { return {0x2B,0x78,0xE4}; } // #2b78e4
inline QColor AccentHover()  { return {0x3B,0x88,0xF4}; }
inline QColor AccentPress()  { return {0x1A,0x64,0xCA}; }

// ── Light palette ──
inline QColor LWindow()   { return {0xF1,0xF3,0xF6}; }
inline QColor LSurface()  { return {0xFF,0xFF,0xFF}; }
inline QColor LBorder()   { return {0xD1,0xD5,0xDB}; }
inline QColor LText()     { return {0x1E,0x22,0x2B}; }
inline QColor LTextMuted(){ return {0x6B,0x72,0x80}; }

inline QFont fontTitle(int pt=14,bool bold=true){ QFont f; f.setPointSize(pt); f.setBold(bold); return f; }
inline QFont fontBody(int pt=11){ QFont f; f.setPointSize(pt); return f; }

} // namespace cc::Design
