// Fitting a text into a width on the display: pick the largest font that is wide enough, and cut what does not fit even in the smallest.
//
// libDaisy draws nothing at all for a line that is wider than the display, and its menus choose the font from the height of the space only, so a long parameter name or value
// (anything over 10 characters in the big font) used to vanish. This is the logic that fixes it; it knows nothing of the display, so the host tests cover it (tests/host/test_fit_text.cpp).
#pragma once
#ifndef FIT_TEXT_H
#define FIT_TEXT_H

#include <cstddef>

namespace fittext {

struct FontSize {
    int width;
    int height;
};

// From big to small: Font_11x18, Font_8x13 (ours, UI/font_8x13.cpp), Font_7x10, Font_6x8, Font_5x8 (libDaisy's, see util/oled_fonts.h). 5x8 is the smallest that stays easy to read.
constexpr FontSize kFonts[] = {{11, 18}, {8, 13}, {7, 10}, {6, 8}, {5, 8}};
constexpr int kFontCount = sizeof(kFonts) / sizeof(kFonts[0]);
constexpr char kCutMark = '~'; // the last character of a text that was cut

/** The biggest font that fits in the height (11x18 from 18 pixels, 8x13 from 13, 7x10 from 10, otherwise 6x8: libDaisy's menus have the rule without the 8x13). Returns an index into kFonts. */
inline int BiggestFontForHeight(int height) { return height >= 18 ? 0 : (height >= 13 ? 1 : (height >= 10 ? 2 : 3)); }

/** The index of the biggest font, not bigger than the height allows, in which `length` characters fit into `width` pixels; the smallest font if none does. */
inline int PickFont(size_t length, int width, int height) {
    for (int i = BiggestFontForHeight(height); i < kFontCount; i++) {
        if (static_cast<int>(length) * kFonts[i].width <= width) return i;
    }
    return kFontCount - 1;
}

/** Copies the text into `out` (of `outSize` bytes) and returns the font index to draw it in: the biggest that fits, and if even the smallest is too wide the text is cut to what fits
 * with a kCutMark as its last character ("Matchless SC30" -> "Matchless S~"). */
inline int Fit(const char *text, int width, int height, char *out, size_t outSize) {
    size_t length = 0;
    while (text[length] != '\0') length++;
    const int font = PickFont(length, width, height);
    size_t keep = length;
    const size_t maxChars = static_cast<size_t>(width > 0 ? width / kFonts[font].width : 0);
    if (length > maxChars) keep = maxChars > 0 ? maxChars - 1 : 0; // room for the mark
    if (keep + 2 > outSize) keep = outSize >= 2 ? outSize - 2 : 0;
    size_t i = 0;
    for (; i < keep; i++) out[i] = text[i];
    if (length > maxChars && outSize >= 2) out[i++] = kCutMark;
    if (outSize > 0) out[i] = '\0';
    return font;
}

} // namespace fittext

#endif
