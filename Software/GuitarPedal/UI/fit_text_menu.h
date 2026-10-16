#pragma once
#ifndef FIT_TEXT_MENU_H
#define FIT_TEXT_MENU_H

#include "daisy_seed.h"

using namespace daisy;

namespace bkshepherd {

/** A FullScreenItemMenu that fits the texts it draws (the item name on the top row and the value below it) into the display: the biggest font in which they fit, cut with a "~" at
 * the end if even the smallest is too wide (see fit_text.h). libDaisy's menu picks the font from the height only and draws nothing for a line wider than the display.
 */
class FitTextMenu : public FullScreenItemMenu {
  protected:
    void DrawTopRow(OneBitGraphicsDisplay &display, bool isVertical, int currentIndex, int numItemsTotal, const char *text, Rectangle rect, bool isSelected) const override;
    void DrawValueText(OneBitGraphicsDisplay &display, bool isVertical, const char *text, Rectangle rect, bool isBeingEdited) const override;
};

/** Draws `text` centred in `rect` in the biggest font that fits (cut with a "~" if none does). */
void DrawFittedText(OneBitGraphicsDisplay &display, const char *text, Rectangle rect);

/** The arrows at the sides of a row (a copy of libDaisy's, whose own helpers are private); returns what is left of the rectangle. */
Rectangle DrawSideArrows(OneBitGraphicsDisplay &display, bool isVertical, Rectangle rect, bool firstAvailable, bool secondAvailable);

} // namespace bkshepherd
#endif
