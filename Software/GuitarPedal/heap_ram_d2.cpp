// heap_ram_d2.cpp
//
// Relocates the C/C++ heap (malloc/new) out of the cramped 128 KB DTCMRAM and
// into the 256 KB D2-domain SRAM (RAM_D2).
//
// Why this is needed (BOOT_SRAM):
//   libDaisy's STM32H750IB_sram.lds defines a `.heap` section in RAM_D2, but the
//   linked allocator is newlib-nano, whose libnosys `_sbrk` ignores that section
//   and instead grows the heap from `end` (the end of .bss) — which lives in
//   DTCMRAM. With every effect enabled, the IR / granular / spectral allocations
//   exceed the DTCMRAM space between `end` and the stack, and because libnosys
//   `_sbrk` performs NO bounds checking it hands back out-of-range pointers; the
//   first such allocation (ImpulseResponse's vector resize) then memsets into
//   unmapped memory and HardFaults.
//
// This file provides a strong `_sbrk` that overrides the libnosys stub. It serves
// allocations from a fixed buffer in RAM_D2 and, crucially, BOUNDS-CHECKS them:
// an over-allocation now returns nullptr / throws std::bad_alloc cleanly instead
// of faulting. RAM_D2 is on-chip and cacheable, so heap performance is unaffected.
//
// Layout in RAM_D2 (origin 0x30008000, 256 KB):
//   - NAM A2 history buffer (~76 KB, .sram_d2_bss, see nam_a2_sections.lds)
//   - this heap (RAM_D2_HEAP_SIZE)
//   Both share the .sram_d2_bss section; their sum must stay under 256 KB.
//
// Verify after building: `grep _sbrk build/guitarpedal.map` should resolve _sbrk
// to this object (heap_ram_d2.o), not libnosys.a(sbrk.o).

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

// ~76 KB is taken by the NAM history; 160 KB heap leaves ~20 KB of RAM_D2 slack.
// Bump toward ~176 KB if a build needs more heap (watch the RAM_D2 region total).
#ifndef RAM_D2_HEAP_SIZE
#define RAM_D2_HEAP_SIZE (160 * 1024)
#endif

alignas(8) static uint8_t s_heap[RAM_D2_HEAP_SIZE]
    __attribute__((section(".sram_d2_bss")));
static uint8_t *s_brk = s_heap;

extern "C" void *_sbrk(ptrdiff_t incr)
{
    if (incr < 0 || s_brk + incr > s_heap + RAM_D2_HEAP_SIZE)
    {
        errno = ENOMEM;
        return reinterpret_cast<void *>(-1);
    }
    uint8_t *const prev = s_brk;
    s_brk += incr;
    return prev;
}
