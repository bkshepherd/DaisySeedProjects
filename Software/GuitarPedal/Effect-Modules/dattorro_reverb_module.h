// SPDX-License-Identifier: GPL-3.0-or-later
//
// This module wraps the GPLv3-licensed Dattorro plate reverb DSP vendored
// under Effect-Modules/Dattorro/ (see that directory's README.md for the
// license and provenance chain). This file itself is new code written for
// GuitarPedal, but since it links directly against GPLv3 code it is likewise
// distributed under the GNU General Public License v3 (or later). It is only
// compiled into the firmware when explicitly enabled - see loaded_effects.h
// and the Makefile for how to opt in, and what that means for your build's
// license.

#pragma once
#ifndef DATTORRO_REVERB_MODULE_H
#define DATTORRO_REVERB_MODULE_H

#include "Dattorro/Dattorro.hpp"
#include "base_effect_module.h"
#include <memory>
#include <stdint.h>
#ifdef __cplusplus

/** @file dattorro_reverb_module.h */

namespace bkshepherd {

// A Dattorro (1997) plate reverb, following the voicing shared by the Hothouse
// "Flick" and "MuleBox" pedals and the bkshepherd "AmpSim" module (all of which
// build on Valley Audio's Plateau implementation). Six parameters are exposed
// to match Flick's reverb edit mode; a seventh (Size, i.e. Dattorro's internal
// Time Scale) is menu-only since the 125B only has six knobs.
//
// Holding the alternate footswitch for 5 seconds resets every parameter except
// Mix back to its factory default - the reverb's parameter space includes
// combinations that are difficult to dial back from by ear alone.
class DattorroReverbModule : public BaseEffectModule {
  public:
    enum Param {
        MIX = 0,
        PRE_DELAY,
        DECAY,
        TONE,
        MOD,
        DIFFUSE,
        SIZE,
        PARAM_COUNT
    };

    DattorroReverbModule();
    ~DattorroReverbModule();

    void Init(float sample_rate) override;

    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;

    // This effect has no use for tap tempo, freeing the alternate footswitch
    // for the factory-reset hold gesture below.
    bool AlternateFootswitchForTempo() const override { return false; }
    void AlternateFootswitchPressed() override;
    void AlternateFootswitchReleased() override;

    void SetEnabled(bool isEnabled) override;

    void UpdateUI(float elapsedTime) override;
    void DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                bool isEditing) override;

  protected:
    void ParameterChanged(int parameter_id) override;

  private:
    // Pushes every current Parameter value into the Dattorro engine. Used once
    // after construction (InitParams() writes defaults directly into storage
    // without going through ParameterChanged()) and again after a factory
    // reset resolves through SetParameterToDefault().
    void SyncAllParametersToEngine();

    // Restores every Parameter except Mix to its factory default.
    void ResetNonMixParametersToDefaults();

    std::unique_ptr<Dattorro> m_dattorro;

    // True if the SDRAM arena couldn't satisfy every InterpDelay allocation
    // the Dattorro engine needed. Rather than risk falling back to the heap
    // (which is small and shared with everything else in the firmware), the
    // module just passes audio through dry in this case.
    bool m_arenaExhausted = false;

    // 5-second alternate-footswitch-hold factory reset gesture.
    bool m_alternateFootswitchHeld = false;
    float m_alternateFootswitchHeldSeconds = 0.0f;
    bool m_factoryResetTriggeredThisHold = false;
    float m_factoryResetFlashSecondsRemaining = 0.0f;
};
} // namespace bkshepherd
#endif
#endif
