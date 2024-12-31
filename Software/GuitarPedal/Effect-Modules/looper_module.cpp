#include "looper_module.h"

#include "../Util/audio_utilities.h"

using namespace bkshepherd;

// 60 seconds at 48kHz
#define kBuffSize 48000 * 60

float DSY_SDRAM_BSS buffer[kBuffSize];

static const char *s_loopModeNames[4] = {"Normal", "One-time", "Replace", "Fripp"};

static const int s_paramCount = 3;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {
        name : "Input Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : -1
    },
    {
        name : "Loop Level",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : -1
    },
    {
        name : "Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 4,
        valueBinNames : s_loopModeNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 2,
        midiCCMapping : -1
    },
};

// Default Constructor
LooperModule::LooperModule()
    : BaseEffectModule(), m_inputLevelMin(0.0f), m_inputLevelMax(1.0f), m_loopLevelMin(0.0f), m_loopLevelMax(1.0f) {
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

    SetLooperMode();
}

void LooperModule::SetLooperMode() {
    const int modeIndex = GetParameterAsBinnedValue(2) - 1;
    m_looper.SetMode(static_cast<daisysp_modified::Looper::Mode>(modeIndex));
}

void LooperModule::ParameterChanged(int parameter_id) {
    if (parameter_id == 2) {
        SetLooperMode();
    }
}

void LooperModule::AlternateFootswitchPressed() { m_looper.TrigRecord(); }

void LooperModule::AlternateFootswitchHeldFor1Second() {
    // clear the loop
    m_looper.Clear();
}

void LooperModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    const float inputLevel = m_inputLevelMin + (GetParameterAsFloat(0) * (m_inputLevelMax - m_inputLevelMin));

    const float loopLevel = m_loopLevelMin + (GetParameterAsFloat(1) * (m_loopLevelMax - m_loopLevelMin));

    float input = in * inputLevel;

    // store signal = loop signal * loop gain + in * in_gain
    float looperOutput = m_looper.Process(input) * loopLevel + input;

    m_audioRight = m_audioLeft = looperOutput;
}

void LooperModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);
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