#include "pitch_shifter_module.h"

#include <algorithm>

#include "../Util/pitch_shifter.h"
#include "daisysp.h"

using namespace bkshepherd;

static const char *s_semitoneBinNames[8] = {"1", "2", "3", "4",
                                            "5", "6", "7", "OCT"};
static const char *s_directionBinNames[2] = {"DOWN", "UP"};
static const char *s_modeBinNames[2] = {"LATCH", "MOMENT"};

// How many samples to delay to based on the "Delay" knob and parameter
// when the time knob is set to max, this is used for the ramp up/down
// transition when in momentary mode. Has no effect on latching mode
const uint32_t k_maxSamplesMaxTime = 48000 * 2;

static const int s_paramCount = 5;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {
      name : "Semitone",
      valueType : ParameterValueType::Binned,
      valueBinCount : 8,
      valueBinNames : s_semitoneBinNames,
      defaultValue : 0,
      knobMapping : 0,
      midiCCMapping : -1
    },
    {
      name : "Crossfade",
      valueType : ParameterValueType::FloatMagnitude,
      valueBinCount : 0,
      defaultValue : 127,
      knobMapping : 1,
      midiCCMapping : -1
    },
    {
      name : "Direction",
      valueType : ParameterValueType::Binned,
      valueBinCount : 2,
      valueBinNames : s_directionBinNames,
      defaultValue : 0,
      knobMapping : 2,
      midiCCMapping : -1
    },
    {
      name : "Mode",
      valueType : ParameterValueType::Binned,
      valueBinCount : 2,
      valueBinNames : s_modeBinNames,
      defaultValue : 0,
      knobMapping : 3,
      midiCCMapping : -1
    },
    {
      name : "Delay",
      valueType : ParameterValueType::FloatMagnitude,
      valueBinCount : 0,
      defaultValue : 0,
      knobMapping : 4,
      midiCCMapping : -1
    },
};

// Fixed loud noise at startup (First time after power cycle) by NOT
// putting this in DSY_SDRAM_BSS
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

void PitchShifterModule::Init(float sample_rate) {
  BaseEffectModule::Init(sample_rate);

  pitchShifter.Init(sample_rate);

  pitchCrossfade.Init(CROSSFADE_CPOW);
  pitchCrossfade.SetPos(GetParameterAsMagnitude(1));

  m_latching = GetParameterAsBinnedValue(3) == 1;

  m_directionDown = GetParameterAsBinnedValue(2) == 1;

  ProcessSemitoneTargetChange();

  pitchShifter.SetTransposition(m_semitoneTarget);

  m_delayValue = GetParameterAsMagnitude(4);
}

void PitchShifterModule::ParameterChanged(int parameter_id) {
  if (parameter_id == 0 || parameter_id == 2) {
    m_directionDown = GetParameterAsBinnedValue(2) == 1;

    // Change semitone when semitone knob is turned or direction is changed
    ProcessSemitoneTargetChange();
  } else if (parameter_id == 1) {
    pitchCrossfade.SetPos(GetParameterAsMagnitude(1));
  } else if (parameter_id == 3) {
    m_latching = GetParameterAsBinnedValue(3) == 1;
  } else if (parameter_id == 4) {
    m_delayValue = GetParameterAsMagnitude(4);
  }

  // Parameters changed, reset the transposition target just in case (mostly
  // impacts momentary/latch and delay)
  pitchShifter.SetTransposition(m_semitoneTarget);
}

void PitchShifterModule::AlternateFootswitchPressed() {
  m_alternateFootswitchPressed = true;

  if (!m_latching && m_sampleCounter == 0) {
    // Initiate the ramp up transition for momentary mode
    m_sampleCounter += 1;
  }
}

void PitchShifterModule::AlternateFootswitchReleased() {
  m_alternateFootswitchPressed = false;

  if (!m_latching && m_sampleCounter > 0) {
    // Initiate the ramp down transition for momentary mode
    m_sampleCounter -= 1;
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

void PitchShifterModule::ProcessStereo(float inL, float inR) {
  ProcessMono(inL);
}

float clamp(float v, float min, float max) {
  return std::min(max, std::max(min, v));
}

float PitchShifterModule::ProcessMomentaryMode(float in) {
  // When in momentary mode, there is a ramp up(pressed)/ramp down(released)
  // transition of the semitone based on the "delay" parameter
  const uint32_t samplesToDelay = static_cast<uint32_t>(
      static_cast<float>(k_maxSamplesMaxTime) * m_delayValue);

  // Are we still in the middle or starting a "ramp up/ramp down
  // period" where we are transitioning to/from a target semitone
  const bool transitioning = m_sampleCounter > 0 && samplesToDelay > 0;

  // ---- Process when NOT in a ramp up/ramp down state ----
  if (!transitioning) {
    if (m_alternateFootswitchPressed) {
      // If the footswitch IS pressed, we maintain the m_sampleCounter value
      // where it was to use for the ramp down

      // Process the pitch shift for completely active to the target
      pitchShifter.SetTransposition(m_semitoneTarget);
      float shifted = pitchShifter.Process(in);
      float out = pitchCrossfade.Process(in, shifted);
      return out;
    } else {
      // If the footswitch isn't pressed, reset values for next ramp up
      // Make sure the crossfader is initialized and the sample counter is
      // ready for the next ramp up transition time
      m_sampleCounter = 0;

      // Return the input directly since the switch isn't pressed and we aren't
      // ramping down

      // Process the pitch shift for completely inactive (0)
      pitchShifter.SetTransposition(0);
      float shifted = pitchShifter.Process(in);
      float out = pitchCrossfade.Process(in, shifted);
      return out;
    }
  }

  // ---- Process ramp up/ramp down transition ----

  // Clamp just to make sure we don't overshoot the semitone just in case
  m_percentageTransitionComplete = clamp(
      static_cast<float>(m_sampleCounter) / static_cast<float>(samplesToDelay),
      0.0f, 1.0f);

  // Perform the pitch shift
  pitchShifter.SetTransposition(m_semitoneTarget *
                                m_percentageTransitionComplete);
  float shifted = pitchShifter.Process(in);
  float pitchOut = pitchCrossfade.Process(in, shifted);

  const bool transitioningTowardsSemitoneTarget = m_alternateFootswitchPressed;

  // Increment or decrement the counter for the next pass through based on if
  // we are ramping up or ramping down and complete or not
  if (transitioningTowardsSemitoneTarget && m_sampleCounter < samplesToDelay) {
    m_sampleCounter += 1;
  } else if (!transitioningTowardsSemitoneTarget && m_sampleCounter > 0) {
    m_sampleCounter -= 1;
  }

  return pitchOut;
}

void PitchShifterModule::DrawUI(OneBitGraphicsDisplay &display,
                                int currentIndex, int numItemsTotal,
                                Rectangle boundsToDrawIn, bool isEditing) {
  BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn,
                           isEditing);

  // Only add a UI for when we are in momentary mode
  if (m_latching) {
    return;
  }

  int width = boundsToDrawIn.GetWidth();
  int numBlocks = 20;
  int blockWidth = width / numBlocks;
  int top = 30;
  int x = 0;
  const bool transitioning = m_sampleCounter > 0 && m_delayValue > 0;
  for (int block = 0; block < numBlocks; block++) {
    Rectangle r(x, top, blockWidth, blockWidth);

    bool active = false;
    if (transitioning) {
      if ((static_cast<float>(block) / static_cast<float>(numBlocks)) <=
          m_percentageTransitionComplete) {
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