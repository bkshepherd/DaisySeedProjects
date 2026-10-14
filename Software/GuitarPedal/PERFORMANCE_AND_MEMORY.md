# Performance and memory notes

Written for anyone who picks this up later: what was learned while converting the FunBox effects (Venus, Jupiter, Saturn, Neptune, Uranus) about the CPU and memory limits of the pedal, which
optimisations made a difference and by how much, what each effect needs in memory, how the compiler change affected things, and a first draft of a matrix of which effects can be enabled together.
It complements `HANDOFF_funbox_effects.md` (what was converted and how), `HANDOFF_block_processing.md` (the earlier block-processing work) and `HANDOFF_level_calibration.md`.

**How reliable is it?** Each figure below says where it comes from. *Measured on the pedal* means the author read it off the pedal's Show Debug screen (CPU average/peak, the timing lines)
or saw it boot. *Build report* means the linker's memory table. *Host* means measured on the PC with the host tools in `tests/host/`. *Estimate* or *inferred* means exactly that. Nothing was
verified on the pedal after the switch to GCC 15 yet (see "The compiler").

## 1. The memory map and why it matters

The firmware boots from SRAM (`APP_TYPE = BOOT_SRAM`). The regions that matter (build report, `arm-none-eabi-size` / the linker's "Memory region" table):

| Region | Size | What lives there |
|---|---|---|
| AXI SRAM ("SRAM") | 480 KB | The program (code and read-only data, including NAM weights and the FFT tables) |
| DTCM | 128 KB | Ordinary global variables (`.bss`, `.data`) and the main stack (`_estack = 0x20020000`). Fast. |
| RAM_D2 | 256 KB | Our `.sram_d2_bss` section (see `nam_a2_sections.lds`: Saturn and Uranus buffers, the NAM history), then **the heap** |
| SDRAM | 64 MB | Anything marked `DSY_SDRAM_BSS`: delay lines, the CloudSeed pool. Large but **slow** (see section 3) |

**The heap is in RAM_D2, right after the static data in that region, and nothing limits it.** The symbol `end` (where newlib's `sbrk` starts) is `0x3003de20` in the all-effects build: RAM_D2 starts at
`0x30008000`, so that is after the 220,704 bytes of `.sram_d2_bss`. The heap can grow up to the end of RAM_D2 (`0x30048000`); beyond that is unmapped memory, and **`sbrk` does not check for it**: the `_sbrk` in the firmware is libnosys's, which (read from the disassembly of the
ELF) only adds the requested size to a pointer that starts at `end` and returns the old value, with no limit at all. Everything created with `new` lives there: the effect objects, the CloudSeed reverb engine's many small objects, the STFT bookkeeping arrays.
**Free heap = 256 KB - the static data in RAM_D2.** Which means that static buffers moved into RAM_D2 for speed are paid for out of the heap.

Check it with: `arm-none-eabi-nm build/guitarpedal.elf | grep ' end$'` (heap start) and `arm-none-eabi-readelf -S build/guitarpedal.elf` (the `.sram_d2_bss` and `.heap` sections).

## 2. What each effect needs

### Static memory (build report and the source; these are exact)

| Effect | DTCM | RAM_D2 (`.sram_d2_bss`) | SDRAM | Notes |
|---|---|---|---|---|
| Venus | about 72 KB | - | 147 KB | DTCM: two 16 KB frequency-domain work buffers, the FFT's two 16 KB workspaces, the window table. SDRAM: the nine overlapping input frames (9 x 4096 floats). Was 441 KB / 0 in the first version (see 4.1) |
| Saturn | small | 124.7 KB | 2.9 MB | RAM_D2: frames 73.7 KB (2 channels x 9 x 1024 floats), work buffers 16 KB, FFT workspace 8 KB, window tables 12 KB, delay line bookkeeping 14 KB. SDRAM: 4 x 175 delay lines x 752 floats (2.1 MB) and two pre-delay lines (0.77 MB) |
| Jupiter | small | - | 4.9 MB (the CloudSeed pool) | One CloudSeed engine: 5,148,080 bytes of the pool, the same for every preset and line count (measured on the host) |
| Neptune | small | - | 4.9 MB (the pool) + 4.6 MB | The delay lines: 2 x 4 s octave lines (1.5 MB) and 2 x 8 s reverse lines (3.1 MB) |
| Uranus | about 13 KB (the 12 FM voices) | 96 KB | - | RAM_D2: the half second of audio the grains are cut from (24000 floats) |

The CloudSeed pool (`Effect-Modules/cloudseed_pool.cpp`) is 19.7 MB (48 x 640 x 640) so that three engines fit (CloudSeed, Jupiter, Neptune); it is only linked when one of them is enabled.

Whole builds (build report):

| Build | DTCM | RAM_D2 | SRAM (code) | SDRAM |
|---|---|---|---|---|
| Uranus only (GCC 10) | 25.2% (33.0 KB) | 36.6% (96.0 KB) | 39.3% | 0 |
| All five effects (GCC 10) | 81.8% (107.2 KB) | 84.2% (220.7 KB) | 63.9% | 44.4% (29.8 MB) |
| All five effects (GCC 15, clean build) | 82.0% (107.5 KB) | 84.2% (220.7 KB) | 66.3% | 44.4% (29.8 MB) |

### Heap (what `new` allocates), measured on the host with `tests/host/run_heap_report.sh`

| Effect | Requested with `new` | Allocations | Heap chunks incl. newlib overhead |
|---|---|---|---|
| Venus | 17.3 KB | 9 | 17.3 KB |
| Jupiter | 129.8 KB | 4,416 | about 170 KB |
| Saturn | 1.8 KB | 15 | 1.8 KB |
| Neptune | 129.8 KB | 4,416 | about 170 KB |
| Uranus | 13.0 KB | 3 | 13.0 KB |

The two CloudSeed-based effects dominate: each engine is made of 4,400 small objects (a third of the bytes are 32-byte allocations, 20% are 512-byte ones). The host has 8-byte pointers and the Daisy 4-byte ones, so
the Daisy's figure is lower, by an unknown amount. **Inferred** from the fact that Venus + Jupiter + Saturn booted (Saturn leaves 131 KB of heap, Venus and Saturn take 19 KB, framework objects some more): one
engine is at most about 112 KB on the Daisy. The actual figure has not been measured; adding a heap readout to Show Debug would settle it (section 7).

## 3. The lockup with all five effects (a hypothesis)

All five enabled together **did not boot** on the pedal (measured on the pedal). Venus alone, Venus + Jupiter, Venus + Jupiter + Saturn, and Uranus alone did boot and run.
The evidence points at the heap:

* With all five, RAM_D2 holds 220,704 bytes of static data, so the heap starts at `0x3003de20` and has **41 KB** before the end of RAM_D2.
* Two CloudSeed engines alone ask for more than 200 KB (host figures, perhaps 150 to 220 KB on the Daisy), and Venus and Uranus add 30 KB. The combination that booted (Venus + Jupiter + Saturn) fits in the 131 KB
  left by Saturn's 124.7 KB. Adding Neptune (another engine) and Uranus (96 KB more static data) cannot fit.
* `sbrk` has no limit (verified in the disassembly), so the heap grows into unmapped memory: a bus fault or a stuck startup, which looks like a lockup. (That the fault happens at that point is the part that is not observed.)
* The CloudSeed author's old comments ("freezes", "if it freezes, check here first", the pool sizes that worked and froze) point the same way: the engine is close to the limit even alone.

Not confirmed on hardware. The matrix in section 8 is built on this model.

## 4. Performance: what was done, and what it gave

**The lessons first**, because they repeated:

1. **The CPU readout's average hides overruns.** The pedal's main readout is a smoothed average. What matters for dropouts is the slowest single block: one block that takes longer than the 1 ms it lasts. Show Debug now
   shows `average/peak%` (the peak is the slowest block of the last two seconds). In every case below the average was fine and the peak was the problem.
2. **SDRAM is far slower than it looks.** A delay line object that keeps its position, delay and fractional part in the same object as its samples (DaisySP's `DelayLine`) needs about ten extra slow accesses per update when the
   object is in SDRAM. Keep the bookkeeping on chip and only the samples in SDRAM. In-place FFT passes over SDRAM are slow for the same reason: copy to fast RAM, transform there, copy back.
3. **Spread heavy work over blocks by sample count, not by block count.** Then the same code works with any block size.
4. **Measure, do not guess.** Three times a reasonable cost estimate was wrong (a faster FFT changed nothing; fewer bins changed nothing). A timing readout per stage found the real cause each time.

### 4.1 Venus (spectral reverb, 4096-point STFT)

Measured on the pedal (average / peak of the CPU readout, block size, sample rate):

| Step | Result |
|---|---|
| First port at 48 kHz / 48-sample blocks, FFT passes in SDRAM | constant overruns, over 80% average |
| Same code at 32 kHz / 256-sample blocks (the original's settings) | 22% average |
| Rescheduled by sample count (any block size), work buffers shared and moved on chip, CMSIS FFT at 48 kHz / 256 | no change in the average |
| CMSIS-DSP FFT at 48 kHz / 48-sample blocks, FFT still working in SDRAM | 33% average, **225% peak**, constant overruns |
| **The FFT copies the frame to a DTCM workspace, transforms there, copies back** | **76% peak** at 48 kHz / 48 samples, no dropouts |

* The cause was the FFT passes running in place on frames in SDRAM. Swapping ShyFFT for the CMSIS FFT alone changed nothing (the cost was the memory, not the arithmetic).
* The stages (forward FFT, 16 chunks of bins, inverse FFT) are issued at 1.5x the rate they need, by sample count, so a frame is always finished before its output is due.
* The frequency-domain work buffers (`middle`, `out`) are shared by all frames (only one is processed at a time): SDRAM use fell from 441 KB to 147 KB.
* The CMSIS tables are restricted with `ARM_DSP_CONFIG_TABLES` and the `ARM_TABLE_*` defines in the Makefile to the sizes that are used: all FFT tables would add about 122 KB of code, the used ones 42 KB.

### 4.2 Saturn (spectral delay, 1024-point STFT, stereo)

Measured on the pedal, 48 kHz / 48 samples, peak of the CPU readout:

| Step | Peak |
|---|---|
| First version (4 chunks per frame, interpolated window lookups) | 157% |
| 16 chunks per frame and a window table | 133% |
| Bins per channel 175 -> 128 | **no change** (133%) |
| **`SdramDelay`: the delay lines' bookkeeping on chip, samples in SDRAM** | **72%** |
| Back to 175 bins | not read off separately; the author kept it |

The Show Debug timing lines (`stg` all stages in one block, `one` the slowest single stage, `smp` the per-sample loop, `ctl` control update, each in % of a block) went from `stg120 one27 smp12 ctl0` to
`stg66 one26 smp11 ctl0`. The delay line update had taken about 2 microseconds, one chunk of 32 bins in both channels a quarter of an audio block. `SdramDelay` is DaisySP's `DelayLine` split in two
(bit-identical output, checked over 300,000 samples).

Smaller changes: the window table (a lookup instead of an interpolated one) saves a constant amount per sample; the controls are turned into delay settings at most every 48 samples; the frames and work
buffers are in RAM_D2, not SDRAM.

### 4.3 The rest (not measured on the pedal)

* **Jupiter and Neptune:** control changes are flagged and one is pushed to the CloudSeed engine per 48 samples (some engine parameters recompute filters and delay lengths), instead of the engine being hit by a burst of
  knob movement in one block. The reverse delay line only computes its two `sinf` per sample in reverse mode (the original computes them in every mode). Neptune's delay lines use the same on-chip bookkeeping as Saturn's.
  The CloudSeed engine itself runs at an internal block size of 4 (found by the earlier block-processing work: larger blocks overflow the cache).
* **Uranus:** the half second of audio is in RAM_D2, since the grain players read it at random places (the Granular Delay module keeps its in SDRAM and notes it can only run one grain player; Uranus runs two).
  Controls are read once per block instead of for every sample.
* **Level of effort:** all of the above are covered by host tests (block path equals per-sample path, bit for bit where the algorithm allows); none of the CPU figures for these were read off the pedal.

## 5. The compiler

* The Makefile requires **GCC 10.x** (`GCC_VERSION_MAJOR_REQUIRED := 10`, "issues have been experienced with the latest version"). Earlier notes record that GCC 13 also worked once a libDaisy LED
  initialisation bug was worked around (see `HANDOFF_block_processing.md`). All the performance work above was done with GCC 10.2.1.
* In October 2026 the author's toolchain became **Arm GNU Toolchain 15.3.1**. Findings (the first two are traps worth knowing):
  * **The version guard also stops `make clean`**, which then fails silently if you redirect its output. A `make clean && make` with a new compiler therefore did *not* clean, and only the changed sources were
    recompiled: the result was a firmware with a mix of GCC 15 and GCC 10 objects. Use `make clean GCC_VERSION_MAJOR_REQUIRED=15` (and `make GCC_VERSION_MAJOR_REQUIRED=15`; a command-line value overrides the file's), or edit the guard.
  * **The prebuilt libraries are not rebuilt by the project's `make`:** `dependencies/libDaisy/build/libdaisy.a`, `dependencies/DaisySP/build/libdaisysp.a`, `dependencies/DaisySP/DaisySP-LGPL/build/libdaisysp-lgpl.a` and
    `dependencies/CloudSeed/build/libcloudseed.a` stay as they were built (GCC 10) until each is rebuilt in its own directory (`make clean && make`). The compiler tag of a binary shows what is in it:
    `arm-none-eabi-readelf -p .comment build/guitarpedal.elf` (a mixed build lists both versions).
  * With everything rebuilt with 15.3.1 (libraries and firmware), the build succeeded with no new `-Werror` failures in DaisySP and CloudSeed and 58 warnings in the project (not examined one by one).
  * **Effect on size:** code (SRAM) grew from 63.9% to 66.3% (about 11 KB) in the all-effects build; DTCM 81.8% to 82.0% (about 0.3 KB); RAM_D2 and SDRAM are unchanged (they are static data laid out by our own sections).
  * **Effect on the lockup: none expected, and not tested on hardware.** The heap model in section 3 does not depend on the compiler (the heap start is the same `0x3003de20`). The newlib in the 15.3 toolchain could change the
    allocator's per-allocation overhead slightly, but not by the 100 KB or more that is missing.
  * **Effect on speed:** not measured. The host tests are unaffected (they use the PC's compiler).

## 6. Where things stand

| Build | Result (measured on the pedal) |
|---|---|
| Uranus only | boots and runs (audio; the MIDI input is untested) |
| Venus + Jupiter + Saturn | booted and ran (CPU figures in 4.1 and 4.2 are from builds with these) |
| Venus + Jupiter + Saturn + Neptune + Uranus | does not boot |

The Makefile and `loaded_effects.h` enable only Uranus at the time of writing; the other four are commented out and `cloudseed_pool.cpp` with them.

## 7. Tools

* **Show Debug** (Global Settings): `average/peak%` CPU in the bottom middle, input and output level in the corners; `BaseEffectModule::GetDebugText()` lets an effect add two lines of its own (Saturn shows its stage timings).
* **`tests/host/run_heap_report.sh`:** the heap table in section 2 (needs the host toolchain only; counts `new` during each effect's constructor and `Init()`).
* **Build report and ELF:** the linker's "Memory region" table; `arm-none-eabi-nm build/guitarpedal.elf | grep ' end$'`; `arm-none-eabi-readelf -S` and `-p .comment`.
* **Host tests** (`tests/host/README.md`): `run_tests.sh` (block path equals per-sample path, signal paths, controls), `run_level_tests.sh` (default levels), `run_defaults_audit.sh` (defaults applied at startup).
* **Not built yet:** a heap readout on the pedal (free heap = `0x30048000` minus `sbrk(0)`, shown in Show Debug), which would turn the estimates below into measurements, and a start-up guard that refuses to start an
  effect whose heap would not fit instead of running into unmapped memory.

## 8. The matrix of effects that can be enabled together (first draft)

Constraint used: **RAM_D2 static data + heap <= 256 KB** (the heap is what limited the all-effects build; DTCM, code and SDRAM all have room for every combination: all five are 82% DTCM, 66% SRAM, 44% SDRAM).
Per effect: static RAM_D2 (Saturn 124.7 KB, Uranus 96.0 KB, the others 0) plus heap (Venus 17.3 KB, Saturn 1.8 KB, Uranus 13.0 KB, and **110 KB per CloudSeed engine as a planning figure**, Jupiter and Neptune; see
section 2 for why this is inferred). A few KB of framework heap is not counted. The three bold rows are observations on the pedal; the rest are arithmetic and **unverified**, and the engine figure is the weak point:
a result within about 20 KB of 256 KB should be tried, not trusted.

| Combination | RAM_D2 demand | Verdict |
|---|---|---|
| Venus | 17 KB | fits (estimate) |
| Jupiter | 107 KB | fits (estimate) |
| Saturn | 124 KB | fits (estimate) |
| Neptune | 107 KB | fits (estimate) |
| Uranus | 106 KB | boots (observed) |
| Venus + Jupiter | 124 KB | fits (estimate) |
| Venus + Saturn | 140 KB | fits (estimate) |
| Venus + Neptune | 124 KB | fits (estimate) |
| Venus + Uranus | 123 KB | fits (estimate) |
| Jupiter + Saturn | 231 KB | fits (estimate) |
| Jupiter + Neptune | 215 KB | fits (estimate) |
| Jupiter + Uranus | 214 KB | fits (estimate) |
| Saturn + Neptune | 231 KB | fits (estimate) |
| Saturn + Uranus | 230 KB | fits (estimate) |
| Neptune + Uranus | 214 KB | fits (estimate) |
| Venus + Jupiter + Saturn | 248 KB | boots (observed) |
| Venus + Jupiter + Neptune | 232 KB | fits (estimate) |
| Venus + Jupiter + Uranus | 231 KB | fits (estimate) |
| Venus + Saturn + Neptune | 248 KB | fits (estimate) |
| Venus + Saturn + Uranus | 247 KB | fits (estimate) |
| Venus + Neptune + Uranus | 231 KB | fits (estimate) |
| Jupiter + Saturn + Neptune | 338 KB | does not fit (estimate) |
| Jupiter + Saturn + Uranus | 337 KB | does not fit (estimate) |
| Jupiter + Neptune + Uranus | 321 KB | does not fit (estimate) |
| Saturn + Neptune + Uranus | 337 KB | does not fit (estimate) |
| Venus + Jupiter + Saturn + Neptune | 355 KB | does not fit (estimate) |
| Venus + Jupiter + Saturn + Uranus | 354 KB | does not fit (estimate) |
| Venus + Jupiter + Neptune + Uranus | 338 KB | does not fit (estimate) |
| Venus + Saturn + Neptune + Uranus | 354 KB | does not fit (estimate) |
| Jupiter + Saturn + Neptune + Uranus | 445 KB | does not fit (estimate) |
| Venus + Jupiter + Saturn + Neptune + Uranus | 462 KB | does not boot (observed) |

To turn this into a real matrix: add the heap readout (section 7), then for each combination that the estimate calls fits or borderline, flash it and record the free heap, the boot result and the Show Debug peak.
Ideas for making more combinations fit, from least to most work:

1. **Share one CloudSeed engine** between the CloudSeed, Jupiter and Neptune modules (only one is active at a time): saves about 110 to 130 KB of heap per extra reverb effect and 4.9 MB of SDRAM. Needs an "effect became
   active" hook so the engine is re-initialised with the active effect's settings.
2. **Move the heap** to the free part of AXI SRAM (about 150 KB) or into SDRAM with its own `_sbrk`, so the static buffers in RAM_D2 stop competing with it. SDRAM-backed heap objects would slow the CloudSeed engine (its small
   objects are accessed per sample), so AXI SRAM is the better target; needs a linker fragment (compare `nam_a2_sections.lds`).
3. **Make the CloudSeed engine allocate less:** its 4,400 small allocations cost newlib's header and rounding overhead on every one (about 40 KB per engine on the host). Fewer, larger allocations would help.
4. **Shrink the static buffers:** Uranus's half second (96 KB) could be a quarter second; Saturn's frames could be shared between the two channels if their stages were run in lockstep (saves about 18 KB).
