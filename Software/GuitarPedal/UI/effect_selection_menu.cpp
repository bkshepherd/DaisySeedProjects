#include "effect_selection_menu.h"

using namespace bkshepherd;

void EffectSelectionMenu::DrawTextItem(OneBitGraphicsDisplay &display, bool isVertical, uint16_t selectedItemIdx, uint16_t numItems,
                                       const char *itemText) const {
    Rectangle bottomRow = display.GetBounds();
    const Rectangle topRow = bottomRow.RemoveFromTop(bottomRow.GetHeight() / 2);
    display.WriteStringAligned("Effect", Font_11x18, topRow, Alignment::centered, true);

    // The arrows, and the effect's name in the biggest font that fits
    bottomRow = DrawSideArrows(display, isVertical, bottomRow, selectedItemIdx > 0, selectedItemIdx < numItems - 1);
    DrawFittedText(display, itemText, bottomRow);
}
