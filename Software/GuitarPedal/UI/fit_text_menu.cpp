#include "fit_text_menu.h"
#include "fit_text.h"
#include "font_8x13.h"

using namespace bkshepherd;

namespace {
const FontDef *const kFontDefs[] = {&Font_11x18, &Font_8x13, &Font_7x10, &Font_6x8, &Font_5x8}; // the order of fittext::kFonts
}

void bkshepherd::DrawFittedText(OneBitGraphicsDisplay &display, const char *text, Rectangle rect) {
    char fitted[48];
    const int font = fittext::Fit(text, rect.GetWidth(), rect.GetHeight(), fitted, sizeof(fitted));
    display.WriteStringAligned(fitted, *kFontDefs[font], rect, Alignment::centered, true);
}

Rectangle bkshepherd::DrawSideArrows(OneBitGraphicsDisplay &display, bool isVertical, Rectangle rect, bool firstAvailable, bool secondAvailable) {
    if (!isVertical) { // left and right arrows
        auto leftArrowRect = rect.RemoveFromLeft(9).WithSizeKeepingCenter(5, 9).Translated(0, -1);
        auto rightArrowRect = rect.RemoveFromRight(9).WithSizeKeepingCenter(5, 9).Translated(0, -1);
        if (firstAvailable) {
            for (int16_t x = leftArrowRect.GetRight() - 1; x >= leftArrowRect.GetX(); x--) {
                display.DrawLine(x, leftArrowRect.GetY(), x, leftArrowRect.GetBottom(), true);
                leftArrowRect = leftArrowRect.Reduced(0, 1);
                if (leftArrowRect.IsEmpty()) break;
            }
        }
        if (secondAvailable) {
            for (int16_t x = rightArrowRect.GetX(); x < rightArrowRect.GetRight(); x++) {
                display.DrawLine(x, rightArrowRect.GetY(), x, rightArrowRect.GetBottom(), true);
                rightArrowRect = rightArrowRect.Reduced(0, 1);
                if (rightArrowRect.IsEmpty()) break;
            }
        }
    } else { // up and down arrows
        auto upArrowRect = rect.RemoveFromLeft(9).WithSizeKeepingCenter(9, 5);
        auto downArrowRect = rect.RemoveFromRight(9).WithSizeKeepingCenter(9, 5);
        if (firstAvailable) {
            for (int16_t y = upArrowRect.GetBottom() - 1; y >= upArrowRect.GetY(); y--) {
                display.DrawLine(upArrowRect.GetX(), y, upArrowRect.GetRight(), y, true);
                upArrowRect = upArrowRect.Reduced(1, 0);
                if (upArrowRect.IsEmpty()) break;
            }
        }
        if (secondAvailable) {
            for (int16_t y = downArrowRect.GetY(); y < upArrowRect.GetBottom(); y++) {
                display.DrawLine(downArrowRect.GetX(), y, downArrowRect.GetRight(), y, true);
                downArrowRect = downArrowRect.Reduced(1, 0);
                if (downArrowRect.IsEmpty()) break;
            }
        }
    }
    return rect;
}

void FitTextMenu::DrawTopRow(OneBitGraphicsDisplay &display, bool isVertical, int currentIndex, int numItemsTotal, const char *text, Rectangle rect, bool isSelected) const {
    if (isSelected) rect = DrawSideArrows(display, isVertical, rect, currentIndex > 0, currentIndex < numItemsTotal - 1);
    DrawFittedText(display, text, rect);
}

void FitTextMenu::DrawValueText(OneBitGraphicsDisplay &display, bool isVertical, const char *text, Rectangle rect, bool isBeingEdited) const {
    if (isBeingEdited) rect = DrawSideArrows(display, isVertical, rect, true, true);
    DrawFittedText(display, text, rect);
}
