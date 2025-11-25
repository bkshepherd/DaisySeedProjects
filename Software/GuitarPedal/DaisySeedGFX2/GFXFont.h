//***********************************************************************************
// GFXFont.h
//***********************************************************************************
#pragma once
#include <stdint.h>

namespace DadGFX {

// Character descriptor table
typedef struct
{
    uint16_t bitmapOffset; // Pointer into GFXfont->bitmap
    uint8_t width;         // Bitmap width in pixels
    uint8_t height;        // Bitmap height in pixels
    uint8_t xAdvance;      // Distance to advance cursor (x-axis)
    int8_t xOffset;        // X offset from cursor position to upper-left corner
    int8_t yOffset;        // Y offset from cursor position to upper-left corner
} GFXglyph;

// Font C descriptor
typedef struct
{
    uint8_t *   bitmap;     // Glyph bitmaps, concatenated
    GFXglyph *  glyph;      // Array of glyph descriptors
    uint16_t    first;      // ASCII range: first character
    uint16_t    last;       // ASCII range: last character
    uint8_t     yAdvance;   // Line height (y-axis)
} GFXCFont;

// Font Binary descriptor
typedef struct
{
    uint32_t    bitmap;     // Offset of Glyph bitmaps, concatenated
    uint32_t    glyph;      // Array of glyph descriptors
    uint16_t    first;      // ASCII range: first character
    uint16_t    last;       // ASCII range: last character
    uint8_t     yAdvance;   // Line height (y-axis)
} GFXBinFont;

} // DadGFX;
