#include "effect_selection_menu.h"

using namespace bkshepherd;

void EffectSelectionMenu::DrawTextItem(OneBitGraphicsDisplay &display, bool isVertical, uint16_t selectedItemIdx, uint16_t numItems,
                                       const char *itemText) const {
    Rectangle bottomRow = display.GetBounds();
    const Rectangle topRow = bottomRow.RemoveFromTop(bottomRow.GetHeight() / 2);
    display.WriteStringAligned("Effect", Font_11x18, topRow, Alignment::centered, true);

    // Same arrow shapes FullScreenItemMenu draws on the top row (its helpers are private).
    Rectangle leftArrow = bottomRow.RemoveFromLeft(9).WithSizeKeepingCenter(5, 9).Translated(0, -1);
    Rectangle rightArrow = bottomRow.RemoveFromRight(9).WithSizeKeepingCenter(5, 9).Translated(0, -1);

    if (selectedItemIdx > 0) {
        for (int16_t x = leftArrow.GetRight() - 1; x >= leftArrow.GetX(); x--) {
            display.DrawLine(x, leftArrow.GetY(), x, leftArrow.GetBottom(), true);
            leftArrow = leftArrow.Reduced(0, 1);
            if (leftArrow.IsEmpty())
                break;
        }
    }

    if (selectedItemIdx < numItems - 1) {
        for (int16_t x = rightArrow.GetX(); x < rightArrow.GetRight(); x++) {
            display.DrawLine(x, rightArrow.GetY(), x, rightArrow.GetBottom(), true);
            rightArrow = rightArrow.Reduced(0, 1);
            if (rightArrow.IsEmpty())
                break;
        }
    }

    display.WriteStringAligned(itemText, Font_11x18, bottomRow, Alignment::centered, true);
}
