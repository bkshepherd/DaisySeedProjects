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

static const char *TimeSignatureLabels[3] = {"4/4", "3/4", "2/4"};
static const uint16_t TimeSignatureBase[3] = {4, 3, 2};

static const int s_paramCount = 3;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {name : "Tempo", valueType : ParameterValueType::FloatMagnitude, valueBinCount : 0, defaultValue : 63, knobMapping : 0, midiCCMapping : 23},
    {name : "Mix", valueType : ParameterValueType::FloatMagnitude, valueBinCount : 0, defaultValue : 10, knobMapping : 1, midiCCMapping : 21},
    {name : "Meter", valueType : ParameterValueType::Binned, valueBinCount : 3,  valueBinNames: TimeSignatureLabels, defaultValue : 0, knobMapping : 2, midiCCMapping : -1}};

// Default Constructor
MetroModule::MetroModule() : BaseEffectModule(), m_tempoBpmMin(40), m_tempoBpmMax(200), m_levelMin(0.0f), m_levelMax(1.0f)
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
  m_beat = 0;
  m_timeSignature = TimeSignature::meter4x4;

  // set oscillator
  m_osc.Init(sample_rate);
  m_osc.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
  m_osc.SetFreq(440.0f);
  m_osc.SetAmp(0.02f);

  // Set envelope
  m_env.Init(sample_rate);
  m_env.SetTime(ADSR_SEG_ATTACK, .1);
  m_env.SetTime(ADSR_SEG_DECAY, .2);
  m_env.SetTime(ADSR_SEG_RELEASE, .01);
  m_env.SetSustainLevel(.5);

  // Set metronome
  const float freq = tempo_to_freq(DefaultTempoBpm);
  m_metro.Init(freq, sample_rate);
}

float MetroModule::Process()
{
  const int tempoRaw = GetParameterRaw(0);
  const uint16_t tempo = raw_tempo_to_bpm(tempoRaw);
  const float freq = tempo_to_freq(tempo);

  uint16_t tsig = GetParameterAsBinnedValue(2) - 1;
  if (tsig != m_timeSignature)
    m_timeSignature = static_cast<TimeSignature>(tsig);

  if (freq != m_metro.GetFreq())
    m_metro.SetFreq(freq);

  if (m_metro.Process()) {
    m_beat++;
    m_osc.SetFreq((m_beat % (TimeSignatureBase[static_cast<uint16_t>(m_timeSignature)]) == 0) ? 440.0f : 220.0f);
    m_direction = !m_direction;
  }

  m_quadrant = m_metro.GetQuadrant16();

  float sig = m_osc.Process();
  float env_out = m_env.Process(m_quadrant < 2);
  return sig * env_out;
}

void MetroModule::ProcessMono(float in)
{
  BaseEffectModule::ProcessMono(in);
  float sig = Process();
  // Adjust the level
  float level = (m_levelMin + (GetParameterAsMagnitude(1) * (m_levelMax - m_levelMin)));
  m_audioLeft = sig * level + in * (1.0f - level);
  m_audioRight = m_audioLeft;
}

void MetroModule::ProcessStereo(float inL, float inR)
{
  BaseEffectModule::ProcessStereo(inL, inR);
  float sig = Process();
  // Adjust the level
  float level = (m_levelMin + (GetParameterAsMagnitude(1) * (m_levelMax - m_levelMin)));
  m_audioLeft = sig * level + inL * (1.0f - level);
  m_audioRight = sig * level + inR * (1.0f - level);
}

void MetroModule::SetTempo(uint32_t bpm) { SetParameterRaw(1, bpm_tempo_to_raw(bpm)); }

float MetroModule::GetBrightnessForLED(int led_id)
{
  float value = BaseEffectModule::GetBrightnessForLED(led_id);

  if (led_id == 1) {
    return 1.0f - (m_quadrant / 16.0);
  }

  return value;
}

uint16_t MetroModule::raw_tempo_to_bpm(uint8_t value) { return m_tempoBpmMin + (value * (m_tempoBpmMax - m_tempoBpmMin) / 127); }

uint8_t MetroModule::bpm_tempo_to_raw(uint16_t bpm)
{
  if (bpm > m_tempoBpmMax)
    bpm = m_tempoBpmMax;
  else if (bpm < m_tempoBpmMin)
    bpm = m_tempoBpmMin;

  float normalized = (bpm - m_tempoBpmMin) / (m_tempoBpmMax - m_tempoBpmMin);
  uint8_t raw = normalized * 127;
  return raw;
}

void MetroModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn, bool isEditing)
{
  BaseEffectModule::DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);

  // Show tempo in BPM
  char strbuff[64];
  int topRowHeight = boundsToDrawIn.GetHeight() / 2;
  int tempoRaw = GetParameterRaw(0);
  int tempo = raw_tempo_to_bpm(tempoRaw);

  sprintf(strbuff, "%s %d BPM", TimeSignatureLabels[static_cast<uint16_t>(m_timeSignature)], tempo);
  boundsToDrawIn.RemoveFromTop(topRowHeight);
  display.WriteStringAligned(strbuff, Font_11x18, boundsToDrawIn, Alignment::centered, true);

  /*
    sprintf(strbuff, " %d ", m_quadrant);
    boundsToDrawIn.RemoveFromTop(topRowHeight);
    display.WriteStringAligned(strbuff, Font_11x18, boundsToDrawIn, Alignment::centered, true);
  */

  // Show metronome indicator
  int pos_inc = boundsToDrawIn.GetWidth() / 16;
  uint16_t position = m_direction == 0 ? m_quadrant * pos_inc : -(m_quadrant - 15) * pos_inc;

  Rectangle r(position, topRowHeight - 7, pos_inc - 1, 10);
  // display.DrawRect(r, true, (m_quadrant % 4) == 0);
  display.DrawRect(r, true, true);
}