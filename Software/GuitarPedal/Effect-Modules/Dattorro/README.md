# Dattorro plate reverb (vendored, GPLv3)

This directory contains a Dattorro (1997) plate reverb DSP core. **It is licensed under the GNU
General Public License v3 (or later), not the MIT license the rest of this repository uses** — see
[`LICENSE-GPLv3.txt`](LICENSE-GPLv3.txt). It is not compiled into GuitarPedal by default; see
"Opting in" below for what that means for your build.

## Provenance

1. **Valley Audio Soft / Dale Johnson**, *Plateau* (`PlateauNEVersio`), Copyright (C) 2020 —
   the original implementation of Jon Dattorro's 1997 plate reverb algorithm, GPL-3.0-or-later.
2. **Hothouse "Flick"** — ported Plateau's DSP core for the Cleveland Music Co. Hothouse
   platform, running it at full rate (96kHz codec, 48kHz reverb tick) with a six-knob "reverb
   edit mode" (mix, pre-delay, decay, tone, modulation, diffusion).
3. **"MuleBox"** (Hothouse, 48kHz) — same DSP core, mix-only, but the reference for correct 48kHz
   input-section scaling (`dattorroScaleFactor`), since Flick runs its codec at 96kHz.
4. **bkshepherd/DaisySeedProjects "AmpSim"** — ported the same core to the 125B hardware this
   repository targets, running it at half rate (24kHz) to fit the CPU budget of an amp
   simulator that also has other work to do each block. AmpSim's copy is the one vendored here,
   because it independently fixed several sample-rate-handling bugs present in the original
   Plateau/Flick/MuleBox source (see "Fixes carried from AmpSim" below) — GuitarPedal runs this
   reverb as its own dedicated effect at full rate, so it inherits those fixes rather than
   re-discovering them.
5. **GuitarPedal `DattorroReverbModule`** (`Effect-Modules/dattorro_reverb_module.{h,cpp}`, one
   directory up) — wraps this DSP core as a normal loadable `BaseEffectModule`, exposing the same
   six knobs as Flick's edit mode, plus a menu-only seventh ("Size" / Dattorro's internal Time
   Scale) since the 125B hardware has exactly six knobs and all are already mapped.

Every vendored file below carries the original Valley Audio copyright header and an
`SPDX-License-Identifier: GPL-3.0-or-later` line, restoring what was dropped somewhere in the
Flick → AmpSim transit, plus a note on what (if anything) changed in this copy.

| File | Origin | Notes |
|---|---|---|
| `Dattorro.hpp` / `Dattorro.cpp` | AmpSim (fixed) | see "Fixes carried from AmpSim" |
| `dsp/delays/AllpassFilter.hpp` | Flick (unmodified upstream of AmpSim too) | style-normalized only |
| `dsp/delays/InterpDelay.hpp` | AmpSim, **allocation scheme rewritten** | see "Arena allocation" |
| `dsp/filters/OnePoleFilters.hpp` | Flick (unmodified upstream of AmpSim too) | style-normalized only |
| `dsp/modulation/LFO.hpp` | Flick (unmodified upstream of AmpSim too) | style-normalized only |
| `LICENSE-GPLv3.txt` | Plateau (PlateauNEVersio) | verbatim |

## Fixes carried from AmpSim

Relative to the original Plateau/Flick/MuleBox source, this copy (via AmpSim) fixes:

- **Tank sample-rate clamp** — `Dattorro1997Tank::maxSampleRate` is now actually wired to the
  constructor argument (`initMaxSampleRate`) instead of being left at its 32kHz default and
  silently clamping every higher rate down to it.
- **Missing `delaysInitialised` guard** — `setSampleRate()` no longer reallocates all twelve
  internal delay lines and allpass filters on every call, only once.
- **Tank LFO sample rate** — the four tank modulation LFOs (`lfo1`..`lfo4`) are now actually told
  the current sample rate (`lfoN.setSamplerate(sampleRate)`), instead of running at whatever rate
  they were constructed with (which was silently wrong at every rate but 32kHz).
- **Output tap rounding** — `rescaleTapTimes()` now rounds (`lroundf`) instead of truncating when
  rescaling the tank's output tap positions for the current sample rate, removing a small
  systematic bias.
- **Pre-delay buffer sizing** — `preDelay` is sized for 200ms at 48kHz (`InterpDelay(9600, ...)`)
  rather than whatever smaller buffer the original assumed.

None of the parameter defaults needed to change to adopt these fixes: AmpSim's own preset
(`ApplyPreset()` in its `reverb_processor.h`) reuses Flick/MuleBox's exact tuned values (decay
0.8, tank diffusion 0.85, time scale 1.0075, and the same tone/mod pitches) against its own
already-fixed tank, which is the evidence this module's defaults were chosen against too.

The tank's *nominal* internal design sample rate (`dattorroSampleRate = 29761.0`, giving a
`sampleRateScale` of ~1.075 at 32kHz and above) is intentionally left alone — it's the shared
voicing all of Flick, MuleBox and AmpSim were tuned against, and re-deriving it from the Dattorro
paper's nominal values would mean re-voicing decay/diffusion/tone from scratch with no reference
to check against.

## Arena allocation

The original `InterpDelay` (Flick, MuleBox) allocates a fixed
`float DSY_SDRAM_BSS sdramData[50][144000]` — 28.8 MB — sized for the largest configuration either
pedal ever instantiates. GuitarPedal's SDRAM is already ~70% committed by the time
`load_effects()` finishes instantiating every other module's own SDRAM buffers, so that doesn't
fit.

`InterpDelay` here instead draws from an `InterpDelayArena`: a static bump allocator over an
external buffer the caller installs with `InterpDelayArena::set(base, size)`. Each `InterpDelay`
constructed while the arena is armed carves its buffer out of it; if none is armed (or it's
exhausted), it falls back to an owned `std::vector<float>` on the heap, which keeps the class
usable in desktop/unit tests without any SDRAM to hand.

`DattorroReverbModule::Init()` arms a 1 MiB arena (declared as a `DSY_SDRAM_BSS` global directly in
`dattorro_reverb_module.cpp`, matching this repo's per-module-owns-its-SDRAM-global convention)
immediately before constructing the `Dattorro` engine, and releases it immediately after. A full
48kHz `Dattorro(48000.0f, 16.0f, 4.0f)` needs roughly 870 KB across its 12 delay lines, leaving
about 15% headroom in the 1 MiB budget. If the arena is ever exhausted, the module does not halt
(there's no halt-loop precedent in this codebase) — it sets a flag and passes audio through dry
instead of risking an unbounded heap fallback on an embedded target.

## Opting in

This code is not built by default. Enabling it means your firmware links GPLv3 code and the
resulting binary must be distributed under GPLv3 (or not distributed at all) — see the top-level
[`README.md`](../../README.md) for the exact lines to uncomment in the `Makefile` and
`loaded_effects.h`, and for what this means for your build.
