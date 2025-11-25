//====================================================================================
// GFX.cpp
//  
// Features:
//   - Provides a set of graphics drawing utilities for rendering shapes, text, 
//     and bitmaps on graphical displays.
//   - Abstract base class for graphics operations, to be implemented by derived classes.
//
// Inspired by:
//   - Adafruit-GFX-Library: https://github.com/adafruit/Adafruit-GFX-Library
//   - eSPI: https://github.com/Bodmer/TFT_eSPI
//
// Copyright (c) 2025 Dad Design. All rights reserved.
//
//====================================================================================

#include "GFX.h"
namespace DadGFX {

//***********************************************************************************
// CFont
// Character font management
//
// Fonts can be generated using the TTF2Bitmap tool. 
// See https://github.com/DADDesign-Projects/TrueType-to-Bitmap-Converter
//
//***********************************************************************************

// --------------------------------------------------------------------------
// Class Initalisation
void cFont::Init(const GFXCFont *pFont)
{
    
    m_pFont = pFont;         // Pointer to the font descriptor
    m_pTable = pFont->glyph; // Pointer to the glyph descriptor table

    // Iterate through all characters to determine the maximum character heights
    // Outputs:
    //    m_NegHeight = height below the cursor line;
    //    m_PosHeight = height above the cursor line;
    GFXglyph *pTable = m_pTable;
    uint16_t SizeTable = 1 + pFont->last - pFont->first;
    m_NegHeight = 0;
    m_PosHeight = 0;
    for (uint16_t index = 0; index < SizeTable; index++)
    {
        int8_t Offset = pTable->yOffset;
        int8_t NegHeight = pTable->height + Offset;
        if (NegHeight > m_NegHeight)
        {
            m_NegHeight = NegHeight;
        }
        if (Offset < m_PosHeight)
        {
            m_PosHeight = Offset;
        }
        pTable++;
    }
}

// --------------------------------------------------------------------------
// Constructor for Binary
cFont::cFont(GFXBinFont *pFont)
{
    uint8_t * pData;
    pData = (uint8_t *) pFont;
    
    m_Font.first = pFont->first;
    m_Font.last = pFont->last;
    m_Font.yAdvance = pFont->yAdvance;
    m_Font.bitmap = pData + pFont->bitmap;
    m_Font.glyph = (GFXglyph *) (pData + pFont->glyph);
    Init(&m_Font);
}


// --------------------------------------------------------------------------
// Reads the width of a string
uint16_t cFont::getTextWidth(const char *Text)
{
    const char *pText = Text;
    uint16_t result = 0;
    while (*pText != '\0')
    {
        result += getCharWidth(*pText++);
    }
    return result;
}

//***********************************************************************************
// cGFX
// Graphics rendering class
//***********************************************************************************

// ==========================================================================
// Shape Drawing Methods
// ==========================================================================

// --------------------------------------------------------------------------
// Draw an empty rectangle
void cGFX::drawRect(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint16_t strokeWidth, const sColor& Color) {
    setRectangle(x, y, Width, strokeWidth, Color);                                                          // Top edge
    setRectangle(x, y + Height - strokeWidth, Width, strokeWidth, Color);                                   // Bottom edge
    setRectangle(x, y + strokeWidth, strokeWidth, Height - (2 * strokeWidth), Color);                       // Left edge
    setRectangle(x + Width - strokeWidth, y + strokeWidth, strokeWidth, Height - (2 * strokeWidth), Color); // Right edge
}

// --------------------------------------------------------------------------
// Draw a line using Bresenham's line algorithm
void cGFX::drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const sColor& Color) {
    int16_t dx = x1 - x0; // Delta x
    int16_t dy = y1 - y0; // Delta y

    int16_t incX = dx < 0 ? -1 : (dx > 0 ? 1 : 0); // X increment direction
    int16_t incY = dy < 0 ? -1 : (dy > 0 ? 1 : 0); // Y increment direction
    if (dx < 0) dx = -dx; // Absolute value of dx
    if (dy < 0) dy = -dy; // Absolute value of dy

    // Horizontal line optimization
    if (dx == 0) {
        setRectangle(x0, y0, 1, dy, Color);
    }

    // Vertical line optimization
    else if (dy == 0) {
        setRectangle(x0, y0, dx, 1, Color);
    }

    // Line is more horizontal than vertical
    else if (dx >= dy) {
        int slope = 2 * dy;       // Slope adjustment
        int error = -dx;         // Error accumulator
        int errorInc = -2 * dx;  // Error increment
        int y = y0;

        for (int x = x0; x != x1 + incX; x += incX) {
            setPixel(x, y, Color);
            error += slope;

            if (error >= 0) {
                y += incY;
                error += errorInc;
            }
        }
    }

    // Line is more vertical than horizontal
    else {
        int slope = 2 * dx;       // Slope adjustment
        int error = -dy;          // Error accumulator
        int errorInc = -2 * dy;   // Error increment
        int x = x0;

        for (int y = y0; y != y1 + incY; y += incY) {
            setPixel(x, y, Color);
            error += slope;

            if (error >= 0) {
                x += incX;
                error += errorInc;
            }
        }
    }
}

