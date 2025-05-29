#include "looper_module.h"

#include "../Util/audio_utilities.h"

using namespace bkshepherd;

// 60 seconds at 48kHz
#define kBuffSize 48000 * 60

float DSY_SDRAM_BSS buffer[kBuffSize];
float DSY_SDRAM_BSS bufferR[kBuffSize];

static const char *s_loopModeNames[4] = {"Normal", "One-time", "Replace", "Fripp"};

static const char *s_loopSpeedMode[3] = {"None", "Stepped", "Smooth"};

static const int s_paramCount = 7;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {
        name : "Input Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : 14
    },
    {
        name : "Loop Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 15
    },
    {
        name : "Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 4,
        valueBinNames : s_loopModeNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 2,
        midiCCMapping : 16
    },
    {
        name : "SpeedMode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_loopSpeedMode,
        defaultValue : {.uint_value = 0},
        knobMapping : 3,
        midiCCMapping : 17
    },
    {
        name : "Speed",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 4,
        midiCCMapping : 18,
        minValue : -3,
        maxValue : 3
    },
    {
        name : "LP Filter",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 1.0f},
        knobMapping : 5,
        midiCCMapping : 19
    },
    {
        name : "MISO",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 20
    },
};

// Default Constructor
LooperModule::LooperModule()
    : BaseEffectModule(), m_inputLevelMin(0.0f), m_inputLevelMax(1.0f), m_loopLevelMin(0.0f), m_loopLevelMax(1.0f),
      m_toneFreqMin(120.0f), m_toneFreqMax(20000.0f) {
    // Set the name of the effect
    m_name = "Looper";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
LooperModule::~LooperModule() {
    // No Code Needed
}

void LooperModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    // Init the looper
    m_looper.Init(buffer, kBuffSize);
    m_looperR.Init(bufferR, kBuffSize);

    SetLooperMode();
    tone.Init(sample_rate);
    toneR.Init(sample_rate);
    currentSpeed = 1.0;
}

void LooperModule::SetLooperMode() {
    const int modeIndex = GetParameterAsBinnedValue(2) - 1;
    m_looper.SetMode(static_cast<daisysp::Looper::Mode>(modeIndex));
    m_looperR.SetMode(static_cast<daisysp::Looper::Mode>(modeIndex));
}

void LooperModule::ParameterChanged(int parameter_id) {
    if (parameter_id == 2) {
        SetLooperMode();
    }
}

void LooperModule::AlternateFootswitchPressed() {
    m_looper.TrigRecord();
    m_looperR.TrigRecord();
}

void LooperModule::AlternateFootswitchHeldFor1Second() {
    // clear the loop
    m_looper.Clear();
    m_looperR.Clear();
}

void LooperModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    const float inputLevel = m_inputLevelMin + (GetParameterAsFloat(0) * (m_inputLevelMax - m_inputLevelMin));

    const float loopLevel = m_loopLevelMin + (GetParameterAsFloat(1) * (m_loopLevelMax - m_loopLevelMin));

    float input = in * inputLevel;

    // Set low pass filter as exponential taper
    tone.SetFreq(m_toneFreqMin + GetParameterAsFloat(5) * GetParameterAsFloat(5) * (m_toneFreqMax - m_toneFreqMin));

    // Handle speed and direction changes smoothly (like a tape reel)
    // TODO maybe move out to only do this when speed param changes
    int speedModeIndex = GetParameterAsBinnedValue(3) - 1;

    if (speedModeIndex == 2) {
        float speed = GetParameterAsFloat(4);
        daisysp::fonepole(currentSpeed, speed, .00006f);
        if (currentSpeed < 0.0) {
            m_looper.SetReverse(true);
        } else {
            m_looper.SetReverse(false);
        }
        float speed_input_abs = abs(currentSpeed);
        m_looper.SetIncrementSize(speed_input_abs);

    } else if (speedModeIndex == 1) {
        float speed = GetParameterAsFloat(4) * 2;
        int temp_speed = speed;
        float ftemp_speed = temp_speed;
        float stepped_speed = ftemp_speed / 2;

        if (speed < 0.0) {
            m_looper.SetReverse(true);
        } else {
            m_looper.SetReverse(false);
        }
        float speed_input_abs = abs(stepped_speed);
        if (speed_input_abs < 0.5) {
            speed_input_abs = 0.5;
        }
        m_looper.SetIncrementSize(speed_input_abs);

    } else {
        m_looper.SetReverse(false);
        m_looper.SetIncrementSize(1.0);
    }
    /////////////////////////

    // store signal = loop signal * loop gain + in * in_gain
    float looperOutput = m_looper.Process(input) * loopLevel + input;
    float filter_out = tone.Process(looperOutput); // Apply tone Low Pass filter (useful to tame aliasing noise on variable speeds)

    m_audioRight = m_audioLeft = filter_out;
}

void LooperModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    // ProcessMono(inL);

    BaseEffectModule::ProcessStereo(inL, inR);

    const float inputLevel = m_inputLevelMin + (GetParameterAsFloat(0) * (m_inputLevelMax - m_inputLevelMin));

    const float loopLevel = m_loopLevelMin + (GetParameterAsFloat(1) * (m_loopLevelMax - m_loopLevelMin));

    float inputR = 0.0;
    float input = m_audioLeft * inputLevel;
    if (!GetParameterAsBool(6)) { // If "MISO" is on, copy left input to right, otherwise do true stereo
        inputR = m_audioRight * inputLevel;
    } else {
        inputR = input;
    }

    // Set low pass filter as exponential taper
    tone.SetFreq(m_toneFreqMin + GetParameterAsFloat(5) * GetParameterAsFloat(5) * (m_toneFreqMax - m_toneFreqMin));
    toneR.SetFreq(m_toneFreqMin + GetParameterAsFloat(5) * GetParameterAsFloat(5) * (m_toneFreqMax - m_toneFreqMin));

    // Handle speed and direction changes smoothly (like a tape reel)
    // TODO maybe move out to only do this when speed param changes
    int speedModeIndex = GetParameterAsBinnedValue(3) - 1;

    if (speedModeIndex == 2) {
        float speed = GetParameterAsFloat(4);
        daisysp::fonepole(currentSpeed, speed, .00006f);
        if (currentSpeed < 0.0) {
            m_looper.SetReverse(true);
            m_looperR.SetReverse(true);
        } else {
            m_looper.SetReverse(false);
            m_looperR.SetReverse(false);
        }
        float speed_input_abs = abs(currentSpeed);
        m_looper.SetIncrementSize(speed_input_abs);
        m_looperR.SetIncrementSize(speed_input_abs);

    } else if (speedModeIndex == 1) {
        float speed = GetParameterAsFloat(4) * 2;
        int temp_speed = speed;
        float ftemp_speed = temp_speed;
        float stepped_speed = ftemp_speed / 2;

        if (speed < 0.0) {
            m_looper.SetReverse(true);
            m_looperR.SetReverse(true);
        } else {
            m_looper.SetReverse(false);
            m_looperR.SetReverse(false);
        }
        float speed_input_abs = abs(stepped_speed);
        if (speed_input_abs < 0.5) {
            speed_input_abs = 0.5;
        }
        m_looper.SetIncrementSize(speed_input_abs);
        m_looperR.SetIncrementSize(speed_input_abs);

    } else {
        m_looper.SetReverse(false);
        m_looper.SetIncrementSize(1.0);
        m_looperR.SetReverse(false);
        m_looperR.SetIncrementSize(1.0);
    }
    /////////////////////////

    // store signal = loop signal * loop gain + in * in_gain
    float looperOutput = m_looper.Process(input) * loopLevel + input;
    float filter_out = tone.Process(looperOutput); // Apply tone Low Pass filter (useful to tame aliasing noise on variable speeds)

    float looperOutputR = m_looperR.Process(inputR) * loopLevel + inputR;
    float filter_outR = toneR.Process(looperOutputR); // Apply tone Low Pass filter (useful to tame aliasing noise on variable speeds)

    m_audioLeft = filter_out;
    m_audioRight = filter_outR;
}

float LooperModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        // Enable the LED if the looper is recording
        return value * m_looper.Recording();
    }

    return value;
}

void LooperModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                          bool isEditing) {
    BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);

    int width = boundsToDrawIn.GetWidth();

    if (m_looper.GetRecSize() > 0) {
        float percentageDone = 100.0 * (m_looper.GetPos() / m_looper.GetRecSize());
        int numBlocks = 20;
        int blockWidth = width / numBlocks;
        int top = 30;
        int x = 0;
        for (int block = 0; block < numBlocks; block++) {
            Rectangle r(x, top, blockWidth, blockWidth);

            bool active = false;
            if ((static_cast<float>(block) / static_cast<float>(numBlocks) * 100.0f) <= percentageDone) {
                active = true;
            }
            display.DrawRect(r, true, active);
            x += blockWidth;
        }

        char strbuff[64];
        if (m_looper.Recording()) {
            sprintf(strbuff, "R " FLT_FMT(1), FLT_VAR(1, percentageDone));
        } else {
            sprintf(strbuff, FLT_FMT(1), FLT_VAR(1, percentageDone));
        }
        display.WriteStringAligned(strbuff, Font_11x18, boundsToDrawIn, Alignment::bottomCentered, true);
    } else {
        char strbuff[64];
        sprintf(strbuff, "Empty");
        display.WriteStringAligned(strbuff, Font_11x18, boundsToDrawIn, Alignment::bottomCentered, true);
    }
}