// Edit the contents of this file to populate the available effects that you
// want to use

#ifndef LOADED_EFFECTS_H
#define LOADED_EFFECTS_H
#pragma once

#include "Effect-Modules/base_effect_module.h"

// Include all effect modules
#include "Effect-Modules/amp_module.h"
#include "Effect-Modules/autopan_module.h"
#include "Effect-Modules/chopper_module.h"
#include "Effect-Modules/chorus_module.h"
#include "Effect-Modules/cloudseed_module.h" // Takes up significant SDRAM (about 30%)
#include "Effect-Modules/compressor_module.h"
#include "Effect-Modules/delay_module.h"
#include "Effect-Modules/distortion_module.h"
#include "Effect-Modules/geq_module.h"
#include "Effect-Modules/granulardelay_module.h"
#include "Effect-Modules/looper_module.h"
#include "Effect-Modules/metro_module.h"
#include "Effect-Modules/modulated_tremolo_module.h"
#include "Effect-Modules/multi_delay_module.h"
#include "Effect-Modules/nam_module.h"
#include "Effect-Modules/noise_gate_module.h"
#include "Effect-Modules/overdrive_module.h"
#include "Effect-Modules/peq_module.h"
// #include "Effect-Modules/pitch_shifter_module.h" // Commented out to make room in DTCRAM
#include "Effect-Modules/polyoctave_module.h"
#include "Effect-Modules/reverb_module.h"
#include "Effect-Modules/scifi_module.h"
#include "Effect-Modules/spectral_delay_module.h"
#include "Effect-Modules/tuner_module.h"

// Keyboard modules
// #include "Effect-Modules/fm_keys_module.h"
// #include "Effect-Modules/midi_keys_module.h"
// #include "Effect-Modules/modal_keys_module.h"
// #include "Effect-Modules/pluckecho_module.h"
// #include "Effect-Modules/string_keys_module.h"

void load_effects(int &availableEffectsCount, BaseEffectModule **&availableEffects) {
    // clang-format off
    static BaseEffectModule* effectList[] = {
        new ModulatedTremoloModule(),
        new OverdriveModule(),
        new AutoPanModule(),
        new ChorusModule(),
        new ChopperModule(),
        new ReverbModule(),
        new MultiDelayModule(),
        new MetroModule(),
        new TunerModule(),
        // new PitchShifterModule(),
        new CompressorModule(),
        new LooperModule(),
        new GraphicEQModule(),
        new ParametricEQModule(),
        new NoiseGateModule(),
        new CloudSeedModule(),
        new AmpModule(),
        new DelayModule(),
        new NamModule(),
        new SciFiModule(),
        new PolyOctaveModule(),
        new SpectralDelayModule(),
        new DistortionModule(),
        new GranularDelayModule(), 

        // The following require a MIDI keyboard
        // new MidiKeysModule(),
        // new PluckEchoModule(),
        // new StringKeysModule(),
        // new ModalKeysModule(),
        // new FmKeysModule(),
    };
    // clang-format on

    availableEffectsCount = sizeof(effectList) / sizeof(effectList[0]);
    availableEffects = new BaseEffectModule *[availableEffectsCount];
    for (int i = 0; i < availableEffectsCount; ++i) {
        availableEffects[i] = effectList[i];
    }
}

#endif
