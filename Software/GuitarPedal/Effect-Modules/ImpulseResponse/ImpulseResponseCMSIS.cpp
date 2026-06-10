#include "ImpulseResponseCMSIS.h"

#include <cmath>

ImpulseResponseCMSIS::ImpulseResponseCMSIS() : position(0), firLength(DEFAULT_FIR_LEN) {}

void ImpulseResponseCMSIS::init(const float* ir, uint32_t len, bool normalize) {
  arm_fill_f32(0.0f, fir_state_, FIR_STATE_LEN);
  setImpulseResponse(ir, len, normalize);
}

// Optional: set this to 0.0 for 100% wet, 1.0 for 100% dry, or in between
constexpr float kDryWet = 0.0f;

float ImpulseResponseCMSIS::process(float x) {
  float y = x;
  arm_fir_f32(&fir_, &y, &y, 1u);
  return (1.0f - kDryWet) * y + kDryWet * x;
}

void ImpulseResponseCMSIS::processBlock(float* in, float* out, uint32_t n) {
  if (n > MAX_BLOCK) {
    n = MAX_BLOCK;
  }
  arm_fir_f32(&fir_, in, out, n);

  if constexpr (kDryWet != 0.0f) {
    for (uint32_t i = 0; i < n; ++i) {
      out[i] = (1.0f - kDryWet) * out[i] + kDryWet * in[i];
    }
  }
}

void ImpulseResponseCMSIS::setImpulseResponse(const float* ir, uint32_t len,
                                         bool norm) {
  if (len > DEFAULT_FIR_LEN) {
    len = DEFAULT_FIR_LEN;
  }
  for (uint32_t i = 0; i < len; ++i) {
    fir_coeffs_[i] = ir[i];
  }
  if (norm) {
    normalise(fir_coeffs_, len);
  }
  arm_fill_f32(0.0f, fir_state_, FIR_STATE_LEN);  // Reset state
  arm_fir_init_f32(&fir_, len, fir_coeffs_, fir_state_, MAX_BLOCK);
}

void ImpulseResponseCMSIS::normalise(float* c, uint32_t len) {
  float energy = 0.0f;
  for (uint32_t i = 0; i < len; ++i) {
    energy += c[i] * c[i];
  }

  if (energy > 1e-12f) {
    float k = 1.0f / sqrtf(energy);  // unit-energy
    arm_scale_f32(c, k, c, len);
  }
}