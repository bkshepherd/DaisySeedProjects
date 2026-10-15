// The delay line memory of the CloudSeed reverb engine (dependencies/CloudSeed), used by the CloudSeed and Jupiter modules.
// The modified engine allocates every delay line from this pool (in SDRAM, 64 MB available on the Daisy) through custom_pool_allocate(); there is no way to
// free memory, and a failed allocation returns null, which freezes the pedal. A ReverbController takes about 4.9 MB (measured on the host; the same for every
// preset and line count, because it allocates for the maximum), so the pool has to hold one per module that uses the engine (CloudSeed, Jupiter, Neptune): 3 x 4.9 MB fit in the 19.7 MB below.
// (A pool of 48 * 256 * 256 = 3.1 MB freezes, one reverb in 48 * 384 * 384 = 7.1 MB works.)
#include "daisy_seed.h"
#include <cstddef>

#include "../Util/heap_guard.h"

// The size follows the number of effects that use the engine: CLOUDSEED_ENGINES is set by the build (tools/effects.py counts them: -DCLOUDSEED_ENGINES in generated/defines.mk); 3 if it is not set (the host tests).
#ifndef CLOUDSEED_ENGINES
#define CLOUDSEED_ENGINES 3
#endif
#if CLOUDSEED_ENGINES <= 1
#define CUSTOM_POOL_SIZE (48 * 384 * 384) // 7.1 MB
#elif CLOUDSEED_ENGINES == 2
#define CUSTOM_POOL_SIZE (48 * 512 * 512) // 12.6 MB
#else
#define CUSTOM_POOL_SIZE (48 * 640 * 640) // 19.7 MB
#endif

DSY_SDRAM_BSS char custom_pool[CUSTOM_POOL_SIZE];
size_t pool_index = 0;
int allocation_count = 0;
void *custom_pool_allocate(size_t size) {
    if (pool_index + size >= CUSTOM_POOL_SIZE) {
#ifdef __arm__
        // The engine does not cope with a null pointer (it freezes the pedal): say what is wrong instead
        heapguard::Halt("OUT OF MEMORY", "CloudSeed pool full", "see CLOUDSEED_ENGINES");
#else
        return 0;
#endif
    }
    void *ptr = &custom_pool[pool_index];
    pool_index += size;
    return ptr;
}
