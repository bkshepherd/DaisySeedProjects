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
    
    // Since setPixel() is protected, use drawLine from point to itself
    layer_->drawLine(x, y, x, y, color);
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
    
    // Convert angles to GFX2 format (0-360 degrees)
    uint16_t gfx_start = start_angle % 360;
    uint16_t gfx_end = (start_angle + sweep) % 360;
    
    // Handle negative angles
    if (gfx_start < 0) gfx_start += 360;
    if (gfx_end < 0) gfx_end += 360;
    
    // If drawing a full circle (sweep >= 360 or <= -360)
    if (abs(sweep) >= 360) {
        layer_->drawCircle(x, y, radius, color);
    } else {
        layer_->drawArc(x, y, radius, gfx_start, gfx_end, color);
    }
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
    
    // Test 1: Full screen colors - verify eraseLayer works
    DadGFX::sColor red(255, 0, 0, 255);
    layer_->eraseLayer(red);
    display_->flush();
    daisy::System::Delay(1000);
    
    DadGFX::sColor green(0, 255, 0, 255);
    layer_->eraseLayer(green);
    display_->flush();
    daisy::System::Delay(1000);
    
    DadGFX::sColor blue(0, 0, 255, 255);
    layer_->eraseLayer(blue);
    display_->flush();
    daisy::System::Delay(1000);
    
    // Clear to black for drawing tests
    layer_->eraseLayer(background_color_);
    display_->flush();
    daisy::System::Delay(500);
    
    // Test 2: DrawPixel - Draw a red cross pattern using pixels
    SetForegroundColor(255, 0, 0);  // Red
    for (int i = 0; i < 20; i++) {
        DrawPixel(20 + i, 20, true);      // Horizontal line
        DrawPixel(29, 11 + i, true);      // Vertical line
    }
    Update();
    daisy::System::Delay(2000);
    
    // Test 3: DrawLine - Draw green lines in different angles
    SetForegroundColor(0, 255, 0);  // Green
    DrawLine(50, 20, 90, 20, true);   // Horizontal
    DrawLine(50, 25, 50, 45, true);   // Vertical
    DrawLine(60, 25, 80, 45, true);   // Diagonal
    DrawLine(80, 25, 60, 45, true);   // Diagonal other way
    Update();
    daisy::System::Delay(2000);
    
    // Test 4: DrawRect - Draw blue rectangles (outline and filled)
    SetForegroundColor(0, 0, 255);  // Blue
    DrawRect(10, 60, 40, 80, true, false);  // Outline rectangle
    DrawRect(50, 60, 80, 80, true, true);   // Filled rectangle
    Update();
    daisy::System::Delay(2000);
    
    // Test 5: DrawCircle - Draw yellow circles
    SetForegroundColor(255, 255, 0);  // Yellow
    DrawCircle(30, 110, 15, true);  // Circle at (30, 110) radius 15
    DrawCircle(70, 110, 10, true);  // Circle at (70, 110) radius 10
    Update();
    daisy::System::Delay(2000);
    
    // Test 6: DrawArc - Draw cyan arcs
    SetForegroundColor(0, 255, 255);  // Cyan
    DrawArc(110, 30, 20, 0, 90, true);    // Quarter circle
    DrawArc(110, 70, 15, 45, 180, true);  // Half circle at 45°
    Update();
    daisy::System::Delay(2000);
    
    // Test 7: Fill - Test Fill function
    Fill(true);  // Fill with foreground (cyan)
    Update();
    daisy::System::Delay(1000);
    
    Fill(false);  // Fill with background (black)
    Update();
    daisy::System::Delay(500);
    
    // Final test: Draw a complete test pattern
    SetForegroundColor(255, 255, 255);  // White
    
    // Border
    DrawRect(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1, true, false);
    
    // Grid pattern
    for (int x = 20; x < TFT_WIDTH; x += 20) {
        DrawLine(x, 0, x, TFT_HEIGHT - 1, true);
    }
    for (int y = 20; y < TFT_HEIGHT; y += 20) {
        DrawLine(0, y, TFT_WIDTH - 1, y, true);
    }
    
    Update();
    daisy::System::Delay(3000);
    
    // Clear and finish
    Fill(false);
    Update();
}

} // namespace bkshepherd