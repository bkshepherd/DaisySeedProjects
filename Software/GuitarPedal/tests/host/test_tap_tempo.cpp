// The series logic of the global tap tempo (Util/tap_tempo.h): three taps set the tempo from the last two, more taps keep adjusting it.
#include "../../Util/tap_tempo.h"
#include <cstdio>
static int fails = 0;
static void check(bool ok, const char *what) { fails += !ok; std::printf("  %-90s %s\n", what, ok ? "OK" : "FAILED"); }
int main() {
    int iv = -1;
    {
        TapTempoSeries t;
        check(!t.Tap(false, 0, iv), "the first tap does not set the tempo");
        check(!t.Tap(true, 24000, iv), "the second tap does not set the tempo (its interval is dropped)");
        check(t.Tap(true, 26000, iv) && iv == 26000, "the third tap sets the tempo from the interval of the last two");
        check(t.Tap(true, 25000, iv) && iv == 25000, "a fourth tap sets it again from its own interval");
        check(t.Tap(true, 20000, iv) && iv == 20000, "and a fifth");
    }
    {
        TapTempoSeries t;
        t.Tap(false, 0, iv); t.Tap(true, 24000, iv); t.Tap(true, 24000, iv);
        check(!t.Tap(false, 0, iv), "a tap that comes too late starts a new series");
        check(!t.Tap(true, 12000, iv), "the second tap of the new series does not set the tempo");
        check(t.Tap(true, 13000, iv) && iv == 13000, "the third tap of the new series sets it");
    }
    {
        TapTempoSeries t;
        t.Tap(false, 0, iv); t.Tap(true, 24000, iv);
        t.Reset(); // the effect does not use tap tempo: the series is forgotten, three more taps are needed
        check(!t.Tap(true, 24000, iv) && !t.Tap(true, 24000, iv), "after Reset the next two taps do not set the tempo");
        check(t.Tap(true, 22000, iv) && iv == 22000, "and the third does");
    }
    std::printf(fails ? "TAP TEMPO TESTS FAILED\n" : "ALL TAP TEMPO TESTS PASSED\n");
    return fails ? 1 : 0;
}
