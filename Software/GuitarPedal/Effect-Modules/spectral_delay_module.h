#pragma once
#ifndef SPECTRAL_DELAY_MODULE_H
#define SPECTRAL_DELAY_MODULE_H

#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>

// clang-format off
#include "../Util/STFT/shy_fft.h"
#include "../Util/STFT/fourier.h"
#include "../Util/STFT/wave.h"
// clang-format on
#include <cmath>
#include <complex>

using namespace bkshepherd;
using namespace soundmath;

#define PI 3.1415926535897932384626433832795

#ifdef __cplusplus

/** @file spectral_delay_module.h */

// NOTES: During testing, DTCRAM overflowing was an issue. Removing Stereo capability helped (i.e. removing one of the stft's).
//  The Hann and HalfHann lookup tables are stored in DTCRAM. HalfHann is not used, so this is commented out.

using namespace daisysp;

namespace bkshepherd {

class SpectralDelayModule : public BaseEffectModule {
  public:
    SpectralDelayModule();
    ~SpectralDelayModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;

  private:
    float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
