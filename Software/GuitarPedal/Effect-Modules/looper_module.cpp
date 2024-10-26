#include "looper_module.h"

using namespace bkshepherd;

using namespace daisysp;

// 60 seconds at 48kHz
#define kBuffSize 48000 * 60

float DSY_SDRAM_BSS bufferL[kBuffSize];
float DSY_SDRAM_BSS bufferR[kBuffSize];

static const int s_paramCount = 2;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {
      name : "Input Level",
      valueType : ParameterValueType::FloatMagnitude,
      valueBinCount : 0,
      defaultValue : 64,
      knobMapping : 0,
      midiCCMapping : -1
    },
    {
      name : "Loop Level",
      valueType : ParameterValueType::FloatMagnitude,
      valueBinCount : 0,
      defaultValue : 64,
      knobMapping : 1,
      midiCCMapping : -1
    },
};

// Default Constructor
LooperModule::LooperModule()
    : BaseEffectModule(),
      m_inputLevelMin(0.0f),
      m_inputLevelMax(1.0f),
      m_loopLevelMin(0.0f),
      m_loopLevelMax(1.0f) {
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

  // Init the loopers
  m_looperL.Init(bufferL, kBuffSize);
  m_looperR.Init(bufferR, kBuffSize);
}

void LooperModule::AlternateFootswitchPressed() {
  m_looperL.TrigRecord();
  m_looperR.TrigRecord();
}

void LooperModule::AlternateFootswitchHeldFor1Second() {
  // clear the loop
  m_looperL.Clear();
  m_looperR.Clear();
}

void LooperModule::ProcessMono(float in) {
  const float inputLevel =
      m_inputLevelMin +
      (GetParameterAsMagnitude(0) * (m_inputLevelMax - m_inputLevelMin));

  const float loopLevel = m_loopLevelMin + (GetParameterAsMagnitude(1) *
                                            (m_loopLevelMax - m_loopLevelMin));

  float input = in * inputLevel;

  // store signal = loop signal * loop gain + in * in_gain
  float looperOutput = m_looperL.Process(input) * loopLevel + input;

  m_audioRight = m_audioLeft = looperOutput;
}

void LooperModule::ProcessStereo(float inL, float inR) {
  const float inputLevel =
      m_inputLevelMin +
      (GetParameterAsMagnitude(0) * (m_inputLevelMax - m_inputLevelMin));

  const float loopLevel = m_loopLevelMin + (GetParameterAsMagnitude(1) *
                                            (m_loopLevelMax - m_loopLevelMin));

  float inputL = inL * inputLevel;
  float inputR = inR * inputLevel;

  // store signal = loop signal * loop gain + in * in_gain
  float looperOutputL = m_looperL.Process(inputL) * loopLevel + inputL;
  float looperOutputR = m_looperR.Process(inputR) * loopLevel + inputR;

  m_audioLeft = looperOutputL;
  m_audioRight = looperOutputR;
}

float LooperModule::GetBrightnessForLED(int led_id) {
  float value = BaseEffectModule::GetBrightnessForLED(led_id);

  if (led_id == 1) {
    // Enable the LED if the looper is recording
    return value * m_looperL.Recording();
  }

  return value;
}

void LooperModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex,
                          int numItemsTotal, Rectangle boundsToDrawIn,
                          bool isEditing) {
  BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn,
                           isEditing);
  uint16_t center = boundsToDrawIn.GetHeight() / 2;

  // TODO: Use this when https://github.com/electro-smith/DaisySP/pull/205 is
  // merged

  // clang-format off
//    float percentageDone = 100.0 * (m_looperL.GetPos() / m_looperL.GetRecSize());
//    char strbuff[64];
//    if (m_looperL.Recording()) {
//      sprintf(strbuff, "R %.2f  ", percentageDone);
//    } else {
//      sprintf(strbuff, "  %.2f  ", percentageDone);
//    }
  // clang-format on

  char strbuff[64];
  if (m_looperL.Recording()) {
    sprintf(strbuff, "REC");
  } else {
    sprintf(strbuff, "PLAY");
  }

  display.WriteStringAligned(strbuff, Font_11x18, boundsToDrawIn,
                             Alignment::centered, true);
}