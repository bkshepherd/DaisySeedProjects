#pragma once
#include "dev/oled_ssd130x.h"
#include "../dependencies/GFX2/cDisplay.h"
#include "../dependencies/GFX2/GFX.h"

namespace bkshepherd {

/**
 * @brief Adapter that makes GFX2 look like OneBitGraphicsDisplay
 * 
 * Phase 1: Basic structure and initialization only
 */
class GFX2Adapter : public daisy::OneBitGraphicsDisplay {
public:
    GFX2Adapter();
    ~GFX2Adapter();

    // Initialize the display with GFX2
    void Init();

    // ===== OneBitGraphicsDisplay PURE VIRTUAL interface =====
    // Note: Return types and signatures must match exactly!
    
    // 1. Dimensions
uint16_t Height() const override { return TFT_HEIGHT; }
uint16_t Width() const override { return TFT_WIDTH; }

// 2. Fill
void Fill(bool on) override;

// 3. Pixel drawing
void DrawPixel(uint_fast8_t x, uint_fast8_t y, bool on) override;

// 4. Line drawing
void DrawLine(uint_fast8_t x1, uint_fast8_t y1, 
              uint_fast8_t x2, uint_fast8_t y2, bool on) override;

// 5. Rectangle drawing
void DrawRect(uint_fast8_t x1, uint_fast8_t y1,
              uint_fast8_t x2, uint_fast8_t y2,
              bool on, bool fill = false) override;

// 6. Arc drawing
void DrawArc(uint_fast8_t x, uint_fast8_t y, uint_fast8_t radius,
             int_fast16_t start_angle, int_fast16_t sweep, bool on) override;

// 7. Text functions
char WriteChar(char c, FontDef font, bool on) override;

char WriteString(const char* str, FontDef font, bool on) override;

// 8. Aligned text
daisy::Rectangle WriteStringAligned(const char* str,
                                   const FontDef& font,
                                   daisy::Rectangle boundingBox,
                                   daisy::Alignment alignment,
                                   bool on) override;

// 9. Update functions
void Update() override;
bool UpdateFinished() override { return true; }

    // ===== REMOVED: These are NON-VIRTUAL helpers in base class =====
    // DO NOT override these - they're already implemented in OneBitGraphicsDisplay
    // - Rectangle GetBounds() const
    // - void DrawRect(const Rectangle& rect, bool on, bool fill)
    // - void DrawCircle(uint_fast8_t x, uint_fast8_t y, uint_fast8_t radius, bool on)
    // - void SetCursor(uint16_t x, uint16_t y)

    // For testing - fill screen with specific color
    void TestFill(uint8_t r, uint8_t g, uint8_t b);

private:
    DadGFX::cDisplay display_;
    DadGFX::cLayer* layer_;
    
    // Static storage (shared by all instances, but we only have one)
    static DadGFX::sFIFO_Data fifo_data_ __attribute__((section(".sram1_bss")));
    static DadGFX::sColor bloc_frame_[BLOC_HEIGHT][BLOC_WIDTH] __attribute__((section(".sdram_bss")));
    static DadGFX::sColor layer_framebuffer_[TFT_HEIGHT][TFT_WIDTH] __attribute__((section(".sdram_bss")));
    
    // Current drawing state
    DadGFX::sColor foreground_color_;
    DadGFX::sColor background_color_;
    uint_fast8_t cursor_x_;
    uint_fast8_t cursor_y_;
};

} // namespace bkshepherd