#include "cloudseed_module.h"
#include "../Util/audio_utilities.h"
#include <array>

// The delay line memory for the CloudSeed reverb engine comes from the pool in cloudseed_pool.cpp (shared with the Jupiter module).

using namespace bkshepherd;

static const char *s_presetNames[8] = {"FChorus", "DullEchos", "Hyperplane", "MedSpace", "Hallway", "RubiKa", "SmallRoom", "90s"};

static auto s_metaData = [] {
    std::array<ParameterMetaData, CloudSeedModule::PARAM_COUNT> params{};

    params[CloudSeedModule::PRE_DELAY] = {
        name : "PreDelay",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 0,
        midiCCMapping : 14,
        unit : " ms",
        decimals : 0,
        // engine: (int)(0.95 * v * 1000) ms
        displayFn : [](float v) { return floorf(v * 0.95f * 1000.0f); }
    };

    params[CloudSeedModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 15,
        unit : " %",
        decimals : 0,
        // crossfade position
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[CloudSeedModule::DECAY] = {
        name : "Decay",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 2,
        midiCCMapping : 16,
        unit : " s",
        decimals : 2,
        // CloudSeed LineDecay (T60 of the delay lines): 0.05 + Response3Dec(v) * 59.95 s
        displayFn : [](float v) { return 0.05f + (powf(1000.0f, v) - 1.0f) / 999.0f * 59.95f; }
    };

    params[CloudSeedModule::MOD_AMT] = {
        name : "Mod Amt",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 3,
        midiCCMapping : 17,
        unit : " ms",
        decimals : 2,
        // CloudSeed LineModAmount: v * 2.5 ms
        displayFn : [](float v) { return v * 2.5f; }
    };

    params[CloudSeedModule::MOD_RATE] = {
        name : "Mod Rate",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 4,
        midiCCMapping : 18,
        unit : " Hz",
        decimals : 2,
        // CloudSeed LineModRate: Response2Dec(v) * 5 Hz (per line scaled 0.7..1.0)
        displayFn : [](float v) { return (powf(100.0f, v) - 1.0f) / 99.0f * 5.0f; }
    };

    params[CloudSeedModule::TONE] = {
        name : "Tone",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 5,
        midiCCMapping : 19,
        unit : " Hz",
        decimals : 0,
        // CloudSeed PostCutoffFrequency: 400 + Response4Oct(v) * 19600 Hz
        displayFn : [](float v) { return 400.0f + (powf(16.0f, v) - 1.0f) / 15.0f * 19600.0f; }
    };

    params[CloudSeedModule::PRESET] = {
        name : "Preset",
        valueType : ParameterValueType::Binned,
        valueBinCount : 8,
        valueBinNames : s_presetNames,
        defaultValue : {.uint_value = 5}, // RubiKa: the preset Init() loads, so the menu shows what is actually running
        knobMapping : -1,
        midiCCMapping : 20
    };

    params[CloudSeedModule::SUM2MONO] = {
        name : "Sum2Mono",
        valueType : ParameterValueType::Bool,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 21
    };

    params[CloudSeedModule::STEREO_IN] = {
        name : "StereoIn",
        valueType : ParameterValueType::Bool,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 22
    };

    // "KnobsOverride(Presets)" - If true, current knob settings override the selected preset.
    // If false, preset changes apply until a knob is moved.
    params[CloudSeedModule::KNOBS_OVRD] = {
        name : "KnobsOvrd",
        valueType : ParameterValueType::Bool,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 23
    };

    params[CloudSeedModule::DRY_VOLUME] = {
        name : "DryVolume",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 24,
        unit : " dB",
        decimals : 1,
        // dry gain is v itself (linear)
        displayFn : [](float v) { return ParamDisplay::Db(v); }
    };

    return params;
}();

// NOTES ABOUT THE CLOUDSEED PARAMETERS
// 1. I changed the "Parameter" class to be "Parameter2" to deconflict with the DaisySP Parameter. Likely there was a much easier way
// to remedy this.
// 2. I have hard coded the presets to work with the limited processing on the Daisy Seed. The main parameters that affect processing
// are:
//        Mostly these:  LineCount, LateDiffusionStages      And to a lesser degree these:  DiffusionStages, TapCount (Added tapcount
//        as param, halved the full range)
//     Increasing the above params past what I have them set at in "ReverbController.h" may freeze the pedal processing.
//     Currently the max stereo line count is 2 on the Daisy Seed, so the presets will sound different than the desktop CloudSeed
//     plugin.

// Default Constructor
CloudSeedModule::CloudSeedModule() : BaseEffectModule(), m_gainMin(0.0f), m_gainMax(1.0f), m_cachedEffectMagnitudeValue(1.0f) {
    // Set the name of the effect
    m_name = "CloudSeed";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
CloudSeedModule::~CloudSeedModule() {
    // No Code Needed
}

void CloudSeedModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    AudioLib::ValueTables::Init();
    CloudSeed::FastSin::Init();

    reverb = new CloudSeed::ReverbController(sample_rate);
    reverb->ClearBuffers();
    reverb->initFactoryRubiKaFields();                // Not setting a preset at the beginning allows you to save the previous preset
    reverb->SetParameter(::Parameter2::LineCount, 3); // 2 on factory chorus for stereo is max, 3 froze it
    CalculateMix();
}

void CloudSeedModule::ParameterChanged(int parameter_id) // Somewhere here is causeing issues on start up, if I take them out it works,
                                                         // adding them in breaks, but it worked once???
{
    if (parameter_id == PRESET) { // Preset
        // Change the preset and then override with current knob settings
        changePreset();
        if (GetParameterAsBool(
                KNOBS_OVRD)) { // If true, apply current knob settings over the chosen preset. If false, don't update until knob is moved.
            reverb->SetParameter(::Parameter2::PreDelay, GetParameterAsFloat(PRE_DELAY) * 0.95); // Was freezing pedal when set to 127
            reverb->SetParameter(::Parameter2::LineDecay, GetParameterAsFloat(DECAY));
            reverb->SetParameter(::Parameter2::LineModAmount, GetParameterAsFloat(MOD_AMT));
            reverb->SetParameter(::Parameter2::LineModRate, GetParameterAsFloat(MOD_RATE));
            reverb->SetParameter(
                ::Parameter2::CutoffEnabled,
                1.0); // If this knob is moved, turn on the cutoff filter, the presets will reset this on/off as needed
            reverb->SetParameter(::Parameter2::PostCutoffFrequency, GetParameterAsFloat(TONE));
        }
    } else {

        if (throttle_counter > 5) {  // The calls to reverb settings need to be throttled, else it will freeze the pedal on startup
                                     // TODO: This will prevent single param change calls (such as a single programmed midi message) to
                                     // only work every 5 calls. Figure out a better fix
            if (parameter_id == PRE_DELAY) { // {PreDelay
                reverb->SetParameter(::Parameter2::PreDelay, GetParameterAsFloat(PRE_DELAY) * 0.95); // Was freezing pedal when set to 127
            } else if (parameter_id == MIX) {                                                  // Mix
                if (!inputMuteForWet) {
                    linearChangeDryLevel.deactivate();
                    // If the wet-input is not frozen, update the mix normally
                    CalculateMix();
                } else {
                    // otherwise - update only the wet mix value
                    currentMix.wet = CalculateMix(GetParameterAsFloat(MIX)).wet;
                }
            } else if (parameter_id == DECAY) { // Decay
                reverb->SetParameter(::Parameter2::LineDecay, GetParameterAsFloat(DECAY));
            } else if (parameter_id == MOD_AMT) { // Mod Amt
                reverb->SetParameter(::Parameter2::LineModAmount, GetParameterAsFloat(MOD_AMT));
            } else if (parameter_id == MOD_RATE) { // Mod Rate
                reverb->SetParameter(::Parameter2::LineModRate, GetParameterAsFloat(MOD_RATE));
            } else if (parameter_id == TONE) { // Tone
                reverb->SetParameter(
                    ::Parameter2::CutoffEnabled,
                    1.0); // If this knob is moved, turn on the cutoff filter, the presets will reset this on/off as needed
                reverb->SetParameter(::Parameter2::PostCutoffFrequency, GetParameterAsFloat(TONE));
            } else if (parameter_id == DRY_VOLUME) { // Dry Volume, when wet-input is muted
                if (inputMuteForWet) {
                    linearChangeDryLevel.deactivate();
                    // When wet-input is frozen, this knob controls the dry input volume for the mix directly
                    currentMix.dry = GetParameterAsFloat(DRY_VOLUME);
                }
            }
            throttle_counter = 0;
        }
        throttle_counter += 1;
    }
}

void CloudSeedModule::AlternateFootswitchPressed() { 
    inputMuteForWet = !inputMuteForWet;

    if (inputMuteForWet) {
        // Don't set dryMix immediately, let it ramp in ProcessStereo/ProcessMono
        linearChangeDryLevel.activate(currentMix.dry, GetParameterAsFloat(DRY_VOLUME), linearChangeDryLevelSteps);

         // remap 'mix' knob to 'dry-volume' knob when wet-input is frozen
        s_metaData[DRY_VOLUME].knobMapping = s_metaData[MIX].knobMapping;
        s_metaData[MIX].knobMapping = -1;
    } else {
        // Don't set dryMix immediately, let it ramp in ProcessStereo/ProcessMono
        linearChangeDryLevel.activate(currentMix.dry, CalculateMix(GetParameterAsFloat(MIX)).dry, linearChangeDryLevelSteps);

        // remap 'dry-volume' knob to 'mix' knob when wet-input is active
        s_metaData[MIX].knobMapping = s_metaData[DRY_VOLUME].knobMapping;
        s_metaData[DRY_VOLUME].knobMapping = -1;
    }
}

void CloudSeedModule::changePreset() {

    int c = (GetParameterAsBinnedValue(PRESET) - 1);
    reverb->ClearBuffers();

    if (c == 0) {
        reverb->initFactoryChorus();
    } else if (c == 1) {
        reverb->initFactoryDullEchos();
    } else if (c == 2) {
        reverb->initFactoryHyperplane();
    } else if (c == 3) {
        reverb->initFactoryMediumSpace();
    } else if (c == 4) {
        reverb->initFactoryNoiseInTheHallway();
    } else if (c == 5) {
        reverb->initFactoryRubiKaFields();
    } else if (c == 6) {
        reverb->initFactorySmallRoom();
    } else if (c == 7) {
        reverb->initFactory90sAreBack();
    }
}

void CloudSeedModule::CalculateMix() {
    currentMix = CalculateMix(GetParameterAsFloat(MIX));
}

CloudSeedModule::Mix CloudSeedModule::CalculateMix(float mixValue) {
    //    A computationally cheap mostly energy constant crossfade from SignalSmith Blog
    //    https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/

    float x2 = 1.0 - mixValue;
    float A = mixValue * x2;
    float B = A * (1.0 + 1.4186 * A);
    float C = B + mixValue;
    float D = B + x2;

    float wetMix = C * C;
    float dryMix = D * D;
    return Mix{wetMix, dryMix};
}

static float inMuted[CloudSeed::ReverbController::MaxBlockSize] = {0};

void CloudSeedModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    float inL[1];
    float outL[1];
    float inR[1];
    float outR[1];

    inL[0] = m_audioLeft;
    inR[0] = m_audioLeft;

    if (inputMuteForWet) {
        reverb->Process(inMuted, inMuted, outL, outR, 1);
    } else {
        reverb->Process(inL, inR, outL, outR, 1);
    }

    // Gradually ramp dryMix if transition is active
    if (linearChangeDryLevel.isActive()) {
        currentMix.dry = linearChangeDryLevel.getNextValue();
    }

    if (GetParameterAsBool(SUM2MONO)) { // If "Sum2Mono" is on, combine L and R signals and half the level
        m_audioLeft = ((outL[0] + outR[0]) / 2.0) * currentMix.wet + inL[0] * currentMix.dry;
        m_audioRight = m_audioLeft;
    } else {
        m_audioLeft = outL[0] * currentMix.wet + inL[0] * currentMix.dry;
        m_audioRight = outR[0] * currentMix.wet + inR[0] * currentMix.dry;
    }
}

void CloudSeedModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    // ProcessMono(inL);
    BaseEffectModule::ProcessStereo(inL, inR);

    float inL2[1];
    float outL[1];
    float inR2[1];
    float outR[1];

    inL2[0] = m_audioLeft;
    if (GetParameterAsBool(STEREO_IN)) { // If Stereo In is true (TODO Verify this works)
        inR2[0] = m_audioRight;
    } else {
        inR2[0] = m_audioLeft;
    }

    if (inputMuteForWet) {
        reverb->Process(inMuted, inMuted, outL, outR, 1);
    } else {
        reverb->Process(inL2, inR2, outL, outR, 1);
    }

    // Gradually ramp dryMix if transition is active
    if (linearChangeDryLevel.isActive()) {
        currentMix.dry = linearChangeDryLevel.getNextValue();
    }

    if (GetParameterAsBool(SUM2MONO)) { // If "Sum2Mono" is on, combine L and R signals and half the level
        m_audioLeft = ((outL[0] + outR[0]) / 2.0) * currentMix.wet + inL2[0] * currentMix.dry;
        m_audioRight = m_audioLeft;
    } else {
        m_audioLeft = outL[0] * currentMix.wet + inL2[0] * currentMix.dry;
        m_audioRight = outR[0] * currentMix.wet + inR2[0] * currentMix.dry;
    }
}

