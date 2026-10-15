// The heap guard on the PC: the figures are the PC fallbacks, and Halt shows its message through the handler before it stops (the real _sbrk limit only exists on the Daisy).
#include "heap_guard.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void handler(const char *l1, const char *l2, const char *l3) {
    const bool ok = std::strcmp(l1, "OUT OF MEMORY") == 0 && std::strcmp(l2, "starting Mars") == 0 && std::strcmp(l3, "heap full") == 0;
    std::printf("  the halt handler got its three lines: %s\n", ok ? "OK" : "FAIL");
    std::fflush(stdout);
    std::_Exit(ok ? 0 : 1); // Halt does not return: the test ends here
}

int main() {
    std::printf("heap guard (PC fallbacks):\n");
    const bool figures = heapguard::Free() > (1u << 20) && heapguard::Used() == 0;
    std::printf("  Free() is large and Used() is 0: %s\n", figures ? "OK" : "FAIL");
    std::fflush(stdout);
    if (!figures) return 1;
    char line[64];
    heapguard::Summary(line, sizeof line);
    const bool fits = std::strlen(line) <= 21 && std::strncmp(line, "heap ", 5) == 0; // the display is 21 characters wide in the 6 pixel font: wider text is not drawn
    std::printf("  the heap line (\"%s\") fits the display: %s\n", line, fits ? "OK" : "FAIL");
    std::fflush(stdout);
    if (!fits) return 1;
    heapguard::SetHaltHandler(handler);
    heapguard::SetStage("starting Mars");
    heapguard::Halt("OUT OF MEMORY", "starting Mars", "heap full");
}
