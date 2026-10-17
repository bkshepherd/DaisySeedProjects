# Performance and memory notes

Written for anyone who picks this up later: what was learned while converting the FunBox effects (Venus, Jupiter, Saturn, Neptune, Uranus, and later Balnea, Pluto, Cassini, Mars, Mercury) about the CPU and memory limits of the pedal, which
optimisations made a difference and by how much, what each effect needs in memory, how the compiler change affected things, and a first draft of a matrix of which effects can be enabled together.
The history of the work it describes is in `CHANGELOG.md`.

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
| Balnea | about 3.5 KB (the module) | - | 384 KB | Two delay lines of 16384 floats and two reverse lines of 32768 floats, read and written sequentially |
| Pluto | small | - | 23 MB | Two loopers of a minute at 48 kHz (11.5 MB each) and a `ReverbSc` (about 0.4 MB); the loops are not cleared at startup |
| Cassini | small | - | 23 MB | Three 20 s sample banks (11.5 MB), the looper's minute (11.5 MB) and a `ReverbSc`; not cleared at startup |
| Mars | about 9 KB (the module, with the FIR state) | - | 0.7 MB | The delay buffer for 2 s plus the second tap; the network and the filters are small |
| Mercury | 33 KB (the WaveNet history, 4112 slots of two channels) | - | - | 9 models x 454 floats of weights are in the program (SRAM), 16 KB |

The CloudSeed pool (`Effect-Modules/cloudseed_pool.cpp`) is 19.7 MB (48 x 640 x 640) so that three engines fit (CloudSeed, Jupiter, Neptune); it is only linked when one of them is enabled.

Whole builds (build report):

| Build | DTCM | RAM_D2 | SRAM (code) | SDRAM |
|---|---|---|---|---|
| Uranus only (GCC 10) | 25.2% (33.0 KB) | 36.6% (96.0 KB) | 39.3% | 0 |
| All five effects (GCC 10) | 81.8% (107.2 KB) | 84.2% (220.7 KB) | 63.9% | 44.4% (29.8 MB) |
| All five effects (GCC 15, clean build) | 82.0% (107.5 KB) | 84.2% (220.7 KB) | 66.3% | 44.4% (29.8 MB) |
| Uranus + Balnea, Pluto, Cassini, Mars, Mercury (GCC 13; Venus, Jupiter, Saturn, Neptune disabled) | 52.3% (68.6 KB) | 36.6% (96.0 KB) | 55.2% | 71.4% (47.9 MB) |

### Heap (what `new` allocates), measured on the host with `tests/host/run_heap_report.sh`

| Effect | Requested with `new` | Allocations | Heap chunks incl. newlib overhead |
|---|---|---|---|
| Venus | 17.3 KB | 9 | 17.3 KB |
| Jupiter | 129.8 KB | 4,416 | about 170 KB |
| Saturn | 1.8 KB | 15 | 1.8 KB |
| Neptune | 129.8 KB | 4,416 | about 170 KB |
| Uranus | 13.0 KB | 3 | 13.0 KB |
| Balnea | 3.6 KB | 3 | 3.5 KB |
| Pluto | 0.6 KB | 3 | 0.5 KB |
| Cassini | 1.2 KB | 3 | 1.2 KB |
| Mars | 9.1 KB | 3 | 9.0 KB |
| Mercury | 0.3 KB | 3 | 0.3 KB |

