#include "chopper_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

static const int s_paramCount = 4;
static const ParameterMetaData s_metaData[s_paramCount] = {{
                                                               name : "Wet",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.5f},
                                                               knobMapping : 0,
                                                               midiCCMapping : 20
                                                           },
                                                           {
                                                               name : "Tempo",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.5f},
                                                               knobMapping : 1,
                                                               midiCCMapping : 23
                                                           },
                                                           {
                                                               name : "Duty",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.25f},
                                                               knobMapping : 2,
                                                               midiCCMapping : 24
                                                           },
                                                           {
                                                               name : "Pattern",
                                                               valueType : ParameterValueType::Binned,
                                                               valueBinCount : 14,
                                                               defaultValue : {.uint_value = 0},
                                                               knobMapping : 3,
                                                               midiCCMapping : 25
                                                           }};

// Default Constructor
ChopperModule::ChopperModule()
    : BaseEffectModule(), m_tempoFreqMin(0.5f), m_tempoFreqMax(4.0f), m_pulseWidthMin(0.1f), m_pulseWidthMax(0.9f),
      m_cachedEffectMagnitudeValue(1.0f)

{
    // Set the name of the effect
    m_name = "Chopper";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
ChopperModule::~ChopperModule() {
    // No Code Needed
}

void ChopperModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    m_chopper.Init(sample_rate);
}

void ChopperModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    // Setup the Effect
    m_chopper.SetFreq(m_tempoFreqMin + (GetParameterAsFloat(1) * (m_tempoFreqMax - m_tempoFreqMin)));
    m_chopper.SetAmp(1.0f);
    m_chopper.SetPw(m_pulseWidthMin + (GetParameterAsFloat(2) * (m_pulseWidthMax - m_pulseWidthMin)));
    m_chopper.SetPattern(GetParameterAsBinnedValue(3) - 1);

    // Calculate the Effect
    // Ease the effect value into it's target to avoid clipping
    fonepole(m_cachedEffectMagnitudeValue, m_chopper.Process(), .01f);

    float audioLeftWet = m_cachedEffectMagnitudeValue * m_audioLeft;

    // Handle the wet / dry mix
    m_audioLeft = audioLeftWet * GetParameterAsFloat(0) + m_audioLeft * (1.0f - GetParameterAsFloat(0));
    m_audioRight = m_audioLeft;
}

void ChopperModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the
    // inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(m_audioLeft, inR);

    // Calculate the Effect
    float audioRightWet = m_cachedEffectMagnitudeValue * m_audioRight;

    // Handle the wet / dry mix
    m_audioRight = audioRightWet * GetParameterAsFloat(0) + m_audioRight * (1.0f - GetParameterAsFloat(0));
}

void ChopperModule::SetTempo(uint32_t bpm) {
    float freq = tempo_to_freq(bpm);

    // Adjust the frequency into a range that makes sense for the effect
    freq = freq / 4.0f;

    if (freq <= m_tempoFreqMin) {
        SetParameterAsMagnitude(1, 0.0f);
    } else if (freq >= m_tempoFreqMax) {
        SetParameterAsMagnitude(1, 1.0f);
    } else {
        float magnitude = static_cast<float>(freq - m_tempoFreqMin) / static_cast<float>(m_tempoFreqMax - m_tempoFreqMin);
        SetParameterAsMagnitude(1, magnitude);
    }
}

float ChopperModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}

void ChopperModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                           bool isEditing) {
    BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);

    Pattern pattern = m_chopper.GetPattern(GetParameterAsBinnedValue(3) - 1);
    int width = boundsToDrawIn.GetWidth();
    int stepWidth = (width / PATTERN_STEPS_MAX);
    int top = 30;

    int x = 0;
    for (int step = 0; step < pattern.length; step++) {
        Note note = pattern.notes[step];
        switch (note.duration) {
        case NoteDuration::D16: {
            Rectangle r(x, top, stepWidth - 2, stepWidth - 2);
            display.DrawRect(r, true, note.active);
            x += stepWidth;
        } break;
        case NoteDuration::D8: {
            Rectangle r(x, top, stepWidth * 2 - 2, stepWidth - 2);
            display.DrawRect(r, true, note.active);
            x += stepWidth * 2;
        } break;
        case NoteDuration::D4: {
            Rectangle r(x, top, stepWidth * 4 - 2, stepWidth - 2);
            display.DrawRect(r, true, note.active);
            x += stepWidth * 4;
        } break;
        }
    }
}