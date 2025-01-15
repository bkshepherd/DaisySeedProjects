// Edit the contents of this file to populate the available effects that you
// want to use

#include "Effect-Modules/autopan_module.h"
#include "Effect-Modules/chopper_module.h"
#include "Effect-Modules/chorus_module.h"
#include "Effect-Modules/compressor_module.h"
#include "Effect-Modules/geq_module.h"
#include "Effect-Modules/looper_module.h"
#include "Effect-Modules/metro_module.h"
#include "Effect-Modules/multi_delay_module.h"
#include "Effect-Modules/noise_gate_module.h"
#include "Effect-Modules/overdrive_module.h"
#include "Effect-Modules/peq_module.h"
// #include "Effect-Modules/pitch_shifter_module.h" // Commented out to make room in DTCRAM
#include "Effect-Modules/reverb_module.h"
#include "Effect-Modules/tuner_module.h"

#include "Effect-Modules/amp_module.h"
#include "Effect-Modules/cloudseed_module.h" // Takes up significant SDRAM (about 30%)
#include "Effect-Modules/nam_module.h"
// #include "Effect-Modules/midi_keys_module.h"
#include "Effect-Modules/delay_module.h"
// #include "Effect-Modules/pluckecho_module.h"
// #include "Effect-Modules/modal_keys_module.h"
// #include "Effect-Modules/string_keys_module.h"
#include "Effect-Modules/polyoctave_module.h"
#include "Effect-Modules/scifi_module.h"
#include "Effect-Modules/spectral_delay_module.h"

int load_effects(BaseEffectModule **availableEffects) {
    // Make sure this count matches the maxindex - 1 (count) of effects that get
    // added to the array
    const int availableEffectsCount = 21;
    availableEffects = new BaseEffectModule *[availableEffectsCount];
    availableEffects[0] = new ModulatedTremoloModule();
    availableEffects[1] = new OverdriveModule();
    availableEffects[2] = new AutoPanModule();
    availableEffects[3] = new ChorusModule();
    availableEffects[4] = new ChopperModule();
    availableEffects[5] = new ReverbModule();
    availableEffects[6] = new MultiDelayModule();
    availableEffects[7] = new MetroModule();
    availableEffects[8] = new TunerModule();
    // availableEffects[9] = new PitchShifterModule();
    availableEffects[9] = new CompressorModule();
    availableEffects[10] = new LooperModule();
    availableEffects[11] = new GraphicEQModule();
    availableEffects[12] = new ParametricEQModule();
    availableEffects[13] = new NoiseGateModule();

    availableEffects[14] = new CloudSeedModule();
    availableEffects[15] = new AmpModule();
    availableEffects[16] = new DelayModule();
    availableEffects[17] = new NamModule();
    availableEffects[18] = new SciFiModule();
    availableEffects[19] = new PolyOctaveModule();
    availableEffects[20] = new SpectralDelayModule();

    // The following require a MIDI keyboard
    // availableEffects[21] = new MidiKeysModule();
    // availableEffects[22] = new PluckEchoModule();
    // availableEffects[23] = new StringKeysModule();
    // availableEffects[24] = new ModalKeysModule();

    return availableEffectsCount;
}