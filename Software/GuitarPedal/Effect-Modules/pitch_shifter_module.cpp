#include "pitch_shifter_module.h"

#include <algorithm>

#include "../Util/pitch_shifter.h"
#include "daisysp.h"

using namespace bkshepherd;

static const char *s_semitoneBinNames[8] = {"1", "2", "3", "4", "5", "6", "7", "OCT"};
static const char *s_directionBinNames[2] = {"DOWN", "UP"};
static const char *s_modeBinNames[2] = {"LATCH", "MOMENT"};

// How many samples to delay to based on the "Delay" knob and parameter
// when the time knob is set to max, this is used for the ramp up/down
// transition when in momentary mode. Has no effect on latching mode
const uint32_t k_maxSamplesMaxTime = 48000 * 2;

// Larger delay size is higher fidelity/quality for a farther transpose, at
// the cost of additional latency (like...a lot of latency)
// 2048 works pretty well for 1 semitone and is ~42ms
// 6000 samples was the best I could find for a usable full octave tone,
// but is a whopping 125 ms, nearly unusable? kind of cool when blended
// with the dry signal though
const uint32_t k_defaultSamplesDelayPitchShifter = 2048;
const uint32_t k_maxSamplesDelayPitchShifter = 6000;

static const int s_paramCount = 6;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {
        name : "Semitone",
        valueType : ParameterValueType::Binned,
        valueBinCount : 8,
        valueBinNames : s_semitoneBinNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 0,
        midiCCMapping : -1
    },
    {
        name : "Crossfade",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 1.0f},
        knobMapping : 1,
        midiCCMapping : -1
    },
    {
        name : "Direction",
        valueType : ParameterValueType::Binned,
        valueBinCount : 2,
        valueBinNames : s_directionBinNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 2,
        midiCCMapping : -1
    },
    {
        name : "Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 2,
        valueBinNames : s_modeBinNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 3,
        midiCCMapping : -1
    },
    {
        name : "Shift",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 4,
        midiCCMapping : -1
    },
    {
        name : "Return",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 5,
        midiCCMapping : -1
    },
};

DSY_SDRAM_BSS float pitch_delay_buffer_a[k_maxSamplesDelayPitchShifter];
DSY_SDRAM_BSS float pitch_delay_buffer_b[k_maxSamplesDelayPitchShifter];

static daisysp_modified::PitchShifter pitchShifter;
static daisysp::CrossFade pitchCrossfade;

// Default Constructor
PitchShifterModule::PitchShifterModule() : BaseEffectModule() {
    m_name = "Pitch";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
PitchShifterModule::~PitchShifterModule() {}

void PitchShifterModule::ProcessSemitoneTargetChange() {
    int semitoneNum = GetParameterAsBinnedValue(0);

    // If this is the last semitone option, convert it to be a full octave
    if (semitoneNum == 8) {
        semitoneNum = 12;
    }

    m_semitoneTarget = semitoneNum;
    if (m_directionDown) {
        m_semitoneTarget *= -1.0f;
    }
}

void PitchShifterModule::SetTranspose(float semitone) {
    // If this is latching, then we also adjust the delay to get the best sound
    // from the adjustments at the cost of increased latency
    if (m_latching) {
        // Value between 0 and 1 where 0 is 1 semitone and 1 is 12 semitones
        const float interpolateValue = (std::abs(semitone) - 1.0f) / 11.0f;

        // Linearly just choose a value between the min and max based on how many
        // semitones we are dropping
        const uint32_t delaySize = std::lerp(k_defaultSamplesDelayPitchShifter, k_maxSamplesDelayPitchShifter, interpolateValue);

        // Set delay size clamping to the min/max that are possible
        pitchShifter.SetDelSize(std::clamp(delaySize, k_defaultSamplesDelayPitchShifter, k_maxSamplesDelayPitchShifter));
    }
    pitchShifter.SetTransposition(semitone);
}

void PitchShifterModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    // clear and initialize SDRAM for pitch shift buffers
    memset(pitch_delay_buffer_a, 0, sizeof(pitch_delay_buffer_a));
    memset(pitch_delay_buffer_b, 0, sizeof(pitch_delay_buffer_b));

    pitchShifter.Init(sample_rate, pitch_delay_buffer_a, pitch_delay_buffer_b, k_maxSamplesDelayPitchShifter);

    pitchCrossfade.Init(CROSSFADE_CPOW);
    pitchCrossfade.SetPos(GetParameterAsFloat(1));

    m_latching = GetParameterAsBinnedValue(3) == 1;

    m_directionDown = GetParameterAsBinnedValue(2) == 1;

    ProcessSemitoneTargetChange();

    SetTranspose(m_semitoneTarget);

    m_samplesToDelayShift = static_cast<uint32_t>(static_cast<float>(k_maxSamplesMaxTime) * GetParameterAsFloat(4));
    m_samplesToDelayReturn = static_cast<uint32_t>(static_cast<float>(k_maxSamplesMaxTime) * GetParameterAsFloat(5));
}

