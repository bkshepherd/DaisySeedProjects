// The delay line memory of the CloudSeed reverb engine (dependencies/CloudSeed), used by the CloudSeed and Jupiter modules.
// The modified engine allocates every delay line from this pool (in SDRAM, 64 MB available on the Daisy) through custom_pool_allocate(); there is no way to
// free memory, and a failed allocation returns null, which freezes the pedal. A ReverbController takes about 4.9 MB (measured on the host; the same for every
// preset and line count, because it allocates for the maximum), so the pool has to hold one per module that uses the engine: 2 x 4.9 MB fit in the 12.6 MB below.
// (A pool of 48 * 256 * 256 = 3.1 MB freezes, one reverb in 48 * 384 * 384 = 7.1 MB works.)
#include "daisy_seed.h"
#include <cstddef>

#define CUSTOM_POOL_SIZE (48 * 512 * 512)

DSY_SDRAM_BSS char custom_pool[CUSTOM_POOL_SIZE];
size_t pool_index = 0;
int allocation_count = 0;
void *custom_pool_allocate(size_t size) {
    if (pool_index + size >= CUSTOM_POOL_SIZE) {
        return 0;
    }
    void *ptr = &custom_pool[pool_index];
    pool_index += size;
    return ptr;
}
