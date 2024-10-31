// This is a copy of the phasor from DaisySP with this PR applied:
// https://github.com/electro-smith/DaisySP/pull/166
// This file can be deleted if that PR is merged and the submodule is updated

#pragma once
#ifndef PHASOR_H
#define PHASOR_H
#ifdef __cplusplus

#define PI_F 3.1415927410125732421875f
#define TWOPI_F (2.0f * PI_F)

namespace daisysp_modified {
/** Generates a normalized signal moving from 0-1 at the specified frequency.

\todo Selecting which channels should be initialized/included in the sequence
conversion. \todo Setup a similar start function for an external mux, but that
seems outside the scope of this file.

*/
class Phasor {
 public:
  Phasor() {}
  ~Phasor() {}
  /** Initializes the Phasor module
  sample rate, and freq are in Hz
  initial phase is in radians
  Additional Init functions have defaults when arg is not specified:
  - phs = 0.0f
  - freq = 1.0f
  */
  inline void Init(float sample_rate, float freq, float initial_phase) {
    sample_rate_ = sample_rate;
    phs_ = initial_phase;
    SetFreq(freq);
  }

  /** Initialize phasor with samplerate and freq
   */
  inline void Init(float sample_rate, float freq) {
    Init(sample_rate, freq, 0.0f);
  }

  /** Initialize phasor with samplerate
   */
  inline void Init(float sample_rate) { Init(sample_rate, 1.0f, 0.0f); }
  /** processes Phasor and returns current value
   */
  float Process() {
    float out;
    out = phs_ / TWOPI_F;
    phs_ += inc_;
    if (phs_ > TWOPI_F) {
      phs_ -= TWOPI_F;
    }
    if (phs_ < 0.0f) {
      phs_ += TWOPI_F;
    }
    return out;
  }

  /** Sets frequency of the Phasor in Hz
   */
  void SetFreq(float freq) {
    freq_ = freq;
    inc_ = (TWOPI_F * freq_) / sample_rate_;
  }

  /** Returns current frequency value in Hz
   */
  inline float GetFreq() { return freq_; }

 private:
  float freq_;
  float sample_rate_, inc_, phs_;
};
}  // namespace daisysp_modified
#endif
#endif
