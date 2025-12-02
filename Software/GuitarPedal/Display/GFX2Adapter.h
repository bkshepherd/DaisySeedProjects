#pragma once
#include "dev/oled_ssd130x.h"
#include "../dependencies/GFX2/cDisplay.h"
#include "../dependencies/GFX2/GFX.h"

namespace bkshepherd {

/**
 * @brief Adapter that makes GFX2 look like OneBitGraphicsDisplay
 * 
 * Phase 2: Core drawing primitives implemented
 * Using GFX2's macro-based memory management
 */
class GFX2Adapter : public daisy::OneBitGraphicsDisplay {
public:
    GFX2Adapter();
    ~GFX2Adapter();

    // Initialize the display with GFX2
    void Init();

    // ===== OneBitGraphicsDisplay PURE VIRTUAL interface =====
    
    // Dimensions
    uint16_t Height() const override { return TFT_HEIGHT; }
    uint16_t Width() const override { return TFT_WIDTH; }
    
    // Fill screen
    void Fill(bool on) override;
    
    // Pixel drawing
    void DrawPixel(uint_fast8_t x, uint_fast8_t y, bool on) override;
    
    // Line drawing
    void DrawLine(uint_fast8_t x1, uint_fast8_t y1, 
                  uint_fast8_t x2, uint_fast8_t y2, bool on) override;
    
    // Rectangle drawing
    void DrawRect(uint_fast8_t x1, uint_fast8_t y1,
                  uint_fast8_t x2, uint_fast8_t y2,
                  bool on, bool fill = false) override;
    
    // Arc drawing (used by DrawCircle)
    void DrawArc(uint_fast8_t x, uint_fast8_t y, uint_fast8_t radius,
                 int_fast16_t start_angle, int_fast16_t sweep, bool on) override;
    
    // Text functions - Phase 3 stubs
    char WriteChar(char c, FontDef font, bool on) override {
        return 7;  // Stub
    }
    
    char WriteString(const char* str, FontDef font, bool on) override {
        if (!str) return 0;
        return strlen(str) * 7;  // Stub
    }
    
    Rectangle WriteStringAligned(const char* str,
                                 const FontDef& font,
                                 Rectangle boundingBox,
                                 Alignment alignment,
                                 bool on) override {
        return Rectangle(0, 0, 10, 10);  // Stub
    }
    
    // Cursor control (not part of base interface)
    void SetCursor(uint_fast8_t x, uint_fast8_t y) {
        cursor_x_ = x;
        cursor_y_ = y;
    }
    
    // Update functions
    void Update() override;
    bool UpdateFinished() override { return true; }

    // Testing utilities
    void TestFill(uint8_t r, uint8_t g, uint8_t b);
    void TestDrawingPrimitives();

    // Color control
    void SetForegroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void SetBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

private:
    DadGFX::cDisplay* display_;
    DadGFX::cLayer* layer_;
    
    // Current drawing state
    DadGFX::sColor foreground_color_;
    DadGFX::sColor background_color_;
    uint_fast8_t cursor_x_;
    uint_fast8_t cursor_y_;
};

} // namespace bkshepherd