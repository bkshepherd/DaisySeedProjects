#pragma once
#ifndef METRO_MODULE_H
#define METRO_MODULE_H

#include "daisysp.h"
#include "base_effect_module.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file metro_module.h */

using namespace daisysp;

namespace bkshepherd
{
class Metronome
{
public:
  Metronome() {}
  ~Metronome() {}
  /** Initializes Metronome module.
      Arguments:
      - freq: frequency at which new clock signals will be generated
          Input Range:
      - sample_rate: sample rate of audio engine
          Input range:
  */
  void Init(float freq, float sample_rate);

  /** checks current state of Metronome object and updates state if necesary.
   */
  uint8_t Process();

  /** resets phase to 0
   */
  inline void Reset() { phs_ = 0.0f; }
  /** Sets frequency at which Metronome module will run at.
   */
  void SetFreq(float freq);

  /** Returns current value for frequency.
   */
  inline float GetFreq() { return freq_; }

  /** Returns current phase.
   */
  inline float GetPhase() { return phs_; }

  /** Returns the phase quadrant (0-4)
   */
  uint16_t GetQuadrant();

  /** Returns the phase quadrant (0-16)
   */
  uint16_t GetQuadrant16();

private:
  float freq_;
  float phs_, sample_rate_, phs_inc_;
};

enum TimeSignature { meter4x4 = 0, meter3x4 = 1, meter2x4 = 2 };
const uint16_t DefaultTempoBpm = 120;

class MetroModule : public BaseEffectModule
{
public:
  MetroModule();
  ~MetroModule();

  void Init(float sample_rate) override;
  void ProcessMono(float in) override;
  void ProcessStereo(float inL, float inR) override;
  float Process();

  void ParameterChanged(int parameter_id) override;
  void SetTempo(uint32_t bpm) override;
  float GetBrightnessForLED(int led_id) override;
  void DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn, bool isEditing) override;

  inline void SetTimeSignature(TimeSignature ts) { m_timeSignature = ts; }
  inline TimeSignature GetTimeSignature() { return m_timeSignature; }

private:
  float m_levelMin;
  float m_levelMax;

  uint16_t m_quadrant;
  uint16_t m_direction;
  uint32_t m_beat;

  TimeSignature m_timeSignature;

  Metronome m_metro;

  daisysp::Oscillator m_osc;
  daisysp::Adsr m_env;

  uint32_t m_bpm;
};
} // namespace bkshepherd
#endif
#endif
