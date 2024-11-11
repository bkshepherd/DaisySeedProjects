#include "autopan_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

static const char *s_waveBinNames[8] = {"Sine", "Triangle", "Saw", "Ramp", "Square", "Poly Tri", "Poly Saw", "Poly Sqr"};

static const int s_paramCount = 4;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {name : "Wet", valueType : ParameterValueType::FloatMagnitude, defaultValue : 127, knobMapping : 0, midiCCMapping : 20},
    {
        name : "Osc Wave",
        valueType : ParameterValueType::Binned,
        valueBinCount : 8,
        valueBinNames : s_waveBinNames,
        defaultValue : 0,
        knobMapping : 2,
        midiCCMapping : 21
    },
    {name : "Osc Freq", valueType : ParameterValueType::FloatMagnitude, defaultValue : 12, knobMapping : 1, midiCCMapping : 1},
    {name : "Stereo",
     valueType : ParameterValueType::Bool,
     defaultValue : 0,
     knobMapping : -1,
     midiCCMapping : 23}}; // 0 is Mono (even if fed stereo) 1 is Stereo

// Default Constructor
AutoPanModule::AutoPanModule()
    : BaseEffectModule(), m_freqOscFreqMin(0.01f), m_freqOscFreqMax(4.0f)

{
    // Set the name of the effect
    m_name = "AutoPan";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
AutoPanModule::~AutoPanModule() {
    // No Code Needed
}

void AutoPanModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    m_freqOsc.Init(sample_rate);
}

void AutoPanModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    // Calculate Pan Oscillation
    m_freqOsc.SetWaveform(GetParameterAsBinnedValue(1) - 1);
    m_freqOsc.SetAmp(0.5f);
    m_freqOsc.SetFreq(m_freqOscFreqMin + (GetParameterAsMagnitude(2) * m_freqOscFreqMax));
    float mod = 0.5f + m_freqOsc.Process();

    if (GetParameterRaw(2) == 0) {
        mod = 0.5f;
        m_pan = mod;
    }

    // Ease the m_pan value into it's target to avoid clipping
    fonepole(m_pan, mod, .001f);

    float r = sqrtf(2.0f) / 2.0f;
    float scaled = m_pan * HALFPI_F;
    float angle = scaled / 2.0f;

    float audioLeftWet = m_audioLeft * (r * (cosf(angle) - sinf(angle)));
    float audioRightWet = m_audioRight * (r * (cosf(angle) + sinf(angle)));

    // Handle the wet / dry mix
    m_audioLeft = audioLeftWet * GetParameterAsMagnitude(0) + m_audioLeft * (1.0f - GetParameterAsMagnitude(0));
    m_audioRight = audioRightWet * GetParameterAsMagnitude(0) + m_audioRight * (1.0f - GetParameterAsMagnitude(0));
}

void AutoPanModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);

    // If we are processing in mono only no need to do anything
    if (!GetParameterAsBool(3)) {
        return;
    }

    // Do the base stereo calculation (which resets both signals even though we calculated it in mono)
    BaseEffectModule::ProcessStereo(inL, inR);

    // Calculate Stereo Effect
    float adjustedPan = (m_pan - 0.5f) * 2.0f;
    float mSignal = 0.5f * (m_audioLeft + m_audioRight);
    float sSignal = m_audioLeft - m_audioRight;

    float audioLeftWet = 0.5f * (1.0f + adjustedPan) * mSignal + sSignal;
    float audioRightWet = 0.5f * (1.0f - adjustedPan) * mSignal - sSignal;

    // Handle the wet / dry mix
    m_audioLeft = audioLeftWet * GetParameterAsMagnitude(0) + m_audioLeft * (1.0f - GetParameterAsMagnitude(0));
    m_audioRight = audioRightWet * GetParameterAsMagnitude(0) + m_audioRight * (1.0f - GetParameterAsMagnitude(0));
}

void AutoPanModule::SetTempo(uint32_t bpm) {
    float freq = tempo_to_freq(bpm);

    // Adjust the frequency into a range that makes sense for the effect
    freq = freq / 4.0f;

    if (freq <= m_freqOscFreqMin) {
        SetParameterRaw(2, 0);
    } else if (freq >= m_freqOscFreqMax) {
        SetParameterRaw(2, 127);
    } else {
        // Get the parameter as close as we can to target tempo
        SetParameterRaw(2, ((freq - m_freqOscFreqMin) / (m_freqOscFreqMax - m_freqOscFreqMin)) * 128);
    }
}

float AutoPanModule::GetBrightnessForLED(int led_id) {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_pan;
    }

    return value;
}

void AutoPanModule::UpdateUI(float elapsedTime) {
    // Let the base class do it's thing.
    BaseEffectModule::UpdateUI(elapsedTime);
}

void AutoPanModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                           bool isEditing) {
    // Let the base class do it's thing.
    BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);

    // If the effect isn't enabled don't draw the custom UI
    if (!IsEnabled()) {
        return;
    }

    int lineY = (boundsToDrawIn.GetHeight() / 5.0f) * 3;
    int lineStartX = boundsToDrawIn.GetX();
    int lineEndX = boundsToDrawIn.GetRight();

    // calculate the pan location
    int panLocationX = lineStartX + (m_pan * (boundsToDrawIn.GetWidth() - 20));

    // Draws a line and a ball where the pan is currently located between L/R side
    display.DrawLine(lineStartX + 10, lineY, lineEndX - 10, lineY, true);
    display.DrawCircle(panLocationX + 10, lineY, 3, true);
    display.SetCursor(lineStartX, lineY - 4);
    display.WriteChar('L', Font_6x8, true);
    display.SetCursor(lineEndX - 6, lineY - 4);
    display.WriteChar('R', Font_6x8, true);
}
