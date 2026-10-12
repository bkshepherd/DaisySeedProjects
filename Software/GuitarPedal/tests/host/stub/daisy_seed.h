// Host-side stub of libDaisy, just enough for the effect modules to compile on a PC. UI types are inert.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
namespace daisy {
struct Rectangle {
    int x = 0, y = 0, w = 0, h = 0;
    int GetHeight() const { return h; } int GetWidth() const { return w; }
    int GetX() const { return x; } int GetY() const { return y; }
    int GetBottom() const { return y + h; } int GetRight() const { return x + w; }
    bool IsEmpty() const { return w <= 0 || h <= 0; }
    Rectangle Reduced(int, int) const { return *this; }
    Rectangle RemoveFromTop(int) { return *this; } Rectangle RemoveFromLeft(int) { return *this; } Rectangle RemoveFromRight(int) { return *this; }
    Rectangle WithSizeKeepingCenter(int, int) const { return *this; } Rectangle Translated(int, int) const { return *this; }
};
struct FontDef {};
static FontDef Font_11x18, Font_7x10, Font_6x8, Font_16x26;
enum class Alignment { centered, bottomCentered, topCentered, centeredLeft, centeredRight, topLeft, topRight, bottomLeft, bottomRight };
struct OneBitGraphicsDisplay {
    template <class... A> void DrawLine(A...) {}
    template <class... A> void WriteStringAligned(A...) {}
    template <class... A> void Fill(A...) {}
    template <class... A> void SetCursor(A...) {}
    template <class... A> void WriteString(A...) {}
};
} // namespace daisy
using namespace daisy;
#define FLT_FMT(n) "%d.%03d"
#define FLT_VAR3(x) (int)(x), (int)(fabsf((x)-(int)(x))*1000)
#define DSY_SDRAM_BSS
#define DSY_QSPI_BSS
#define DSY_QSPI_DATA
#define DSY_SDRAM_DATA
