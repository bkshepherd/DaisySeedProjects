#include "GFX2Adapter.h"
#include <cstring>
#include <cmath>

namespace bkshepherd {

// Define static storage
DadGFX::sFIFO_Data GFX2Adapter::fifo_data_;
DadGFX::sColor GFX2Adapter::bloc_frame_[BLOC_HEIGHT][BLOC_WIDTH];
DadGFX::sColor GFX2Adapter::layer_framebuffer_[TFT_HEIGHT][TFT_WIDTH];

GFX2Adapter::GFX2Adapter()
    : layer_(nullptr),
      foreground_color_(255, 255, 255, 255),  // White
      background_color_(0, 0, 0, 255),        // Black
      cursor_x_(0),
      cursor_y_(0) {
}

GFX2Adapter::~GFX2Adapter() {
    // Memory is statically allocated, no cleanup needed
}

void GFX2Adapter::Init() {
    // Initialize the GFX2 display system
    display_.init(&fifo_data_, &bloc_frame_[0][0]);
    
    // Create primary layer (full screen, Z-order 1)
    layer_ = display_.addLayer(&layer_framebuffer_[0][0], 0, 0, 
                               TFT_WIDTH, TFT_HEIGHT, 1);
    
    if (!layer_) {
        return;  // Failed to create layer
    }
    
    // Set initial drawing colors
    layer_->setTextFrontColor(foreground_color_);
    layer_->setTextBackColor(background_color_);
    
    // Clear the screen to black
    layer_->eraseLayer(background_color_);
    display_.flush();
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
    layer_->setPixel(x, y, color);
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
    // OneBitGraphicsDisplay uses: 0° = right, counterclockwise
    // GFX2 uses: 0° = right, clockwise (standard)
    
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
    // Flush all pending changes to the display
    display_.flush();
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
    display_.flush();
}

void GFX2Adapter::TestDrawingPrimitives() {
    if (!layer_) return;
    
    // Clear screen to black
    Fill(false);
    Update();
    daisy::System::Delay(500);
    
    // Test 1: Draw some pixels
    SetForegroundColor(255, 0, 0);  // Red
    for (int i = 0; i < 20; i++) {
        DrawPixel(10 + i, 10, true);
        DrawPixel(10 + i, 11, true);
    }
    Update();
    daisy::System::Delay(500);
    
    // Test 2: Draw lines
    SetForegroundColor(0, 255, 0);  // Green
    DrawLine(10, 20, 50, 60, true);
    DrawLine(50, 20, 10, 60, true);
    Update();
    daisy::System::Delay(500);
    
    // Test 3: Draw rectangles
    SetForegroundColor(0, 0, 255);  // Blue
    DrawRect(70, 10, 110, 30, true, false);  // Outline
    DrawRect(70, 40, 110, 60, true, true);   // Filled
    Update();
    daisy::System::Delay(500);
    
    // Test 4: Draw circles
    SetForegroundColor(255, 255, 0);  // Yellow
    DrawCircle(40, 100, 15, true);
    DrawCircle(80, 100, 20, true);
    Update();
    daisy::System::Delay(1000);
    
    // Clear and finish
    Fill(false);
    Update();
}

} // namespace bkshepherd