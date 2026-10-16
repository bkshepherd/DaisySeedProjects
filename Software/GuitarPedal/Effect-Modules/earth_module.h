// SPDX-License-Identifier: GPL-3.0-or-later
//
// Earth: a plate reverb with an octave effect, adapted from the Earth pedal of the FunBox project by GuitarML (https://github.com/GuitarML/Funbox,
// software/Earth, GPL-3.0-or-later, Copyright (c) 2024 Keith Bloemer). It links against the GPLv3 Dattorro engine vendored under Effect-Modules/Dattorro/
// and contains code adapted from GPLv3 sources, so it is distributed under the GNU General Public License v3 (or later). It is only compiled into the
// firmware when explicitly enabled together with the Dattorro reverb - see loaded_effects.h, the Makefile and Effect-Modules/Earth/README.md for what that
// means for your build's license.
#pragma once
#ifndef EARTH_MODULE_H
#define EARTH_MODULE_H

#include "../Util/Multirate.h"
#include "../Util/OctaveGenerator.h"
#include "Dattorro/Dattorro.hpp"
#include "base_effect_module.h"
#include "daisysp.h"
#include <memory>
#include <q/fx/biquad.hpp>
#include <q/support/literals.hpp>
#include <stdint.h>
#ifdef __cplusplus

/** @file earth_module.h */

using namespace daisysp;

namespace bkshepherd {

// Earth: Dattorro plate reverb in front of an optional octave effect (up, or up and down), with a momentary action on the secondary footswitch (freeze the
// reverb, swell an overdrive on it, or switch the octave on). Mono in, stereo out.
class EarthModule : public BaseEffectModule {
  public:
    float GetMaxTrailSeconds() const override { return 8.0f; }

    enum Param {
        PRE_DELAY = 0,
        MIX,
        DECAY,
        MOD_DEPTH,
        MOD_SPEED,
        FILTER,
        SIZE,
        OCTAVE,
        FS_ACTION,
        IN_DIFFUSION,
        OCT_ONLY,
        PARAM_COUNT
    };

    EarthModule();
    ~EarthModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;
    float GetBrightnessForLED(int led_id) const override;

    // The secondary footswitch performs the momentary action chosen by FS_ACTION while it is held
    void AlternateFootswitchPressed() override;
    void AlternateFootswitchReleased() override;
    bool AlternateFootswitchForTempo() const override { return false; }

  private:
    // noclone: ProcessMono calls this with n = 1, which would otherwise make GCC emit a second, specialised copy
    __attribute__((noclone)) void ProcessSamples(const float *in, float *outL, float *outR, size_t n);
    void ApplyEngineSettings();
    void UpdateSmoothedParameters(size_t n);

    std::unique_ptr<Dattorro> m_dattorro;
    bool m_arenaExhausted; // the SDRAM arena could not hold every delay line: pass the audio through dry rather than fall back to the heap

    // The octave effect (the same code as the PolyOctave module): the signal is decimated by resample_factor (6), shifted, interpolated back and shaped by
    // two shelves, in chunks of resample_factor samples. The sample rate must be divisible by the resample factor (48000 / 6 = 8000).
    static constexpr int kOctaveSampleRate = 48000;
    Decimator2 m_decimate;
    Interpolator m_interpolate;
    OctaveGenerator m_octave;
    cycfi::q::highshelf m_eq1;
    cycfi::q::lowshelf m_eq2;
    float m_buff[resample_factor];
    float m_buffOut[resample_factor];
    int m_binCounter;

    Overdrive m_overdrive[2];
    bool m_odOn;
    float m_odSwell;  // current overdrive drive, swells towards m_odTarget while the footswitch is held
    float m_odTarget;

    // Smoothed values, moved towards the controls every sample like the original and handed to the engine once per call
    float m_preDelay;
    float m_modDepth;
    float m_modSpeed;
    float m_decay;

    float m_sampleRate;
    float m_rateRatio; // 32 kHz relative to the sample rate (see earth_module.cpp)

    bool m_footswitchHeld;
    bool m_freeze;
    bool m_octaveMomentary;
};
} // namespace bkshepherd
#endif
#endif
