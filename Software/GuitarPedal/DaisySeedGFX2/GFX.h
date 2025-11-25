//====================================================================================
// GFX.h
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

#pragma once // Ensure this file is only included once during compilation
#include "stdint.h"
#include "GFXFont.h"


namespace DadGFX {

//***********************************************************************************
// Constants
//***********************************************************************************
constexpr float __PI = 3.14159265358979;   // Value of π
constexpr float __PI_2 = 1.57079632679489; // Value of π/2

//***********************************************************************************
// Enumerations
//***********************************************************************************
// --------------------------------------------------------------------------
// Error codes for graphics operations
enum class DAD_GFX_ERROR {
    OK,            // Operation successful
    DMA2D_Error,   // Error in DMA2D hardware operation
    Size_Error,    // Size exceeds bounds or invalid dimensions
    Memory_Error   // Memory allocation or access error
};

//***********************************************************************************
// sColor 
//   Stores a color in the A8R8G8B8 format
//***********************************************************************************
struct sColor {
    union {
        struct {
            uint8_t m_B;  // BLUE channel (8-bit)
            uint8_t m_G;  // GREEN channel (8-bit)
            uint8_t m_R;  // RED channel (8-bit)
            uint8_t m_A;  // ALPHA channel (8-bit, transparency)
        };
        uint32_t m_ARGB;  // 32-bit representation of the color
    };

    // --------------------------------------------------------------------------
    // Constructor
    // Initializes the default color
    sColor(){}

    // --------------------------------------------------------------------------
    // Constructor
    // Initializes the color using red, green, blue, and optional alpha values
    sColor(uint8_t R, uint8_t G, uint8_t B, uint8_t Alpha = 255)
        : m_B(B), m_G(G), m_R(R), m_A(Alpha) {}

    // --------------------------------------------------------------------------
    // Set a new color value
    // Updates the red, green, blue, and alpha channels
    inline void set(uint8_t R, uint8_t G, uint8_t B, uint8_t Alpha = 255) {
        m_R = R;
        m_G = G;
        m_B = B;
        m_A = Alpha;
    }

    // --------------------------------------------------------------------------
    // Conversion operator to uint32_t
    // Allows the structure to be used directly as a 32-bit ARGB value
    inline operator uint32_t() const {
        return m_ARGB;
    }
};

//***********************************************************************************
// CFont
// Character font management
//
// Fonts can be generated using the TTF2Bitmap tool. 
// See https://github.com/DADDesign-Projects/TrueType-to-Bitmap-Converter
//
//***********************************************************************************

//***********************************************************************************
// CFont
class cFont {
public:
    // --------------------------------------------------------------------------
    // Constructor use for C font
    cFont(const GFXCFont *pFont){
        Init(pFont);
    }

    // --------------------------------------------------------------------------
    // Constructor use for Binary font
    cFont(GFXBinFont *pFont);

    // --------------------------------------------------------------------------
    // Reads the width of character c
    inline uint8_t getCharWidth(char c){
        return ((m_pFont->glyph) + (c - m_pFont->first))->xAdvance;
    }

    // --------------------------------------------------------------------------
    // Reads the width of a string
    uint16_t getTextWidth(const char *Text);

    // --------------------------------------------------------------------------
    // Reads the maximum font height
    inline uint8_t getHeight(){
        return m_NegHeight - m_PosHeight;
    }

    // --------------------------------------------------------------------------
    // Reads the maximum font height above the cursor line
    inline uint8_t getPosHeight(){
        return -m_PosHeight;
    }

    // --------------------------------------------------------------------------
    // Reads the maximum font height below the cursor line
    inline uint8_t getNegHeight(){
        return m_NegHeight;
    }

    // --------------------------------------------------------------------------
    // Returns the address of the font descriptor
    inline const GFXCFont *getGFXfont() { return m_pFont; }

    // --------------------------------------------------------------------------
    // Returns the address of the glyph descriptor table
    inline const GFXglyph *getGFXglyph() { return m_pTable; }

    // --------------------------------------------------------------------------
    // Returns the address of the glyph descriptor for character c
    inline const GFXglyph *getGFXglyph(char c){
        return m_pTable + (c - m_pFont->first);
    }

    // --------------------------------------------------------------------------
    // Returns the address of the bitmap representing character c
    inline const uint8_t *getBitmap(char c){
        return &m_pFont->bitmap[m_pTable[c - m_pFont->first].bitmapOffset];
    }
protected :
    void Init(const GFXCFont *pFont);

    // --------------------------------------------------------------------------
    // Class data
protected:
    const GFXCFont  *m_pFont;    // Font descriptor
    GFXglyph        *m_pTable;   // Glyph descriptor table

