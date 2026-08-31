#include "dattorro_reverb_module.h"
#include "Dattorro/dsp/delays/InterpDelay.hpp"
#include <array>

using namespace bkshepherd;

namespace {
// 1 MiB of SDRAM, shared by every InterpDelay the Dattorro engine owns (the
// tank's 8 delay lines plus the input section's pre-delay and 4 allpass
// delays). At full 48kHz with the maxTimeScale=4.0 headroom below this comes
// to ~870 KB, so this leaves ~15% margin. See
// Effect-Modules/Dattorro/dsp/delays/InterpDelay.hpp for how the arena works
// and Effect-Modules/Dattorro/README.md for how this figure was derived.
constexpr size_t kDattorroArenaFloatCount = 262144; // 1 MiB / sizeof(float)

constexpr float kFactoryResetHoldSeconds = 5.0f;
constexpr float kFactoryResetFlashSeconds = 1.0f;
} // namespace

float DSY_SDRAM_BSS s_dattorroArena[kDattorroArenaFloatCount];

static const auto s_metaData = [] {
    std::array<ParameterMetaData, DattorroReverbModule::PARAM_COUNT> params{};

    params[DattorroReverbModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 0,
        midiCCMapping : 1
    };

    params[DattorroReverbModule::PRE_DELAY] = {
        name : "Pre Dly",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 1,
        midiCCMapping : 21
    };

    params[DattorroReverbModule::DECAY] = {
        name : "Decay",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.8f},
        knobMapping : 2,
        midiCCMapping : 22
    };

    params[DattorroReverbModule::TONE] = {
        name : "Tone",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.725f},
        knobMapping : 3,
        midiCCMapping : 23
    };

    params[DattorroReverbModule::MOD] = {
        name : "Mod",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 4,
        midiCCMapping : 24
    };

    params[DattorroReverbModule::DIFFUSE] = {
        name : "Diffuse",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.85f},
        knobMapping : 5,
        midiCCMapping : 25
    };

    params[DattorroReverbModule::SIZE] = {
        name : "Size",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        // (0.5075 - 0.5) / 3.5 -> setTimeScale(1.0075), matching the fixed
        // Time Scale Flick/MuleBox/AmpSim all use.
        defaultValue : {.float_value = 0.145f},
        knobMapping : -1, // Menu only - all six knobs are already mapped above.
        midiCCMapping : 26
    };

    return params;
}();

// Default Constructor
DattorroReverbModule::DattorroReverbModule() : BaseEffectModule() {
    // Set the name of the effect. Keep this at or under 11 characters ("Multi
    // Delay" is the longest existing name) - a longer name centered at the
    // display's large Font_11x18 (used for both the Settings -> Effect list
    // and the home-screen name banner) computes a negative starting X
    // coordinate, which wraps around in libDaisy's OneBitGraphicsDisplay::
    // SetCursor(uint16_t x, ...) and renders as a completely blank line
    // instead of clipping.
    m_name = "Dattorro";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
DattorroReverbModule::~DattorroReverbModule() {
    // No Code Needed
}

void DattorroReverbModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    // Arm the SDRAM arena before constructing the Dattorro engine below -
    // every InterpDelay it (transitively) owns carves its buffer from this
    // arena the moment it's constructed, instead of falling back to the heap.
    InterpDelayArena::set(s_dattorroArena, kDattorroArenaFloatCount);

    // initMaxLfoDepth=16.0f, initMaxTimeScale=4.0f match AmpSim's own
    // construction call, just against the full (not halved) sample rate -
    // see the plan/README for why those values transfer directly.
    m_dattorro = std::make_unique<Dattorro>(sample_rate, 16.0f, 4.0f);

    // Check before releasing the arena pointer - exhausted() is reset by set().
    m_arenaExhausted = InterpDelayArena::exhausted();

    // Detach the arena now that every delay line has claimed its slice;
    // nothing else in the firmware should allocate from it.
    InterpDelayArena::set(nullptr, 0);

    // Fixed settings shared by Flick/MuleBox/AmpSim, not exposed as
    // Parameters - see Effect-Modules/Dattorro/README.md.
    m_dattorro->enableInputDiffusion(true);
    m_dattorro->setInputFilterLowCutoffPitch(2.87f);
    m_dattorro->setInputFilterHighCutoffPitch(7.25f);
    m_dattorro->setTankFilterLowCutFrequency(2.87f);
    m_dattorro->setTankModShape(0.25f);

    // InitParams() (called from the constructor, above) wrote every
    // Parameter's default value directly into storage without routing
    // through ParameterChanged(), so the Dattorro engine just constructed
    // above still has its own hardcoded defaults. Push every current
    // Parameter value into it now.
    SyncAllParametersToEngine();
}

void DattorroReverbModule::SyncAllParametersToEngine() {
    for (int i = 0; i < PARAM_COUNT; ++i) {
        ParameterChanged(i);
    }
}

void DattorroReverbModule::ResetNonMixParametersToDefaults() {
    for (int i = 0; i < PARAM_COUNT; ++i) {
        if (i == MIX) {
            continue;
        }

        SetParameterToDefault(i);
    }

    // The UI caches Parameter values and writes them back into this Effect
    // every tick; without this it would silently revert the reset above on
    // the very next tick.
    RequestParameterResync();
}

