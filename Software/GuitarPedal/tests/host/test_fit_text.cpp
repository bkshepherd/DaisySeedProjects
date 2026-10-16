// Fitting a text into the display (UI/fit_text.h): the biggest font that is wide enough, and a cut with a "~" when even the smallest is too wide.
#include "fit_text.h"
#include <cstdio>
#include <cstring>

static int g_fails = 0;
static void check(bool ok, const char *what) { std::printf("  %-84s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

int main() {
    using namespace fittext;
    char out[48];
    std::printf("choosing the font (110 pixels wide, as the name row after its arrows; 18 pixels high):\n");
    check(PickFont(10, 110, 18) == 0, "10 characters fit in the biggest font (11 x 18)");
    check(PickFont(11, 110, 18) == 1 && PickFont(13, 110, 18) == 1, "11 to 13 characters go to 8 x 13");
    check(PickFont(14, 110, 18) == 2 && PickFont(15, 110, 18) == 2, "14 and 15 characters go to 7 x 10");
    check(PickFont(16, 110, 18) == 3 && PickFont(18, 110, 18) == 3, "16 to 18 characters go to 6 x 8");
    check(PickFont(19, 110, 18) == 4 && PickFont(22, 110, 18) == 4, "19 to 22 characters go to 5 x 8");
    check(PickFont(0, 110, 18) == 0, "an empty text gets the biggest font");
    check(PickFont(1, 110, 9) == 3 && PickFont(1, 110, 12) == 2 && PickFont(1, 110, 13) == 1, "a low row never gets a font taller than it (6 x 8 under 10 pixels, 7 x 10 under 13, 8 x 13 under 18)");

    std::printf("\nfitting the text:\n");
    int f = Fit("Depth", 110, 18, out, sizeof out);
    check(f == 0 && std::strcmp(out, "Depth") == 0, "a short text is left as it is, in the biggest font");
    f = Fit("Delay Feedback", 110, 18, out, sizeof out);
    check(f == 2 && std::strcmp(out, "Delay Feedback") == 0, "a long name is drawn in full, in a smaller font (14 characters: 7 x 10)");
    f = Fit("Input Level", 110, 18, out, sizeof out);
    check(f == 1 && std::strcmp(out, "Input Level") == 0, "11 characters now get 8 x 13, the size between the big font and 7 x 10");
    f = Fit("Tr Mod Sustain Release Time", 110, 18, out, sizeof out); // 27 characters: more than 22
    check(f == 4 && std::strlen(out) == 22 && out[21] == kCutMark && std::strncmp(out, "Tr Mod Sustain Release", 21) == 0, "a text too long for the smallest font is cut to 22 characters ending in ~");
    f = Fit("1234567890123456789012", 110, 18, out, sizeof out); // exactly 22
    check(f == 4 && std::strcmp(out, "1234567890123456789012") == 0, "exactly what fits in the smallest font is not cut");
    f = Fit("Matchless SC30", 66, 18, out, sizeof out); // a narrow space: 66 pixels
    check(std::strlen(out) * kFonts[f].width <= 66 && out[std::strlen(out) - 1] == kCutMark, "a narrow space cuts earlier, and always to what fits");
    f = Fit("abcdef", 3, 18, out, sizeof out);
    check(std::strcmp(out, "") == 0 || std::strlen(out) <= 1, "a space too small for a character gives at most the mark");
    char tiny[4];
    f = Fit("a long text", 110, 18, tiny, sizeof tiny);
    check(std::strlen(tiny) <= 3, "the output never overruns its buffer");
    f = Fit("", 110, 18, out, sizeof out);
    check(f == 0 && out[0] == '\0', "an empty text stays empty");

    std::printf("\n%s\n", g_fails == 0 ? "ALL FIT TEXT TESTS PASSED" : "FIT TEXT TESTS FAILED");
    return g_fails == 0 ? 0 : 1;
}