// Block versions. Same signal flow as ProcessMono/ProcessStereo, but the reverb stages run over the whole block per call instead of
// once per sample, which removes most of the per-call overhead of walking every stage for a single sample.
void CloudSeedModule::ProcessBlockInternal(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    static float wetL[CloudSeed::ReverbController::MaxBlockSize];
    static float wetR[CloudSeed::ReverbController::MaxBlockSize];

    if (inputMuteForWet) {
        reverb->Process(inMuted, inMuted, wetL, wetR, static_cast<int>(n));
    } else {
        // The reverb copies its inputs before writing outputs, but its API takes non-const pointers
        reverb->Process(const_cast<float *>(inL), const_cast<float *>(inR), wetL, wetR, static_cast<int>(n));
    }

    const bool sum2Mono = GetParameterAsBool(SUM2MONO);

    for (size_t i = 0; i < n; i++) {
        // Gradually ramp dryMix if transition is active
        if (linearChangeDryLevel.isActive()) {
            currentMix.dry = linearChangeDryLevel.getNextValue();
        }

        if (sum2Mono) { // If "Sum2Mono" is on, combine L and R signals and half the level
            const float l = ((wetL[i] + wetR[i]) / 2.0f) * currentMix.wet + inL[i] * currentMix.dry;
            outL[i] = l;
            outR[i] = l;
        } else {
            const float l = wetL[i] * currentMix.wet + inL[i] * currentMix.dry;
            const float r = wetR[i] * currentMix.wet + inR[i] * currentMix.dry;
            outL[i] = l;
            outR[i] = r;
        }
    }
}

void CloudSeedModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    constexpr size_t kMax = CloudSeed::ReverbController::MaxBlockSize;
    for (size_t start = 0; start < n; start += kMax) {
        const size_t len = (n - start < kMax) ? (n - start) : kMax;
        ProcessBlockInternal(in + start, in + start, outL + start, outR + start, len);
    }
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void CloudSeedModule::ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    constexpr size_t kMax = CloudSeed::ReverbController::MaxBlockSize;
    // If Stereo In is false the right input is a copy of the left, as in ProcessStereo.
    const float *right = GetParameterAsBool(STEREO_IN) ? inR : inL;
    for (size_t start = 0; start < n; start += kMax) {
        const size_t len = (n - start < kMax) ? (n - start) : kMax;
        ProcessBlockInternal(inL + start, right + start, outL + start, outR + start, len);
    }
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

float CloudSeedModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    static long flashCounter = 0;

    if (led_id == 1) {
        if (inputMuteForWet) {
            flashCounter++;
            if ((flashCounter / 10000) % 2 == 0) {
                return value * m_cachedEffectMagnitudeValue;
            } else {
                return 0;
            }
        }
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