    int8_t          m_PosHeight; // Height above the cursor line
    int8_t          m_NegHeight; // Height below the cursor line
    GFXCFont        m_Font;
};

//***********************************************************************************
// cGFX
// Graphics rendering class
//***********************************************************************************
class cGFX {
public:
    // --------------------------------------------------------------------------
    // Constructor
    cGFX() {}

    // ==========================================================================
    // Shape Drawing Methods
    // ==========================================================================

    // --------------------------------------------------------------------------
    // Draw an empty rectangle
    void drawRect(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint16_t strokeWidth, const sColor& Color);

    // --------------------------------------------------------------------------
    // Draw a filled rectangle
    inline void drawFillRect(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const sColor& Color) {
        setRectangle(x, y, Width, Height, Color);
    }

    // --------------------------------------------------------------------------
    // Draw a line using Bresenham's line algorithm
    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const sColor& Color);

    // --------------------------------------------------------------------------   
    // Draw an empty circle
    // Uses Bresenham's circle algorithm
    void drawCircle(uint16_t centerX, uint16_t centerY, uint16_t radius, const sColor& Color);

    // --------------------------------------------------------------------------   
    // Draw a filled circle
    // Uses Bresenham's circle algorithm
    void drawFillCircle(uint16_t centerX, uint16_t centerY, uint16_t radius, const sColor& Color);

    // --------------------------------------------------------------------------  
    // Draw an arc
    void drawArc(uint16_t centerX, uint16_t centerY, uint16_t radius, uint16_t AlphaIn, uint16_t AlphaOut, const sColor& Color);

    // ==========================================================================
    // Draw text
    // ==========================================================================

    //-----------------------------------------------------------------------------------
    // Set the cursor position
    inline void setCursor(uint16_t x, uint16_t y) {
        m_xCursor = x;
        m_yCursor = y;
    }

    //-----------------------------------------------------------------------------------
    // Set the font
    inline void setFont(cFont *pFont) { 
        m_pFont = pFont; 
    }

    //-----------------------------------------------------------------------------------
    // Set the text foreground color
    inline void setTextFrontColor(const sColor& Color) { 
        m_TextFrontColor = Color; 
    }

    //-----------------------------------------------------------------------------------
    // Set the text background color
    inline void setTextBackColor(const sColor& Color) { 
        m_TextBackColor = Color; 
    }

    //-----------------------------------------------------------------------------------
    // Draw a single character
    void drawChar(const char c);

    //-----------------------------------------------------------------------------------
    // Draw a string of text
    void drawText(const char *Text);

    //-----------------------------------------------------------------------------------
    // Get the current cursor X position
    inline uint8_t getXCursor() { 
        return m_xCursor; 
    }

    //-----------------------------------------------------------------------------------
    // Get the current cursor Y position
    inline uint8_t getYCursor() { 
        return m_yCursor; 
    }

    //-----------------------------------------------------------------------------------
    // Get the currently set font
    inline cFont *getFont() { 
        return m_pFont; 
    }

    //-----------------------------------------------------------------------------------
    // Get the width of a string in pixels
    inline uint16_t getTextWidth(const char *Text) {
        return m_pFont->getTextWidth(Text);
    }

    //-----------------------------------------------------------------------------------
    // Get the height of the current font in pixels
    inline uint8_t getTextHeight() {
        return m_pFont->getHeight();
    }

    protected:
    // ==========================================================================
    // Virtual methods implemented in cLayer
    // ==========================================================================

    // Set a pixel at a specific position with a given color
    virtual DAD_GFX_ERROR setPixel(uint16_t x, uint16_t y, const sColor& Color) = 0;

    // Draw a rectangle at a specific position, size, and color
    virtual DAD_GFX_ERROR setRectangle(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const sColor& Color) = 0;

    // Fill a rectangle using a bitmap, with foreground and background colors
    virtual DAD_GFX_ERROR fillRectWithBitmap(uint16_t x0, uint16_t y0,
                                            const uint8_t* pBitmap, uint16_t BitmapWidth, uint16_t BitmapBmpHeight,
                                            const sColor& ForegroundColor, const sColor& BackgroundColor) = 0;

    // ==========================================================================
    // Member variables for text drawing
    // ==========================================================================

    // Current cursor position
    uint16_t m_xCursor = 0;
    uint16_t m_yCursor = 0;

    // Current font being used
    cFont *m_pFont = nullptr;

    // Current text foreground and background colors
    sColor m_TextFrontColor = sColor(255, 255, 255, 255); // Default: White
    sColor m_TextBackColor = sColor(0, 0, 0, 0);          // Default: Transparent black
};
} // DadGFX