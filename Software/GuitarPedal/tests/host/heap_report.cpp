// Counts what each converted effect allocates with `new` during construction and Init(), on the host. The Daisy's heap (newlib, in RAM_D2 after the static data) is small and
// unchecked, so this is how much of it each effect would take, to build the matrix of which effects fit together (see PERFORMANCE_AND_MEMORY.md).
// Host pointers are 8 bytes (the Daisy's 4), so the figures are an overestimate for pointer-heavy objects (the CloudSeed engine is mostly small allocations).
#include <cstdio>
#include <cstdlib>
#include <map>
#include <new>
static size_t g_bytes = 0, g_count = 0;
static std::map<size_t, size_t> *g_hist = nullptr;
static bool g_histOn = false;
static bool g_busy = false; // the bookkeeping below allocates too: not counted
static void record(size_t n) {
    if (g_busy) return;
    g_busy = true;
    g_bytes += n;
    g_count++;
    if (g_histOn) {
        if (!g_hist) g_hist = new std::map<size_t, size_t>;
        (*g_hist)[n]++;
    }
    g_busy = false;
}
void *operator new(size_t n) { record(n); return malloc(n); }
void *operator new[](size_t n) { record(n); return malloc(n); }
void operator delete(void *p) noexcept { free(p); }
void operator delete[](void *p) noexcept { free(p); }
void operator delete(void *p, size_t) noexcept { free(p); }
void operator delete[](void *p, size_t) noexcept { free(p); }

#include "balnea_module.h"
#include "cassini_module.h"
#include "jupiter_module.h"
#include "mars_module.h"
#include "mercury_module.h"
#include "pluto_module.h"
#include "neptune_module.h"
#include "saturn_module.h"
#include "uranus_module.h"
#include "venus_module.h"

template <class M> static void measure(const char *name) {
    const size_t b0 = g_bytes, c0 = g_count;
    g_busy = true;
    g_hist = new std::map<size_t, size_t>;
    g_busy = false;
    g_histOn = true;
    M *m = new M();
    m->Init(48000.0f);
    g_histOn = false;
    size_t chunks = 0; // newlib rounds a request up to a multiple of 8 and adds a 4 byte header (minimum chunk 16)
    for (auto &kv : *g_hist) {
        size_t chunk = ((kv.first + 4 + 7) / 8) * 8;
        if (chunk < 16) chunk = 16;
        chunks += chunk * kv.second;
    }
    printf("%-8s sizeof %6zu   new: %7zu bytes in %5zu calls   about %7zu bytes of heap chunks\n", name, sizeof(M), g_bytes - b0, g_count - c0, chunks);
}

int main() {
    measure<bkshepherd::VenusModule>("Venus");
    measure<bkshepherd::JupiterModule>("Jupiter");
    measure<bkshepherd::SaturnModule>("Saturn");
    measure<bkshepherd::NeptuneModule>("Neptune");
    measure<bkshepherd::UranusModule>("Uranus");
    measure<bkshepherd::BalneaModule>("Balnea");
    measure<bkshepherd::PlutoModule>("Pluto");
    measure<bkshepherd::CassiniModule>("Cassini");
    measure<bkshepherd::MarsModule>("Mars");
    measure<bkshepherd::MercuryModule>("Mercury");
    printf("(host build: 8 byte pointers; the Daisy's figures are lower for pointer-heavy objects)\n");
}