// --------------------------------------------------------------------------   
// Draw an empty circle
// Uses Bresenham's circle algorithm
void cGFX::drawCircle(uint16_t centerX, uint16_t centerY, uint16_t radius, const sColor& Color) {
    int16_t x = 0;
    int16_t y = radius;
    int16_t m = 5 - 4 * radius;

    while (x <= y) {
        // Draw the 8 symmetrical points of the circle
        setPixel(centerX + x, centerY + y, Color);
        setPixel(centerX + x, centerY - y, Color);
        setPixel(centerX - x, centerY + y, Color);
        setPixel(centerX - x, centerY - y, Color);
        setPixel(centerX + y, centerY + x, Color);
        setPixel(centerX + y, centerY - x, Color);
        setPixel(centerX - y, centerY + x, Color);
        setPixel(centerX - y, centerY - x, Color);

        if (m > 0) {
            y--;
            m -= 8 * y;
        }
        x++;
        m += 8 * x + 4;
    }
}

// --------------------------------------------------------------------------   
// Draw a filled circle
// Uses Bresenham's circle algorithm
void cGFX::drawFillCircle(uint16_t centerX, uint16_t centerY, uint16_t radius, const sColor& Color) {
    int32_t x = 0;
    int32_t dx = 1;
    int32_t dy = radius + radius;
    int32_t p = -(radius >> 1);

    uint16_t x1 = centerX - radius;
    uint16_t x2 = x1 + dy + 1;
    uint16_t y1 = centerY;

    // Fill the initial horizontal line
    setRectangle(x1, y1, x2 - x1, 1, Color); 

    while (x < radius) {
        if (p >= 0) {
            x1 = centerX - x;
            x2 = x1 + dx;
            y1 = centerY + radius;
            setRectangle(x1, y1, x2 - x1, 1, Color); // Top line segment
            y1 = centerY - radius;
            setRectangle(x1, y1, x2 - x1, 1, Color); // Bottom line segment
            dy -= 2;
            p -= dy;
            radius--;
        }
        dx += 2;
        p += dx;
        x++;

        x1 = centerX - radius;
        x2 = x1 + dy + 1;
        y1 = centerY + x;
        setRectangle(x1, y1, x2 - x1, 1, Color); // Right segment
        y1 = centerY - x;
        setRectangle(x1, y1, x2 - x1, 1, Color); // Left segment
    }        
}

