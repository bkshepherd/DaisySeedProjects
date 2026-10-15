// Edit the contents of this file to populate the available effects that you
// want to use

#ifndef LOADED_EFFECTS_H
#define LOADED_EFFECTS_H
#pragma once

#include "Effect-Modules/base_effect_module.h"
#include "Effect-Modules/effect_chain.h" // Some caution required - See README and examples below for usage

// The effects that are built into the firmware are chosen in effects.config (see tools/effects.py): this includes their headers
#include "generated/effects_includes.inc"

namespace bkshepherd {

// heapEstimates: for each effect in the list, what it is estimated to allocate with new (bytes, from effects-memory.json), for the check at startup
void load_effects(int &availableEffectsCount, BaseEffectModule **&availableEffects, const uint32_t *&heapEstimates) {
    // clang-format off
    static BaseEffectModule* effectList[] = {
        // The chosen effects (their order is the order in effects.json: it decides the menu order and the layout of the saved settings)
        #include "generated/effects_instances.inc"

        // The chains of chains.json (several effects combined into one entry, see README.md: Combining Effects), made with tools/effects.py
        #include "generated/effects_chains.inc"

        // Effects chain examples

        // Combines two or more effects into one, processed serially. Note that
        // many modules cannot be used more than once - see README.md.
        // In this case, the ReverbModule is single-instance only, so it cannot
        // be used standalone if it is used here.

        // new EffectChain(
        //     "TremVerb",
        //     // Slots: a short menu tag plus the child effect instance (owned by the chain).
        //     {{"Tr", new ModulatedTremoloModule()}, {"Rv", new ReverbModule()}},
        //     // Knob/MIDI CC mappings: {slot, child param id, knob (-1 = none), midi CC (-1 = none)}.
        //     {{0, ModulatedTremoloModule::DEPTH,    0, 20},
        //      {0, ModulatedTremoloModule::FREQ,     1, 21},
        //      {0, ModulatedTremoloModule::WAVE,     2, 22},

        //      {1, ReverbModule::TIME,               3, 23},
        //      {1, ReverbModule::DAMP,               4, 24},
        //      {1, ReverbModule::MIX,                5, 25},
             
        //      // Shift bank - hold the alternate footswitch for 1s to reach these.
        //      {0, ModulatedTremoloModule::OSC_WAVE, 6, 26},
        //      {0, ModulatedTremoloModule::OSC_FREQ, 7, 27}}
        // ),

        // Example of the alternate footswitch's toggle mode: instead of tap
        // tempo, pressing it toggles the tremolo off/on (and leaves the
        // reverb and delay always-on). Change footswitchTogglesSlots to {0, 1}
        // to toggle tremolo+delay together, or to {} to give the footswitch
        // back to the delay for tap tempo instead.
        // Note that DelayModule is single-instance-only so cannot be used
        // standalone if it is used here.

        // new EffectChain(
        //     "Ambience",
        //     {{"HT", new HarmonicTremoloModule()}, {"Dl", new DelayModule()}, {"Rv", new DattorroReverbModule()}},
        //     // Knob/MIDI CC mappings: {slot, child param id, knob (-1 = none), midi CC (-1 = none)}.
        //     {{2, DattorroReverbModule::MIX,       0, 20},
        //      {0, HarmonicTremoloModule::DEPTH,    1, 21},
        //      {0, HarmonicTremoloModule::SPEED,    2, 22},

        //      {1, DelayModule::DELAY_MIX,          3, 23},
        //      {1, DelayModule::DELAY_TIME,         4, 24},
        //      {1, DelayModule::D_FEEDBACK,         5, 25},

        //      // Shift bank - hold the alternate footswitch for 1s to reach these.
        //      {2, DattorroReverbModule::TONE,      6, 26},
        //      {2, DattorroReverbModule::DECAY,     7, 27},
        //      {2, DattorroReverbModule::SIZE,      8, 28},
             
        //      {1, DelayModule::D_SPREAD,            9, 29},
        //      {1, DelayModule::MOD_AMT,          10, 30},
        //      {1, DelayModule::MOD_RATE,            11, 31}},
        //     /* primarySlot */ 1, // delay owns the LED
        //     /* footswitchTogglesSlots */ {0} // but footswitch toggles tremolo on/off
        // ),
    };
    // clang-format on

    // The first entry is a placeholder, so that the array is never empty (a list without effects)
    static const uint32_t heapEstimateList[] = {
        0,
        #include "generated/effects_heap.inc"
    };

    availableEffectsCount = sizeof(effectList) / sizeof(effectList[0]);
    availableEffects = effectList;
    heapEstimates = &heapEstimateList[1];
}

} // namespace bkshepherd

#endif
