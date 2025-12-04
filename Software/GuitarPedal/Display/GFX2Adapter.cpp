#include "GFX2Adapter.h"
#include <cstring>
#include <cmath>

namespace bkshepherd {

// Use GFX2's macros for proper memory management
// The display MUST be named __Display for the ADD_LAYER macro to work
DECLARE_DISPLAY(__Display);
DECLARE_LAYER(MainLayer, TFT_WIDTH, TFT_HEIGHT);

GFX2Adapter::GFX2Adapter()
    : display_(nullptr),
      layer_(nullptr),
      foreground_color_(255, 255, 255, 255),  // White
      background_color_(0, 0, 0, 255),        // Black
      cursor_x_(0),
      cursor_y_(0) {
}

GFX2Adapter::~GFX2Adapter() {
    // Memory is managed by GFX2 macros, no manual cleanup needed
}

void GFX2Adapter::Init() {
    // Initialize using GFX2's macro system
    INIT_DISPLAY(__Display);
    display_ = &__Display;
    
    // Add the main layer at position (0,0) with Z-order 1
    layer_ = ADD_LAYER(MainLayer, 0, 0, 1);
    
    if (!layer_) {
        // Failed to create layer
        while(1) {
            daisy::System::Delay(200);
        }
    }
    
    // Set initial drawing colors
    layer_->setTextFrontColor(foreground_color_);
    layer_->setTextBackColor(background_color_);
    
    // Test sequence - same as Phase 1 that worked
    DadGFX::sColor red(255, 0, 0, 255);
    DadGFX::sColor green(0, 255, 0, 255);
    DadGFX::sColor blue(0, 0, 255, 255);
    
    layer_->eraseLayer(red);
    display_->flush();
    daisy::System::Delay(1000);
    
    layer_->eraseLayer(green);
    display_->flush();
    daisy::System::Delay(1000);
    
    layer_->eraseLayer(blue);
    display_->flush();
    daisy::System::Delay(1000);
    
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
    
    // Bounds check
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
    
    DadGFX::sColor color = on ? foreground_color_ : background_color_;
    
    // Use drawFillRect with 1x1 size instead of drawLine
    // This is more reliable than line(x,y,x,y)
    layer_->drawFillRect(x, y, 1, 1, color);
}

void GFX2Adapter::DrawLine(uint_fast8_t x1, uint_fast8_t y1,
                           uint_fast8_t x2, uint_fast8_t y2, bool on) {
    if (!layer_) return;
    
    DadGFX::sColor color = on ? foreground_color_ : background_color_;
    layer_->drawLine(x1, y1, x2, y2, color);
    
    // For diagonal lines, force an update
    // This is inefficient but works around GFX2's tile tracking issue
    if (x1 != x2 && y1 != y2 && display_) {
        display_->flush();
    }
}

void GFX2Adapter::DrawRect(uint_fast8_t x1, uint_fast8_t y1,
                           uint_fast8_t x2, uint_fast8_t y2,
                           bool on, bool fill) {
    if (!layer_) return;
    
    DadGFX::sColor color = on ? foreground_color_ : background_color_;
    
    // Convert coordinates to width/height
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
    
    // If drawing a full circle (sweep >= 360 or <= -360)
    if (abs(sweep) >= 360) {
        layer_->drawCircle(x, y, radius, color);
    } else {
        // For partial arcs
        int_fast16_t end_angle = start_angle + sweep;
        
        // Normalize angles to 0-360 range
        while (start_angle < 0) start_angle += 360;
        while (end_angle < 0) end_angle += 360;
        while (start_angle >= 360) start_angle -= 360;
        while (end_angle >= 360) end_angle -= 360;
        
        layer_->drawArc(x, y, radius, start_angle, end_angle, color);
    }
    
    // Force update for circles/arcs
    // This is inefficient but works around GFX2's tile tracking issue
    if (display_) {
        display_->flush();
    }
}

char GFX2Adapter::WriteChar(char c, FontDef font, bool on) {
    if (!layer_) return 0;
    
    // Bounds check
    if (cursor_x_ >= TFT_WIDTH) return 0;
    
    DadGFX::sColor fg_color = on ? foreground_color_ : background_color_;
    DadGFX::sColor bg_color = on ? background_color_ : foreground_color_;
    
    // Get character data from font
    // Font data is stored starting at ASCII 32 (space)
    if (c < 32 || c > 126) c = 32; // Default to space for invalid chars
    
    const uint8_t* char_data = &font.data[(c - 32) * font.FontHeight];
    
    // Draw character using GFX2's fillRectWithBitmap would be ideal, but it's not accessible
    // So we'll draw pixel by pixel using drawFillRect for efficiency
    for (uint8_t row = 0; row < font.FontHeight; row++) {
        uint8_t byte = char_data[row];
        
        for (uint8_t col = 0; col < font.FontWidth; col++) {
            if (cursor_x_ + col >= TFT_WIDTH) break;
            if (cursor_y_ + row >= TFT_HEIGHT) break;
            
            // Check if bit is set (column 0 is LSB)
            bool pixel_on = (byte & (1 << col)) != 0;
            DadGFX::sColor color = pixel_on ? fg_color : bg_color;
            
            // Draw single pixel using 1x1 filled rect
            layer_->drawFillRect(cursor_x_ + col, cursor_y_ + row, 1, 1, color);
        }
    }
    
    // Move cursor forward
    cursor_x_ += font.FontWidth;
    
    return font.FontWidth;
}

char GFX2Adapter::WriteString(const char* str, FontDef font, bool on) {
    if (!str || !layer_) return 0;
    
    uint16_t start_x = cursor_x_;
    
    while (*str) {
        if (*str == '\n') {
            // Handle newline
            cursor_x_ = start_x;
            cursor_y_ += font.FontHeight;
        } else {
            WriteChar(*str, font, on);
        }
        str++;
    }
    
    return cursor_x_ - start_x;
}

Rectangle GFX2Adapter::WriteStringAligned(const char* str,
                                          const FontDef& font,
                                          Rectangle boundingBox,
                                          Alignment alignment,
                                          bool on) {
    if (!str || !layer_) return Rectangle(0, 0, 0, 0);
    
    // Calculate string dimensions
    size_t str_len = strlen(str);
    uint16_t str_width = str_len * font.FontWidth;
    uint16_t str_height = font.FontHeight;
    
    // Calculate starting position based on alignment
    uint16_t x = boundingBox.GetX();
    uint16_t y = boundingBox.GetY();
    
    switch (alignment) {
        case Alignment::topLeft:
            // Already at top-left
            break;
        case Alignment::topCentered:
            if (boundingBox.GetWidth() > str_width) {
                x += (boundingBox.GetWidth() - str_width) / 2;
            }
            break;
        case Alignment::topRight:
            if (boundingBox.GetWidth() > str_width) {
                x += boundingBox.GetWidth() - str_width;
            }
            break;
        case Alignment::centerLeft:
            if (boundingBox.GetHeight() > str_height) {
                y += (boundingBox.GetHeight() - str_height) / 2;
            }
            break;
        case Alignment::centered:
            if (boundingBox.GetWidth() > str_width) {
                x += (boundingBox.GetWidth() - str_width) / 2;
            }
            if (boundingBox.GetHeight() > str_height) {
                y += (boundingBox.GetHeight() - str_height) / 2;
            }
            break;
        case Alignment::centerRight:
            if (boundingBox.GetWidth() > str_width) {
                x += boundingBox.GetWidth() - str_width;
            }
            if (boundingBox.GetHeight() > str_height) {
                y += (boundingBox.GetHeight() - str_height) / 2;
            }
            break;
        case Alignment::bottomLeft:
            if (boundingBox.GetHeight() > str_height) {
                y += boundingBox.GetHeight() - str_height;
            }
            break;
        case Alignment::bottomCentered:
            if (boundingBox.GetWidth() > str_width) {
                x += (boundingBox.GetWidth() - str_width) / 2;
            }
            if (boundingBox.GetHeight() > str_height) {
                y += boundingBox.GetHeight() - str_height;
            }
            break;
        case Alignment::bottomRight:
            if (boundingBox.GetWidth() > str_width) {
                x += boundingBox.GetWidth() - str_width;
            }
            if (boundingBox.GetHeight() > str_height) {
                y += boundingBox.GetHeight() - str_height;
            }
            break;
    }
    
    // Set cursor and draw string
    SetCursor(x, y);
    WriteString(str, font, on);
    
    return Rectangle(x, y, str_width, str_height);
}

void GFX2Adapter::Update() {
    if (!display_) return;
    // Flush all pending changes to the display
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
    if (!layer_) return;
    
    DadGFX::sColor white(255, 255, 255, 255);
    DadGFX::sColor red(255, 0, 0, 255);
    DadGFX::sColor green(0, 255, 0, 255);
    DadGFX::sColor blue(0, 0, 255, 255);
    DadGFX::sColor yellow(255, 255, 0, 255);
    DadGFX::sColor cyan(0, 255, 255, 255);
    
    // Test 1: Verify basics work
    layer_->eraseLayer(red);
    display_->flush();
    daisy::System::Delay(1000);
    
    layer_->eraseLayer(green);
    display_->flush();
    daisy::System::Delay(1000);
    
    layer_->eraseLayer(blue);
    display_->flush();
    daisy::System::Delay(1000);
    
    // Clear to black
    layer_->eraseLayer(background_color_);
    display_->flush();
    
    // Test 2: DIRECT GFX2 pixel test (not through wrapper)
    layer_->drawFillRect(20, 20, 1, 1, red);  // Single pixel
    layer_->drawFillRect(25, 20, 5, 5, red);  // 5x5 block
    display_->flush();
    daisy::System::Delay(2000);
    
    layer_->eraseLayer(background_color_);
    display_->flush();
    
    // Test 3: DIRECT GFX2 line test
    layer_->drawLine(10, 30, 100, 30, green);      // Horizontal
    layer_->drawLine(10, 35, 10, 70, green);       // Vertical  
    layer_->drawLine(30, 40, 80, 70, green);       // Diagonal
    display_->flush();
    daisy::System::Delay(2000);
    
    layer_->eraseLayer(background_color_);
    display_->flush();
    
    // Test 4: DIRECT GFX2 rectangle test
    layer_->drawRect(10, 20, 40, 30, 1, blue);           // Outline
    layer_->drawFillRect(60, 20, 40, 30, blue);          // Filled
    display_->flush();
    daisy::System::Delay(2000);
    
    layer_->eraseLayer(background_color_);
    display_->flush();
    
    // Test 5: DIRECT GFX2 circle test
    layer_->drawCircle(40, 80, 25, yellow);    // Large circle
    layer_->drawCircle(90, 80, 15, yellow);    // Medium circle
    display_->flush();
    daisy::System::Delay(2000);
    
    layer_->eraseLayer(background_color_);
    display_->flush();
    
    // Test 6: DIRECT GFX2 arc test (move them further apart)
    layer_->drawArc(35, 60, 30, 0, 90, cyan);      // Left arc: 0° to 90°
    layer_->drawArc(95, 60, 25, 180, 270, cyan);   // Right arc: 180° to 270°
    // Also draw circles at the same positions to see the difference
    layer_->drawCircle(35, 120, 30, yellow);
    layer_->drawCircle(95, 120, 25, yellow);
    display_->flush();
    daisy::System::Delay(3000);
    
    // Test 7: Test through wrapper functions
    layer_->eraseLayer(background_color_);
    display_->flush();
    daisy::System::Delay(500);
    
    SetForegroundColor(255, 255, 255);
    
    // Test wrapper DrawPixel
    for (int i = 0; i < 30; i++) {
        DrawPixel(10 + i, 20, true);
    }
    Update();
    daisy::System::Delay(1000);
    
    // Test wrapper DrawLine
    DrawLine(10, 40, 100, 40, true);
    DrawLine(10, 45, 80, 80, true);
    Update();
    daisy::System::Delay(1000);
    
    // Test wrapper DrawRect
    DrawRect(10, 90, 50, 120, true, false);
    DrawRect(60, 90, 100, 120, true, true);
    Update();
    daisy::System::Delay(1000);
    
    // Test wrapper DrawCircle
    layer_->eraseLayer(background_color_);
    display_->flush();
    DrawCircle(40, 80, 30, true);
    DrawCircle(90, 80, 20, true);
    Update();
    daisy::System::Delay(2000);
    
    // Clear and finish
    Fill(false);
    Update();
}

} // namespace bkshepherd