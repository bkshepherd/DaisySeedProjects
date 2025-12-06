#include "GFX2Adapter.h"
#include <cstring>
#include <cmath>

namespace bkshepherd {

// Use GFX2's macros for proper memory management
DECLARE_DISPLAY(__Display);
DECLARE_LAYER(MainLayer, TFT_WIDTH, TFT_HEIGHT);

GFX2Adapter::GFX2Adapter()
    : display_(nullptr),
      layer_(nullptr),
      gfx_font_(nullptr),
      foreground_color_(255, 255, 255, 255),  // White
      background_color_(0, 0, 0, 255),        // Black
      cursor_x_(0),
      cursor_y_(0) {
}

GFX2Adapter::~GFX2Adapter() {
    // Clean up font
    if (gfx_font_) {
        delete gfx_font_;
        gfx_font_ = nullptr;
    }
}

void GFX2Adapter::Init() {
    // Initialize using GFX2's macro system
    INIT_DISPLAY(__Display);
    display_ = &__Display;
    
    // Add the main layer at position (0,0) with Z-order 1
    layer_ = ADD_LAYER(MainLayer, 0, 0, 1);
    
    if (!layer_) {
        // Failed to create layer - hang with blink pattern
        while(1) {
            daisy::System::Delay(200);
        }
    }
    
    // Initialize single GFX2 font
    gfx_font_ = new DadGFX::cFont(&FreeSans12pt7b);
    
    if (gfx_font_ && layer_) {
        layer_->setFont(gfx_font_);
    }
    
    // Set initial drawing colors
    layer_->setTextFrontColor(foreground_color_);
    layer_->setTextBackColor(background_color_);
    
    // Clear to black
    layer_->eraseLayer(background_color_);
    display_->flush();
}

void GFX2Adapter::Fill(bool on) {
    if (!layer_) return;
    
    DadGFX::sColor fill_color = on ? foreground_color_ : background_color_;
    layer_->eraseLayer(fill_color);
}

void GFX2Adapter::DrawPixel(uint_fast8_t x, uint_fast8_t y, bool on) {
    if (!layer_) return;
    
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
    
    DadGFX::sColor color = on ? foreground_color_ : background_color_;
    layer_->drawFillRect(x, y, 1, 1, color);
}

void GFX2Adapter::DrawLine(uint_fast8_t x1, uint_fast8_t y1,
                           uint_fast8_t x2, uint_fast8_t y2, bool on) {
    if (!layer_) return;
    
    DadGFX::sColor color = on ? foreground_color_ : background_color_;
    layer_->drawLine(x1, y1, x2, y2, color);
}

void GFX2Adapter::DrawRect(uint_fast8_t x1, uint_fast8_t y1,
                           uint_fast8_t x2, uint_fast8_t y2,
                           bool on, bool fill) {
    if (!layer_) return;
    
    DadGFX::sColor color = on ? foreground_color_ : background_color_;
    
    uint16_t width = (x2 >= x1) ? (x2 - x1 + 1) : 0;
    uint16_t height = (y2 >= y1) ? (y2 - y1 + 1) : 0;
    
    if (width == 0 || height == 0) return;
    
    if (fill) {
        layer_->drawFillRect(x1, y1, width, height, color);
    } else {
        layer_->drawRect(x1, y1, width, height, 1, color);
    }
}

void GFX2Adapter::DrawArc(uint_fast8_t x, uint_fast8_t y, uint_fast8_t radius,
                          int_fast16_t start_angle, int_fast16_t sweep, bool on) {
    if (!layer_) return;
    
    DadGFX::sColor color = on ? foreground_color_ : background_color_;
    
    if (abs(sweep) >= 360) {
        layer_->drawCircle(x, y, radius, color);
    } else {
        int_fast16_t end_angle = start_angle + sweep;
        
        while (start_angle < 0) start_angle += 360;
        while (end_angle < 0) end_angle += 360;
        while (start_angle >= 360) start_angle -= 360;
        while (end_angle >= 360) end_angle -= 360;
        
        layer_->drawArc(x, y, radius, start_angle, end_angle, color);
    }
}

char GFX2Adapter::WriteChar(char c, FontDef font, bool on) {
    if (!layer_ || !gfx_font_) return 0;
    
    // Ignore font parameter - use our single GFX2 font for all
    
    // Set colors
    DadGFX::sColor text_color = on ? foreground_color_ : background_color_;
    DadGFX::sColor bg_color = on ? background_color_ : foreground_color_;
    
    layer_->setTextFrontColor(text_color);
    layer_->setTextBackColor(bg_color);
    
    // Set cursor and draw
    layer_->setCursor(cursor_x_, cursor_y_);
    layer_->drawChar(c);
    
    // Get actual character width and advance cursor
    uint8_t char_width = gfx_font_->getCharWidth(c);
    cursor_x_ = layer_->getXCursor();
    
    return char_width;
}

char GFX2Adapter::WriteString(const char* str, FontDef font, bool on) {
    if (!str || !layer_ || !gfx_font_) return 0;
    
    // Ignore font parameter - use our single GFX2 font for all
    
    // Set colors
    DadGFX::sColor text_color = on ? foreground_color_ : background_color_;
    DadGFX::sColor bg_color = on ? background_color_ : foreground_color_;
    
    layer_->setTextFrontColor(text_color);
    layer_->setTextBackColor(bg_color);
    
    // Set cursor and draw
    layer_->setCursor(cursor_x_, cursor_y_);
    layer_->drawText(str);
    
    // Update cursor position
    cursor_x_ = layer_->getXCursor();
    
    // Calculate total width
    uint16_t total_width = gfx_font_->getTextWidth(str);
    
    return total_width;
}

Rectangle GFX2Adapter::WriteStringAligned(const char* str,
                                           const FontDef& font,
                                           Rectangle boundingBox,
                                           Alignment alignment,
                                           bool on) {
    if (!str || !layer_ || !gfx_font_) {
        return Rectangle(0, 0, 0, 0);
    }
    
    // Ignore font parameter - use our single GFX2 font for all
    
    // Get text dimensions
    uint16_t text_width = gfx_font_->getTextWidth(str);
    uint8_t text_height = gfx_font_->getHeight();
    
    // Calculate position based on alignment
    uint16_t x = boundingBox.GetX();
    uint16_t y = boundingBox.GetY();
    
    switch(alignment) {
        case Alignment::topLeft:
            break;
        case Alignment::topCentered:
            if (boundingBox.GetWidth() > text_width) {
                x += (boundingBox.GetWidth() - text_width) / 2;
            }
            break;
        case Alignment::topRight:
            if (boundingBox.GetWidth() > text_width) {
                x += boundingBox.GetWidth() - text_width;
            }
            break;
        case Alignment::centeredLeft:
            if (boundingBox.GetHeight() > text_height) {
                y += (boundingBox.GetHeight() - text_height) / 2;
            }
            break;
        case Alignment::centered:
            if (boundingBox.GetWidth() > text_width) {
                x += (boundingBox.GetWidth() - text_width) / 2;
            }
            if (boundingBox.GetHeight() > text_height) {
                y += (boundingBox.GetHeight() - text_height) / 2;
            }
            break;
        case Alignment::centeredRight:
            if (boundingBox.GetWidth() > text_width) {
                x += boundingBox.GetWidth() - text_width;
            }
            if (boundingBox.GetHeight() > text_height) {
                y += (boundingBox.GetHeight() - text_height) / 2;
            }
            break;
        case Alignment::bottomLeft:
            if (boundingBox.GetHeight() > text_height) {
                y += boundingBox.GetHeight() - text_height;
            }
            break;
        case Alignment::bottomCentered:
            if (boundingBox.GetWidth() > text_width) {
                x += (boundingBox.GetWidth() - text_width) / 2;
            }
            if (boundingBox.GetHeight() > text_height) {
                y += boundingBox.GetHeight() - text_height;
            }
            break;
        case Alignment::bottomRight:
            if (boundingBox.GetWidth() > text_width) {
                x += boundingBox.GetWidth() - text_width;
            }
            if (boundingBox.GetHeight() > text_height) {
                y += boundingBox.GetHeight() - text_height;
            }
            break;
    }
    
    // Set colors and draw
    DadGFX::sColor text_color = on ? foreground_color_ : background_color_;
    DadGFX::sColor bg_color = on ? background_color_ : foreground_color_;
    
    layer_->setTextFrontColor(text_color);
    layer_->setTextBackColor(bg_color);
    layer_->setCursor(x, y);
    layer_->drawText(str);
    
    return Rectangle(x, y, text_width, text_height);
}

void GFX2Adapter::Update() {
    if (!display_) return;
    display_->flush();
}

void GFX2Adapter::SetForegroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    foreground_color_.set(r, g, b, a);
    if (layer_) {
        layer_->setTextFrontColor(foreground_color_);
    }
}

void GFX2Adapter::SetBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    background_color_.set(r, g, b, a);
    if (layer_) {
        layer_->setTextBackColor(background_color_);
    }
}

void GFX2Adapter::TestFill(uint8_t r, uint8_t g, uint8_t b) {
    if (!layer_) return;
    
    DadGFX::sColor test_color(r, g, b, 255);
    layer_->eraseLayer(test_color);
    display_->flush();
}

void GFX2Adapter::TestDrawingPrimitives() {
    if (!layer_ || !gfx_font_) return;
    
    // Test text rendering
    layer_->eraseLayer(background_color_);
    display_->flush();
    
    SetCursor(10, 30);
    WriteString("Emerald", Font_7x10, true);
    
    SetCursor(10, 60);
    WriteString("By Spencer Wilde", Font_11x18, true);
    
    Update();
    daisy::System::Delay(3000);
    
    // Clear
    Fill(false);
    Update();
}

} // namespace bkshepherd