**Check on the pedal:** the host count for the CloudSeed engine (Jupiter, Neptune) is far too high: Show Debug on the pedal went from 45 KB of heap used (Uranus + Balnea + Pluto + Cassini + Mars + Mercury) to 54 KB with Jupiter added, so one engine takes about **9 KB** of heap; its delay lines are in the SDRAM pool and the host count included memory that
was freed again (vectors growing). The framework itself (menus, storage) took about 18 KB (45 KB used against 26 KB estimated for the six effects). `effects-memory.json` has these figures (Neptune's was found by difference from the next flash: 60 KB used with Jupiter and Neptune but without Cassini; the CloudSeed module's is inferred from Jupiter's).

The two CloudSeed-based effects dominate: each engine is made of 4,400 small objects (a third of the bytes are 32-byte allocations, 20% are 512-byte ones). The host has 8-byte pointers and the Daisy 4-byte ones, so
the Daisy's figure is lower, by an unknown amount. **Inferred** from the fact that Venus + Jupiter + Saturn booted (Saturn leaves 131 KB of heap, Venus and Saturn take 19 KB, framework objects some more): one
engine is at most about 112 KB on the Daisy. The actual figure has not been measured; adding a heap readout to Show Debug would settle it (section 7).

## 3. The lockup with all five effects (a hypothesis)

All five enabled together **did not boot** on the pedal (measured on the pedal). Venus alone, Venus + Jupiter, Venus + Jupiter + Saturn, and Uranus alone did boot and run.
The evidence points at the heap:

* With all five, RAM_D2 holds 220,704 bytes of static data, so the heap starts at `0x3003de20` and has **41 KB** before the end of RAM_D2.
* What the five need from the heap, with the figures measured on the pedal since (framework 18 KB, Jupiter 9 KB, Neptune 7 KB) and the host figures for the rest: 18 + Venus 17 + Jupiter 9 + Saturn 2 + Neptune 9 + Uranus 13 = **about 67 KB, against 41 KB**. (The first version of this explanation assumed 110 KB per CloudSeed engine, which turned out to be wrong; the conclusion
  holds with the right figure.) Venus + Jupiter + Saturn, which booted, needs about 46 KB and has 134 KB (Saturn's 124.7 KB of static data leaves that much).
* `sbrk` had no limit (verified in the disassembly), so the heap grew into unmapped memory: a bus fault or a stuck startup, which looks like a lockup. (That the fault happened at that point was not observed.) The heap guard (section 7) now stops with a message instead.

Still not confirmed by running the five again with the guard (it would now show OUT OF MEMORY). The estimates in section 8 are built on this model, and it agrees with the observations so far (the six-effect FunBox set boots with 108 to 116 KB of heap free).

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

* The Makefile used to refuse any compiler but **GCC 10.x** (a version guard, "issues have been experienced with the latest version"). **That guard has been removed**: it was no longer justified (GCC 13 and then 15 build
  and link fine; GCC 13 needed a libDaisy LED initialisation workaround in `BaseHardwareModule::InitLeds`). All the performance work above was done with GCC 10.2.1.
* In October 2026 the author's toolchain became **Arm GNU Toolchain 15.3.1**. Findings (the first two are traps worth knowing; the first one is gone with the guard, the lesson is that **a compiler change never triggers a rebuild**):
  * **With the guard in place it also stopped `make clean`**, which then failed silently if its output was redirected. A `make clean && make` with a new compiler therefore did *not* clean, and only the changed sources were
    recompiled: the result was a firmware with a mix of GCC 15 and GCC 10 objects. After changing compiler always run a real `make clean` (the Makefile does not notice a compiler change by itself) and rebuild the libraries below.
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
| Uranus + Balnea + Pluto + Cassini + Mars + Mercury | **not tried yet** (build report above: DTCM 52%, RAM_D2 37%, SDRAM 71%, about 15 KB of extra heap) |

The Makefile and `loaded_effects.h` enable Uranus and the five newest effects at the time of writing; Venus, Jupiter, Saturn and Neptune are commented out and `cloudseed_pool.cpp` with them.

## 7. Tools

* **Show Debug** (Global Settings): `average/peak%` CPU in the bottom middle, input and output level in the corners; `BaseEffectModule::GetDebugText()` lets an effect add two lines of its own (Saturn shows its stage timings).
* **`tests/host/run_heap_report.sh`:** the heap table in section 2 (needs the host toolchain only; counts `new` during each effect's constructor and `Init()`).
* **Build report and ELF:** the linker's "Memory region" table; `arm-none-eabi-nm build/guitarpedal.elf | grep ' end$'`; `arm-none-eabi-readelf -S` and `-p .comment`.
* **Host tests** (`tests/host/README.md`): `run_tests.sh` (block path equals per-sample path, signal paths, controls), `run_level_tests.sh` (default levels), `run_defaults_audit.sh` (defaults applied at startup).
* **Effect selection tool (`effects_config.sh`):** which effects are built, the generated files, the defines and stale objects, and the memory budget commands are described in [GENERATE_FIRMWARE.md](GENERATE_FIRMWARE.md).
* **Heap readout and guard (`Util/heap_guard.{h,cpp}`):** Show Debug prints `heap <used>K free <free>K` at the top of the effect screen: *used* is how far the heap has grown (it never shrinks, so it is also the most it has needed), *free* is what is left
  before the end of RAM_D2 (`_ram_d2_limit`, provided by `nam_a2_sections.lds`). The firmware's `_sbrk` (which replaces libnosys's unlimited one) stops at that limit: instead of running into unmapped memory it shows **OUT OF MEMORY**, what was being
  started and how much was wanted on the display, and halts. At startup, before each effect's `Init()`, the heap estimate from `effects-memory.json` (the tool generates `generated/effects_heap.inc`; a chain gets the sum of its effects) is compared with what is free: an effect that is estimated not to fit gets a
  **HEAP WARNING** screen for 2.5 s and is then started anyway (the estimates are rough; the guard is what decides, so that a combination can still be tried and measured). Only the PC-side parts are tested (`tests/host/test_heap_guard.cpp`); the limit itself needs the pedal.
  To turn the estimates into measurements: flash a selection, open Show Debug, write down *used* and *free*, and add the result to `observed` in `effects-memory.json` (see the matrix in section 8).

## 8. The matrix of effects that can be enabled together

**This is now generated by `effects_config.sh` (`./effects_config.sh matrix funbox-venus funbox-jupiter funbox-saturn funbox-neptune funbox-uranus` prints the table below; `report` does the same for the current selection)**, from the memory
figures in `effects-memory.json` (static sizes measured from the object files by `effects_config.sh measure` and `calibrate`; heap figures from section 2) and from what has been tried on the pedal (the `observed` list in the same file).
Constraint used: **RAM_D2 static data + heap <= 256 KB** (the heap is what limited the all-effects build), with DTCM (minus a 16 KB reserve for the stack), code space and SDRAM checked too. Verdicts: *fits*, *tight* (over 92% of a
region) and *does not fit*. The framework's own heap (18 KB, measured) is counted; the per-effect heap figures are host counts except Jupiter's (measured): a result near
the limit should be tried, not trusted. Observed results are in bold; with the generated table the estimate for the observed rows can be compared with what happened.

| Combination | RAM_D2 | SRAM | DTCM | SDRAM | Estimate | Observed on the pedal |
|---|---|---|---|---|---|---|
| funbox-venus | 34 KB | 235 KB | 101 KB | 0.1 MB | fits | **boots and runs** |
| funbox-jupiter | 27 KB | 220 KB | 29 KB | 9.2 MB | fits |  |
| funbox-saturn | 141 KB | 211 KB | 29 KB | 2.7 MB | fits |  |
| funbox-neptune | 25 KB | 221 KB | 29 KB | 13.6 MB | fits |  |
| funbox-uranus | 124 KB | 194 KB | 32 KB | 0.0 MB | fits | **boots and runs** |
| funbox-venus + funbox-jupiter | 43 KB | 269 KB | 101 KB | 9.3 MB | fits | **boots and runs** |
| funbox-venus + funbox-saturn | 158 KB | 254 KB | 101 KB | 2.9 MB | fits |  |
| funbox-venus + funbox-neptune | 42 KB | 270 KB | 101 KB | 13.7 MB | fits |  |
| funbox-venus + funbox-uranus | 141 KB | 243 KB | 104 KB | 0.1 MB | tight |  |
| funbox-jupiter + funbox-saturn | 150 KB | 246 KB | 29 KB | 11.9 MB | fits |  |
| funbox-jupiter + funbox-neptune | 34 KB | 248 KB | 29 KB | 18.8 MB | fits |  |
| funbox-jupiter + funbox-uranus | 133 KB | 228 KB | 32 KB | 9.2 MB | fits |  |
| funbox-saturn + funbox-neptune | 148 KB | 246 KB | 29 KB | 16.3 MB | fits |  |
| funbox-saturn + funbox-uranus | 248 KB | 219 KB | 32 KB | 2.7 MB | tight |  |
| funbox-neptune + funbox-uranus | 131 KB | 229 KB | 32 KB | 13.6 MB | fits |  |
| funbox-venus + funbox-jupiter + funbox-saturn | 167 KB | 288 KB | 101 KB | 12.0 MB | fits | **boots and runs** |
| funbox-venus + funbox-jupiter + funbox-neptune | 51 KB | 297 KB | 101 KB | 18.9 MB | fits |  |
| funbox-venus + funbox-jupiter + funbox-uranus | 150 KB | 277 KB | 104 KB | 9.3 MB | tight |  |
| funbox-venus + funbox-saturn + funbox-neptune | 165 KB | 289 KB | 101 KB | 16.4 MB | fits |  |
| funbox-venus + funbox-saturn + funbox-uranus | 264 KB | 262 KB | 104 KB | 2.9 MB | does not fit |  |
| funbox-venus + funbox-neptune + funbox-uranus | 148 KB | 277 KB | 104 KB | 13.7 MB | tight |  |
| funbox-jupiter + funbox-saturn + funbox-neptune | 157 KB | 273 KB | 29 KB | 21.5 MB | fits |  |
| funbox-jupiter + funbox-saturn + funbox-uranus | 257 KB | 253 KB | 32 KB | 11.9 MB | does not fit |  |
| funbox-jupiter + funbox-neptune + funbox-uranus | 140 KB | 255 KB | 32 KB | 18.8 MB | fits |  |
| funbox-saturn + funbox-neptune + funbox-uranus | 255 KB | 254 KB | 32 KB | 16.3 MB | tight |  |
| funbox-venus + funbox-jupiter + funbox-saturn + funbox-neptune | 174 KB | 316 KB | 101 KB | 21.7 MB | fits |  |
| funbox-venus + funbox-jupiter + funbox-saturn + funbox-uranus | 273 KB | 296 KB | 104 KB | 12.0 MB | does not fit |  |
| funbox-venus + funbox-jupiter + funbox-neptune + funbox-uranus | 157 KB | 304 KB | 104 KB | 18.9 MB | tight |  |
| funbox-venus + funbox-saturn + funbox-neptune + funbox-uranus | 272 KB | 297 KB | 104 KB | 16.4 MB | does not fit |  |
| funbox-jupiter + funbox-saturn + funbox-neptune + funbox-uranus | 264 KB | 281 KB | 32 KB | 21.5 MB | does not fit |  |
| funbox-venus + funbox-jupiter + funbox-saturn + funbox-neptune + funbox-uranus | 281 KB | 323 KB | 104 KB | 21.7 MB | does not fit | **does not boot** |

The five newest effects (Balnea, Pluto, Cassini, Mars, Mercury) add no RAM_D2 and about 15 KB of heap together (3.5, 0.5, 1.2, 9.0 and 0.3 KB), so they change none of the verdicts above by more than that: each row can have them added at a
cost of up to 15 KB of RAM_D2 demand (Mercury also needs 33 KB of DTCM: with Venus's 72 KB that is 105 KB of the 128 KB, which is too tight with the stack; Venus and Mercury together would need checking). **They do add to the SDRAM: 64 MB in all, of which the
two loopers take 23 MB each (Pluto, Cassini; the Looper module takes 23 MB too), Neptune 9.4 MB, Jupiter 4.9 MB, Saturn 2.9 MB, Mars 0.7 MB, Balnea 0.4 MB, Venus 0.15 MB, and the CloudSeed pool (when the CloudSeed module is in the build) 19.7 MB.** Pluto and Cassini together
take 47 MB, which leaves 17 MB: Saturn (2.9 MB), Venus, Balnea and Mars fit next to them. With the effect tool (`effects_config.sh`) the CloudSeed pool is only as big as the number of effects that use the engine (4.9 MB for one, 9.8 MB for two, 14.7 MB for three, plus the effects' own delay lines), so Jupiter (4.9 MB) fits next to
both loopers and Neptune (4.9 + 4.6 MB) just about does; the tool's report says what a selection needs.

To improve it: add the heap readout (section 7: it exists now, read it off the pedal), then for each combination that the estimate calls fits or tight, flash it and add the free heap, the boot result and the Show Debug peak to `observed` in `effects-memory.json`.
Ideas for making more combinations fit, from least to most work:

1. **Share one CloudSeed engine** between the CloudSeed, Jupiter and Neptune modules (only one is active at a time): saves 4.9 MB of SDRAM. Needs an "effect became
   active" hook so the engine is re-initialised with the active effect's settings.
2. **Move the heap** to the free part of AXI SRAM (about 150 KB) or into SDRAM with its own `_sbrk`, so the static buffers in RAM_D2 stop competing with it. SDRAM-backed heap objects would slow the CloudSeed engine (its small
   objects are accessed per sample), so AXI SRAM is the better target; needs a linker fragment (compare `nam_a2_sections.lds`).
3. (The engine's heap is only about 9 KB, so allocating less is no longer worth it.)
4. **Shrink the static buffers:** Uranus's half second (96 KB) could be a quarter second; Saturn's frames could be shared between the two channels if their stages were run in lockstep (saves about 18 KB).