void PitchShifterModule::ParameterChanged(int parameter_id) {
    if (parameter_id == 0 || parameter_id == 2) {
        m_directionDown = GetParameterAsBinnedValue(2) == 1;

        // Change semitone when semitone knob is turned or direction is changed
        ProcessSemitoneTargetChange();
    } else if (parameter_id == 1) {
        pitchCrossfade.SetPos(GetParameterAsFloat(1));
    } else if (parameter_id == 3) {
        m_latching = GetParameterAsBinnedValue(3) == 1;
        if (!m_latching) {
            pitchShifter.SetDelSize(k_defaultSamplesDelayPitchShifter);
        }
    } else if (parameter_id == 4) {
        m_samplesToDelayShift = static_cast<uint32_t>(static_cast<float>(k_maxSamplesMaxTime) * GetParameterAsFloat(4));
    } else if (parameter_id == 5) {
        m_samplesToDelayReturn = static_cast<uint32_t>(static_cast<float>(k_maxSamplesMaxTime) * GetParameterAsFloat(5));
    }

    // Parameters changed, reset the transposition target just in case (mostly
    // impacts momentary/latch and delay)
    SetTranspose(m_semitoneTarget);
}

void PitchShifterModule::AlternateFootswitchPressed() {
    m_alternateFootswitchPressed = true;

    if (!m_latching) {
        // Initiate the ramp up transition for momentary mode
        m_transitioningShift = true;

        // We were in the middle of a return so reset for shifting
        if (m_transitioningReturn) {
            // Calculate sample counter to maintain the exact transpose/percentage we
            // are currently at to avoid it "starting from the beginning of the
            // transition"
            m_sampleCounter = (1.0f - m_percentageTransitionComplete) * m_samplesToDelayShift;
            m_transitioningReturn = false;
        }
    }
}

void PitchShifterModule::AlternateFootswitchReleased() {
    m_alternateFootswitchPressed = false;

    if (!m_latching) {
        // Initiate the ramp down transition for momentary mode
        m_transitioningReturn = true;

        // We were in the middle of a shift so reset for return
        if (m_transitioningShift) {
            // Calculate sample counter to maintain the exact transpose/percentage we
            // are currently at to avoid it "starting from the beginning of the
            // transition"
            m_sampleCounter = (1.0f - m_percentageTransitionComplete) * m_samplesToDelayReturn;
            m_transitioningShift = false;
        }
    }
}

void PitchShifterModule::ProcessMono(float in) {
    float out = in;

    if (m_latching) {
        // When in latching mode, just process the target semitone at all times
        // immediately
        float shifted = pitchShifter.Process(in);
        out = pitchCrossfade.Process(in, shifted);
    } else {
        out = ProcessMomentaryMode(in);
    }

    m_audioRight = m_audioLeft = out;
}

void PitchShifterModule::ProcessStereo(float inL, float inR) { ProcessMono(inL); }

float PitchShifterModule::ProcessMomentaryMode(float in) {
    // ---- Process when NOT in a ramp up/ramp down state ----
    if (!m_transitioningShift && !m_transitioningReturn) {
        // Make sure the the sample counter is ready for the next ramp up/down
        // transition time
        m_sampleCounter = 0;

        // Process the pitch shift for completely active to the target by default
        float semitone = m_semitoneTarget;
        if (!m_alternateFootswitchPressed) {
            // Process the pitch shift for completely inactive (0)
            semitone = 0.0f;
        }

        // Process the pitch shift for completely active to the target
        SetTranspose(semitone);
        float shifted = pitchShifter.Process(in);
        float out = pitchCrossfade.Process(in, shifted);
        return out;
    }

    // ---- Process ramp up/ramp down transition ----

    // When in momentary mode, there is a ramp up(pressed [shift])/ramp
    // down(released [return]) transition of the semitone based on the "delay"
    // parameter
    uint32_t samplesToDelay = 0;
    if (m_transitioningShift) {
        samplesToDelay = m_samplesToDelayShift;
    } else if (m_transitioningReturn) {
        samplesToDelay = m_samplesToDelayReturn;
    }

    if (samplesToDelay != 0) {
        // Clamp just to make sure we don't overshoot the goal just in case
        m_percentageTransitionComplete =
            std::clamp(static_cast<float>(m_sampleCounter) / static_cast<float>(samplesToDelay), 0.0f, 1.0f);
    } else {
        m_percentageTransitionComplete = 1.0f;
    }

    // Perform the pitch shift
    if (m_transitioningShift) {
        SetTranspose(m_semitoneTarget * m_percentageTransitionComplete);
    } else if (m_transitioningReturn) {
        SetTranspose(m_semitoneTarget * (1.0f - m_percentageTransitionComplete));
    }

    float shifted = pitchShifter.Process(in);
    float pitchOut = pitchCrossfade.Process(in, shifted);

    // Increment the counter for the next pass
    if (m_sampleCounter < samplesToDelay) {
        m_sampleCounter += 1;
    } else {
        // Transition is complete
        m_transitioningShift = false;
        m_transitioningReturn = false;
    }

    return pitchOut;
}

void PitchShifterModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                                bool isEditing) {
    BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);

    // Only add a UI for when we are in momentary mode
    if (m_latching) {
        return;
    }

    int width = boundsToDrawIn.GetWidth();
    int numBlocks = 20;
    int blockWidth = width / numBlocks;
    int top = 30;
    int x = 0;
    for (int block = 0; block < numBlocks; block++) {
        Rectangle r(x, top, blockWidth, blockWidth);

        bool active = false;
        if (m_transitioningShift) {
            if ((static_cast<float>(block) / static_cast<float>(numBlocks)) <= m_percentageTransitionComplete) {
                active = true;
            }
        } else if (m_transitioningReturn) {
            if (static_cast<float>(block) / static_cast<float>(numBlocks) <= (1.0f - m_percentageTransitionComplete)) {
                active = true;
            }
        } else {
            if (m_alternateFootswitchPressed) {
                active = true;
            }
        }

        display.DrawRect(r, true, active);
        x += blockWidth;
    }
}