void DattorroReverbModule::ParameterChanged(int parameter_id) {
    if (!m_dattorro) {
        return;
    }

    switch (parameter_id) {
    case PRE_DELAY:
        // 0..250ms, matching MuleBox's range.
        m_dattorro->setPreDelay(GetParameterAsFloat(PRE_DELAY) * 0.25f);
        break;

    case DECAY:
        m_dattorro->setDecay(GetParameterAsFloat(DECAY));
        break;

    case TONE:
        // Despite the name, setTankFilterHighCutFrequency takes a pitch value
        // (internally converted via 440 * 2^(pitch - 5)), not raw Hz - this
        // scaling is what Flick's edit mode uses too.
        m_dattorro->setTankFilterHighCutFrequency(GetParameterAsFloat(TONE) * 10.0f);
        break;

    case MOD: {
        float v = GetParameterAsFloat(MOD);
        m_dattorro->setTankModSpeed(0.5f + v);
        // Depth is capped near the tank's 1.0 design ceiling (excursion is
        // depth * 16 samples against a timePadding of 16).
        m_dattorro->setTankModDepth(0.1f + v * 0.9f);
        break;
    }

    case DIFFUSE:
        m_dattorro->setTankDiffusion(GetParameterAsFloat(DIFFUSE));
        break;

    case SIZE:
        // 0.5..4.0 - this is Dattorro's internal Time Scale, which is what
        // separates a plate voicing from a room/hall voicing.
        m_dattorro->setTimeScale(0.5f + GetParameterAsFloat(SIZE) * 3.5f);
        break;

    case MIX:
    default:
        // Mix is applied directly as a dry/wet crossfade in ProcessMono/
        // ProcessStereo - nothing to push into the engine for it.
        break;
    }
}

void DattorroReverbModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    float dry = m_audioLeft;

    if (m_arenaExhausted || !m_dattorro) {
        // Degrade to dry-only rather than risk unbounded heap fallback on an
        // embedded target.
        return;
    }

    m_dattorro->process(dry, dry);

    float wetL = m_dattorro->getLeftOutput();
    float wetR = m_dattorro->getRightOutput();

    // Squared-taper crossfade so Mix can reach fully wet, unlike AmpSim's
    // amp-sim-oriented dry floor.
    float wetGain = GetParameterAsFloat(MIX) * GetParameterAsFloat(MIX);
    float dryGain = 1.0f - wetGain;

    m_audioLeft = wetL * wetGain + dry * dryGain;
    m_audioRight = wetR * wetGain + dry * dryGain;
}

void DattorroReverbModule::ProcessStereo(float inL, float inR) {
    // Do the base stereo calculation (which resets the right signal to be the
    // inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(inL, inR);

    float dryL = m_audioLeft;
    float dryR = m_audioRight;

    if (m_arenaExhausted || !m_dattorro) {
        return;
    }

    m_dattorro->process(dryL, dryR);

    float wetL = m_dattorro->getLeftOutput();
    float wetR = m_dattorro->getRightOutput();

    float wetGain = GetParameterAsFloat(MIX) * GetParameterAsFloat(MIX);
    float dryGain = 1.0f - wetGain;

    m_audioLeft = wetL * wetGain + dryL * dryGain;
    m_audioRight = wetR * wetGain + dryR * dryGain;
}

void DattorroReverbModule::AlternateFootswitchPressed() {
    m_alternateFootswitchHeld = true;
    m_alternateFootswitchHeldSeconds = 0.0f;
    m_factoryResetTriggeredThisHold = false;
}

void DattorroReverbModule::AlternateFootswitchReleased() {
    m_alternateFootswitchHeld = false;
    m_alternateFootswitchHeldSeconds = 0.0f;
    m_factoryResetTriggeredThisHold = false;
}

void DattorroReverbModule::SetEnabled(bool isEnabled) {
    BaseEffectModule::SetEnabled(isEnabled);

    if (!isEnabled) {
        // AlternateFootswitchPressed/Released are only dispatched while this
        // Effect is enabled, so a hold that spans a bypass toggle would
        // otherwise strand the accumulator mid-count.
        m_alternateFootswitchHeld = false;
        m_alternateFootswitchHeldSeconds = 0.0f;
        m_factoryResetTriggeredThisHold = false;
    }
}

void DattorroReverbModule::UpdateUI(float elapsedTime) {
    BaseEffectModule::UpdateUI(elapsedTime);

    if (m_alternateFootswitchHeld && !m_factoryResetTriggeredThisHold) {
        m_alternateFootswitchHeldSeconds += elapsedTime;

        if (m_alternateFootswitchHeldSeconds >= kFactoryResetHoldSeconds) {
            ResetNonMixParametersToDefaults();
            m_factoryResetTriggeredThisHold = true;
            m_factoryResetFlashSecondsRemaining = kFactoryResetFlashSeconds;
        }
    }

    if (m_factoryResetFlashSecondsRemaining > 0.0f) {
        m_factoryResetFlashSecondsRemaining -= elapsedTime;
    }
}

void DattorroReverbModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                                  bool isEditing) {
    // Flash a full-screen "DEFAULTS" confirmation for a second after a
    // factory reset fires, mirroring how EffectModuleMenuItem takes over the
    // whole screen for its "Saving..." notification - a small corner label at
    // Font_6x8 turned out to be easy to miss.
    if (m_factoryResetFlashSecondsRemaining > 0.0f) {
        display.WriteStringAligned("DEFAULTS", Font_11x18, boundsToDrawIn, Alignment::centered, true);
        return;
    }

    // Draw the base UI
    BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);
}
