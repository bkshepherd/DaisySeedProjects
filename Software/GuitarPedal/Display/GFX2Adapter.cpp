#include "GFX2Adapter.h"
#include <cstring>

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
        // Handle error - layer creation failed
        return;
    }
    
    // Set initial drawing colors
    layer_->setTextFrontColor(foreground_color_);
    layer_->setTextBackColor(background_color_);
    
    // Clear the screen to black
    layer_->eraseLayer(background_color_);
    display_.flush();
}

void GFX2Adapter::DrawPixel(uint_fast8_t x, uint_fast8_t y, bool on) {
    if (!layer_) return;
    
    DadGFX::sColor color = on ? foreground_color_ : background_color_;
    // Use drawFillRect to draw a 1x1 pixel
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
    
    // Calculate width and height
    uint16_t width = (x2 >= x1) ? (x2 - x1 + 1) : 1;
    uint16_t height = (y2 >= y1) ? (y2 - y1 + 1) : 1;
    
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
    
    // GFX2 uses 0-360 degrees, Daisy uses tenths of degrees
    // Convert: Daisy angle / 10 = degrees
    uint16_t start_deg = start_angle / 10;
    uint16_t end_deg = (start_angle + sweep) / 10;
    
    layer_->drawArc(x, y, radius, start_deg, end_deg, color);
}

char GFX2Adapter::WriteChar(char c, FontDef font, bool on) {
    // For now, just return the font width so UI can calculate positions
    // Actual text rendering can come later
    return font.FontWidth;
}

char GFX2Adapter::WriteString(const char* str, FontDef font, bool on) {
    if (!str) return 0;
    // Return total width
    return strlen(str) * font.FontWidth;
}

daisy::Rectangle GFX2Adapter::WriteStringAligned(const char* str,
                                          const FontDef& font,
                                          daisy::Rectangle boundingBox,
                                          daisy::Alignment alignment,
                                          bool on) {
    if (!str || !layer_) return daisy::Rectangle(0, 0, 0, 0);
    
    // Calculate text dimensions
    uint16_t textWidth = strlen(str) * font.FontWidth;
    uint16_t textHeight = font.FontHeight;
    
    // Actually draw the text background box so we can see SOMETHING
    DadGFX::sColor color = on ? foreground_color_ : background_color_;
    layer_->drawFillRect(boundingBox.GetX(), boundingBox.GetY(), 
                         boundingBox.GetWidth(), boundingBox.GetHeight(), color);
    
    // Return a rectangle representing where the text would be
    return daisy::Rectangle(boundingBox.GetX(), boundingBox.GetY(), textWidth, textHeight);
}

void GFX2Adapter::Fill(bool on) {
    if (!layer_) return;
    
    DadGFX::sColor fill_color = on ? foreground_color_ : background_color_;
    layer_->eraseLayer(fill_color);
}

void GFX2Adapter::Update() {
    // Three-layer protection against blocking:
    
    // 1. Rate limit to 20Hz
    static uint32_t lastFlush = 0;
    uint32_t now = daisy::System::GetNow();
    
    if (now - lastFlush < 50) {
        return;  // Too soon, skip this update
    }
    
    // 2. Check if we're already in a flush (shouldn't happen but be safe)
    static bool flushing = false;
    if (flushing) {
        return;  // Already flushing, don't re-enter
    }
    
    // 3. Perform the flush with re-entry protection
    flushing = true;
    display_.flush();
    flushing = false;
    
    lastFlush = now;
}

void GFX2Adapter::TestFill(uint8_t r, uint8_t g, uint8_t b) {
    if (!layer_) return;
    
    DadGFX::sColor test_color(r, g, b, 255);
    layer_->eraseLayer(test_color);
    display_.flush();
}

} // namespace bkshepherd