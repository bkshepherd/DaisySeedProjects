#include "metro_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

static const int s_paramCount = 1;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {name : "Tempo", valueType : ParameterValueType::FloatMagnitude, valueBinCount : 0, defaultValue : 63, knobMapping : 0, midiCCMapping : 23}};

// Default Constructor
MetroModule::MetroModule() : BaseEffectModule(), m_tempoFreqMin(0.5f), m_tempoFreqMax(4.0f), m_cachedEffectMagnitudeValue(1.0f)

{
  // Set the name of the effect
  m_name = "Metronome";

  // Setup the meta data reference for this Effect
  m_paramMetaData = s_metaData;

  // Initialize Parameters for this Effect
  this->InitParams(s_paramCount);
}

// Destructor
MetroModule::~MetroModule()
{
  // No Code Needed
}

void MetroModule::Init(float sample_rate)
{
  BaseEffectModule::Init(sample_rate);

  // m_chopper.Init(sample_rate);
}

void MetroModule::ProcessMono(float in)
{
  BaseEffectModule::ProcessMono(in);
  m_audioRight = m_audioLeft = in;
}

void MetroModule::ProcessStereo(float inL, float inR)
{
  m_audioRight = inL;
  m_audioRight = inR;
}

void MetroModule::SetTempo(uint32_t bpm)
{
  float freq = tempo_to_freq(bpm);

  // Adjust the frequency into a range that makes sense for the effect
  freq = freq / 4.0f;

  if (freq <= m_tempoFreqMin) {
    SetParameterRaw(1, 0);
  } else if (freq >= m_tempoFreqMax) {
    SetParameterRaw(1, 127);
  } else {
    // Get the parameter as close as we can to target tempo
    SetParameterRaw(1, ((freq - m_tempoFreqMin) / (m_tempoFreqMax - m_tempoFreqMin)) * 128);
  }
}

float MetroModule::GetBrightnessForLED(int led_id)
{
  float value = BaseEffectModule::GetBrightnessForLED(led_id);

  if (led_id == 1) {
    return value * m_cachedEffectMagnitudeValue;
  }

  return value;
}

void MetroModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn, bool isEditing)
{
  BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);
  /*

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
  */
}