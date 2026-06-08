/*
    nam_a2_sections.lds

    Supplemental linker fragment for the NAM A2 module.

    Adds an `.axi_sram_bss` output section in the 480 KB AXI-SRAM (D1 domain,
    0x24000000) so the large NAM A2 history buffer (~76 KB) can live outside the
    cramped 128 KB DTCMRAM while still being ON-CHIP. AXI-SRAM is far faster than
    the external SDRAM (0xC0000000): even on a D-cache miss it refills in a few
    cycles, whereas SDRAM's multi-cycle CAS latency made the runtime's scattered,
    dilated history reads too slow (callback overruns).

    The hot weights and work buffers stay in DTCMRAM (see NAM_A2_HOT_DATA in
    nam_a2_runtime.h). Only A2State (history + ring write positions) moves here.

    Wiring (see Makefile): this file is passed to the linker with `-T` BEFORE
    libDaisy's main script (STM32H750IB_sram.lds). Because it contains only an
    INSERT command, GNU ld augments rather than replaces the main script, and the
    `SRAM` MEMORY region defined there is visible to us. Per the ld manual the
    INSERT script must be listed first on the command line. Do NOT add a MEMORY
    command here.
*/

SECTIONS
{
    .axi_sram_bss (NOLOAD) :
    {
        . = ALIGN(32);
        *(.axi_sram_bss)
        *(.axi_sram_bss*)
        . = ALIGN(32);
    } > SRAM
} INSERT AFTER .bss;
