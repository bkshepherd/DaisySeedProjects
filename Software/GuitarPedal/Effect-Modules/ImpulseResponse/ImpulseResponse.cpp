#include "ImpulseResponse.h"

#include <cmath>

ImpulseResponse::ImpulseResponse() {}

void ImpulseResponse::init(const float *ir, uint32_t len, bool normalize) {
    arm_fill_f32(0.0f, fir_state_, FIR_STATE_LEN);
    setImpulseResponse(ir, len, normalize);
}

// Optional: set this to 0.0 for 100% wet, 1.0 for 100% dry, or in between
constexpr float kDryWet = 0.0f;

void ImpulseResponse::processBlock(const float *in, float *out, uint32_t n) {
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

void ImpulseResponse::setImpulseResponse(const float *ir, uint32_t len, bool norm) {
    if (len > DEFAULT_FIR_LEN) {
        len = DEFAULT_FIR_LEN;
    }
    // arm_fir_f32 expects coefficients in time-reversed order
    // {b[numTaps-1], ..., b[0]}, so flip the IR here to get convolution
    // rather than correlation.
    for (uint32_t i = 0; i < len; ++i) {
        fir_coeffs_[i] = ir[len - 1u - i];
    }
    if (norm) {
        normalise(fir_coeffs_, len);
    }
    arm_fill_f32(0.0f, fir_state_, FIR_STATE_LEN); // Reset state
    arm_fir_init_f32(&fir_, len, fir_coeffs_, fir_state_, MAX_BLOCK);
}

void ImpulseResponse::normalise(float *c, uint32_t len) {
    float energy = 0.0f;
    for (uint32_t i = 0; i < len; ++i) {
        energy += c[i] * c[i];
    }

    if (energy > 1e-12f) {
        float k = 1.0f / sqrtf(energy); // unit-energy
        arm_scale_f32(c, k, c, len);
    }
}