// --------------------------------------------------------------------------  
// Draw an arc
void cGFX::drawArc(uint16_t centerX, uint16_t centerY, uint16_t radius, uint16_t AlphaIn, uint16_t AlphaOut, const sColor& Color) {
    bool Inv = false;
    if (AlphaIn > AlphaOut) {
        // Swap angles if needed and invert logic
        uint16_t Temp = AlphaIn;
        AlphaIn = AlphaOut;
        AlphaOut = Temp;
        Inv = true;
    }

    int16_t x = 0;
    int16_t y = radius;
    int16_t m = 5 - 4 * radius;

    while (x <= y) {
        uint16_t angle = atan2((float)y, (float)x) * 180 / __PI; // Convert radians to degrees

        if (Inv) {
            // Handle inverted angle logic
            uint16_t angle1 = 90 - angle;
            if (!((angle1 >= AlphaIn) && (angle1 <= AlphaOut))) setPixel(centerX + x, centerY - y, Color);
            uint16_t angle2 = angle;
            if (!((angle2 >= AlphaIn) && (angle2 <= AlphaOut))) setPixel(centerX + y, centerY - x, Color);
            angle1 += 90;
            if (!((angle1 >= AlphaIn) && (angle1 <= AlphaOut))) setPixel(centerX + y, centerY + x, Color);
            angle2 += 90;
            if (!((angle2 >= AlphaIn) && (angle2 <= AlphaOut))) setPixel(centerX + x, centerY + y, Color);
            angle1 += 90;
            if (!((angle1 >= AlphaIn) && (angle1 <= AlphaOut))) setPixel(centerX - x, centerY + y, Color);
            angle2 += 90;
            if (!((angle2 >= AlphaIn) && (angle2 <= AlphaOut))) setPixel(centerX - y, centerY + x, Color);
            angle1 += 90;
            if (!((angle1 >= AlphaIn) && (angle1 <= AlphaOut))) setPixel(centerX - y, centerY - x, Color);
            angle2 += 90;
            if (!((angle2 >= AlphaIn) && (angle2 <= AlphaOut))) setPixel(centerX - x, centerY - y, Color);
        } else {
            // Handle normal angle logic
            uint16_t angle1 = 90 - angle;
            if (((angle1 >= AlphaIn) && (angle1 <= AlphaOut))) setPixel(centerX + x, centerY - y, Color);
            uint16_t angle2 = angle;
            if (((angle2 >= AlphaIn) && (angle2 <= AlphaOut))) setPixel(centerX + y, centerY - x, Color);
            angle1 += 90;
            if (((angle1 >= AlphaIn) && (angle1 <= AlphaOut))) setPixel(centerX + y, centerY + x, Color);
            angle2 += 90;
            if (((angle2 >= AlphaIn) && (angle2 <= AlphaOut))) setPixel(centerX + x, centerY + y, Color);
            angle1 += 90;
            if (((angle1 >= AlphaIn) && (angle1 <= AlphaOut))) setPixel(centerX - x, centerY + y, Color);
            angle2 += 90;
            if (((angle2 >= AlphaIn) && (angle2 <= AlphaOut))) setPixel(centerX - y, centerY + x, Color);
            angle1 += 90;
            if (((angle1 >= AlphaIn) && (angle1 <= AlphaOut))) setPixel(centerX - y, centerY - x, Color);
            angle2 += 90;
            if (((angle2 >= AlphaIn) && (angle2 <= AlphaOut))) setPixel(centerX - x, centerY - y, Color);
        }

        if (m > 0) {
            y--;
            m -= 8 * y;
        }
        x++;
        m += 8 * x + 4;
    }
}

// ==========================================================================
// Draw text
// ==========================================================================

//-----------------------------------------------------------------------------------
// Draw a single character
void cGFX::drawChar(const char c) {
    const GFXglyph *pTable = m_pFont->getGFXglyph(c);

    // Draw the character using a bitmap, applying the font glyph offsets
    fillRectWithBitmap(m_xCursor + pTable->xOffset, m_yCursor + pTable->yOffset,
                    m_pFont->getBitmap(c), 
                    pTable->width, pTable->height,
                    m_TextFrontColor, m_TextBackColor);

    // Advance the cursor based on the glyph's xAdvance value
    m_xCursor += pTable->xAdvance;
}

//-----------------------------------------------------------------------------------
// Draw a string of text
void cGFX::drawText(const char *Text) {
    const char *pText = Text;
    // Iterate through each character in the string and draw it
    while (*pText != '\0') {
        drawChar(*pText++);
    }
}

} // DadGFX