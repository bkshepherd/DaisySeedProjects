#include "heap_guard.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
heapguard::HaltHandler s_handler = nullptr;
const char *s_stage = "starting";
} // namespace

namespace heapguard {

void Summary(char *buffer, size_t size) { std::snprintf(buffer, size, "heap %uK free %uK", static_cast<unsigned>(Used() / 1024), static_cast<unsigned>(Free() / 1024)); }

void SetHaltHandler(HaltHandler handler) { s_handler = handler; }
void SetStage(const char *what) { s_stage = what; }

void Halt(const char *line1, const char *line2, const char *line3) {
    if (s_handler != nullptr) s_handler(line1, line2, line3);
#ifdef __arm__
    for (;;) {
    }
#else
    std::fprintf(stderr, "%s\n%s\n%s\n", line1, line2, line3);
    std::abort();
#endif
}

} // namespace heapguard

#ifdef __arm__
// `end` is where the heap starts and _ram_d2_limit the end of RAM_D2 (both from the linker scripts: nam_a2_sections.lds provides the limit)
extern "C" char end;
extern "C" char _ram_d2_limit;

namespace {
char *s_break = nullptr;
}

namespace heapguard {
size_t Used() { return s_break == nullptr ? 0 : static_cast<size_t>(s_break - &end); }
size_t Free() { return static_cast<size_t>(&_ram_d2_limit - (s_break == nullptr ? &end : s_break)); }
} // namespace heapguard

// Replaces libnosys's _sbrk (which has no limit): the same, except that running past the end of RAM_D2 stops with a message instead of a bus fault
extern "C" void *_sbrk(ptrdiff_t increment) {
    if (s_break == nullptr) s_break = &end;
    char *previous = s_break;
    if (increment > 0 && static_cast<size_t>(&_ram_d2_limit - s_break) < static_cast<size_t>(increment)) {
        char line2[32], line3[32];
        std::snprintf(line2, sizeof line2, "%s", s_stage);
        std::snprintf(line3, sizeof line3, "%uK used, +%uK", static_cast<unsigned>(heapguard::Used() / 1024), static_cast<unsigned>((increment + 1023) / 1024));
        heapguard::Halt("OUT OF MEMORY", line2, line3);
    }
    s_break += increment;
    return previous;
}
#else
// On the PC (the host tests): plenty of heap
namespace heapguard {
size_t Used() { return 0; }
size_t Free() { return static_cast<size_t>(1) << 30; }
} // namespace heapguard
#endif
