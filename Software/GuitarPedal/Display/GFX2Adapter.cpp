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

void GFX2Adapter::Fill(bool on) {
    if (!layer_) return;
    
    DadGFX::sColor fill_color = on ? foreground_color_ : background_color_;
    layer_->eraseLayer(fill_color);
}

void GFX2Adapter::Update() {
    // Flush all pending changes to the display
    display_.flush();
}

void GFX2Adapter::TestFill(uint8_t r, uint8_t g, uint8_t b) {
    if (!layer_) return;
    
    DadGFX::sColor test_color(r, g, b, 255);
    layer_->eraseLayer(test_color);
    display_.flush();
}

} // namespace bkshepherd