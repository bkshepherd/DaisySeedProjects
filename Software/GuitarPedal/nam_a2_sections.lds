/*
    nam_a2_sections.lds

    Supplemental linker fragment for the NAM A2 module.

    Adds a `.sram_d2_bss` output section in the 256 KB D2-domain SRAM (RAM_D2,
    0x30008000) so the large NAM A2 history buffer (~76 KB) can live outside the
    cramped 128 KB DTCMRAM while still being ON-CHIP.

    Why RAM_D2 and not the AXI-SRAM (D1, 0x24000000): under BOOT_SRAM the program
    (.text/.rodata) already fills most of the 480 KB AXI-SRAM, so putting the
    history there overflows it. RAM_D2 is otherwise empty. It is cacheable — only
    the first 32 KB (RAM_D2_DMA) is marked non-cacheable by libDaisy's MPU for DMA
    coherency; RAM_D2 at 0x30008000 keeps the default Write-Back attributes and the
    D-cache is enabled, so the runtime's scattered dilated history reads hit cache.
    Either way it is far faster than external SDRAM (0xC0000000), whose multi-cycle
    CAS latency on a cache miss made the runtime overrun the audio callback.

    The hot weights and work buffers stay in DTCMRAM (see NAM_A2_HOT_DATA in
    nam_a2_runtime.h). Only A2State (history + ring write positions) moves here.

    Wiring (see Makefile): this file is passed to the linker with `-T` BEFORE
    libDaisy's main script (STM32H750IB_sram.lds). Because it contains only an
    INSERT command, GNU ld augments rather than replaces the main script, and the
    `RAM_D2` MEMORY region defined there is visible to us. Per the ld manual the
    INSERT script must be listed first on the command line. Do NOT add a MEMORY
    command here.

    Placement (INSERT BEFORE .heap): this just keeps the history at the bottom of
    RAM_D2. The active heap is NOT here — newlib's _sbrk grows it from `end` in
    DTCMRAM, and libDaisy's RAM_D2 `.heap` section is unused on this target — so the
    exact INSERT point is benign. (Keeping the history out of DTCMRAM is what leaves
    that DTCMRAM heap its headroom.)
*/

SECTIONS
{
    .sram_d2_bss (NOLOAD) :
    {
        . = ALIGN(32);
        *(.sram_d2_bss)
        *(.sram_d2_bss*)
        . = ALIGN(32);
    } > RAM_D2
} INSERT BEFORE .heap;
