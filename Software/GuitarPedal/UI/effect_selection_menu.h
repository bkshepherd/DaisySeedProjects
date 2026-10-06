#pragma once
#ifndef EFFECT_SELECTION_MENU_H
#define EFFECT_SELECTION_MENU_H

#include "daisy_seed.h"

using namespace daisy;

namespace bkshepherd {

/** A FullScreenItemMenu that keeps "Effect" on the top row and shows each item on the bottom row,
 * so browsing effects looks like editing a value rather than navigating a menu.
 */
class EffectSelectionMenu : public FullScreenItemMenu {
  private:
    void DrawTextItem(OneBitGraphicsDisplay &display, bool isVertical, uint16_t selectedItemIdx, uint16_t numItems,
                      const char *itemText) const override;
};
} // namespace bkshepherd
#endif
