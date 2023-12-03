#pragma once
#ifndef METRO_MODULE_H
#define METRO_MODULE_H

#include "daisysp.h"
#include "base_effect_module.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file chopper_module.h */

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

  uint16_t GetQuadrant();

private:
  float freq_;
  float phs_, sample_rate_, phs_inc_;
};

const uint16_t DefaultTempoBpm = 120;

class MetroModule : public BaseEffectModule
{
public:
  MetroModule();
  ~MetroModule();

  void Init(float sample_rate) override;
  void ProcessMono(float in) override;
  void ProcessStereo(float inL, float inR) override;
  void Process();

  void SetTempo(uint32_t bpm) override;
  float GetBrightnessForLED(int led_id) override;
  void DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn, bool isEditing) override;

private:
  uint16_t m_tempoBpmMin;
  uint16_t m_tempoBpmMax;
  uint16_t m_quadrant;

  Metronome m_metro;

  uint16_t raw_tempo_to_bpm(uint8_t value);
};
} // namespace bkshepherd
#endif
#endif
