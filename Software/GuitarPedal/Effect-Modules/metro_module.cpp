#include "metro_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

void Metronome::Init(float freq, float sample_rate)
{
  freq_ = freq;
  phs_ = 0.0f;
  sample_rate_ = sample_rate;
  phs_inc_ = (TWOPI_F * freq_) / sample_rate_;
}

uint8_t Metronome::Process()
{
  phs_ += phs_inc_;
  if (phs_ >= TWOPI_F) {
    phs_ -= TWOPI_F;
    return 1;
  }
  return 0;
}

void Metronome::SetFreq(float freq)
{
  freq_ = freq;
  phs_inc_ = (TWOPI_F * freq_) / sample_rate_;
}

uint16_t Metronome::GetQuadrant()
{
  if (phs_ < PI_F / 2.0)
    return 0;
  else if (phs_ >= PI_F / 2.0 && phs_ < PI_F)
    return 1;
  else if (phs_ >= PI_F && phs_ < PI_F * 3.0 / 2.0)
    return 2;
  else
    return 3;
}

uint16_t Metronome::GetQuadrant16()
{
  float phase = phs_;
  if (phase > TWOPI_F)
    phase = TWOPI_F;

  uint16_t quadrant = (uint16_t)(phase * 16.0 / TWOPI_F);
  return quadrant;
}

static const int s_paramCount = 1;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {name : "Tempo", valueType : ParameterValueType::FloatMagnitude, valueBinCount : 0, defaultValue : 63, knobMapping : 0, midiCCMapping : 23}};

// Default Constructor
MetroModule::MetroModule() : BaseEffectModule(), m_tempoBpmMin(10), m_tempoBpmMax(200)
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
  m_quadrant = 0;
  m_direction = 0;

  const float freq = tempo_to_freq(DefaultTempoBpm);
  m_metro.Init(freq, sample_rate);
}

void MetroModule::Process()
{
  const int tempoRaw = GetParameterRaw(0);
  const uint16_t tempo = raw_tempo_to_bpm(tempoRaw);
  const float freq = tempo_to_freq(tempo);

  if (freq != m_metro.GetFreq())
    m_metro.SetFreq(freq);

  m_metro.Process();
  m_quadrant = m_metro.GetQuadrant16();
}

void MetroModule::ProcessMono(float in)
{
  BaseEffectModule::ProcessMono(in);
  Process();
  m_audioRight = m_audioLeft = in;
}

void MetroModule::ProcessStereo(float inL, float inR)
{
  BaseEffectModule::ProcessStereo(inL, inR);
  Process();
  m_audioRight = inL;
  m_audioRight = inR;
}

void MetroModule::SetTempo(uint32_t bpm)
{
  // TODO: need to adjust for min/max BPM
  float freq = tempo_to_freq(bpm);
  /*
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
  */
}

float MetroModule::GetBrightnessForLED(int led_id)
{
  float value = BaseEffectModule::GetBrightnessForLED(led_id);

  if (led_id == 1) {
    //  return value * m_cachedEffectMagnitudeValue;
  }

  return value;
}

uint16_t MetroModule::raw_tempo_to_bpm(uint8_t value) { return m_tempoBpmMin + (value * (m_tempoBpmMax - m_tempoBpmMin) / 127); }

void MetroModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn, bool isEditing)
{
  BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);

  // Show tempo in BPM
  char strbuff[128];
  int topRowHeight = boundsToDrawIn.GetHeight() / 2;
  int tempoRaw = GetParameterRaw(0);
  int tempo = raw_tempo_to_bpm(tempoRaw);
  /*
    sprintf(strbuff, "%d BPM", tempo);
    boundsToDrawIn.RemoveFromTop(topRowHeight);
    display.WriteStringAligned(strbuff, Font_11x18, boundsToDrawIn, Alignment::centered, true);
  */

  sprintf(strbuff, "%d", m_quadrant);

  boundsToDrawIn.RemoveFromTop(topRowHeight);
  display.WriteStringAligned(strbuff, Font_11x18, boundsToDrawIn, Alignment::centered, true);

  // Show metronome indicator
  int pos_inc = boundsToDrawIn.GetWidth() / 16;
  uint16_t position = m_direction == 0 ? m_quadrant * pos_inc : -(m_quadrant - 15) * pos_inc;

  Rectangle r(position, topRowHeight - 5, pos_inc - 1, 10);
  // Rectangle r(-(m_quadrant - 15) * pos_inc, topRowHeight - 5, pos_inc - 1, 10);
  display.DrawRect(r, true, (m_quadrant % 4) == 0);

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