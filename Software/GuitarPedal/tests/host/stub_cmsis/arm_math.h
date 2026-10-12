// Host stand-in for the bits of CMSIS-DSP used by ImpulseResponse: a plain streaming FIR with CMSIS state semantics
// (coefficients stored time-reversed, state holds the previous numTaps-1 inputs).
#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
typedef float float32_t;
typedef struct { uint16_t numTaps; float *pState; const float *pCoeffs; } arm_fir_instance_f32;
inline void arm_fir_init_f32(arm_fir_instance_f32 *S, uint16_t numTaps, const float *c, float *state, uint32_t blockSize) {
    S->numTaps = numTaps; S->pCoeffs = c; S->pState = state; std::memset(state, 0, sizeof(float) * (numTaps + blockSize - 1));
}
inline void arm_fir_f32(const arm_fir_instance_f32 *S, const float *in, float *out, uint32_t n) {
    const uint32_t T = S->numTaps; float *st = S->pState;
    for (uint32_t i = 0; i < n; i++) {
        st[T - 1 + i] = in[i];
        float acc = 0.0f;
        for (uint32_t k = 0; k < T; k++) acc += S->pCoeffs[k] * st[i + k];
        out[i] = acc;
    }
    std::memmove(st, st + n, sizeof(float) * (T - 1));
}
inline void arm_fill_f32(float v, float *p, uint32_t n) { for (uint32_t i = 0; i < n; i++) p[i] = v; }
inline void arm_scale_f32(const float *s, float k, float *d, uint32_t n) { for (uint32_t i = 0; i < n; i++) d[i] = s[i] * k; }
inline void arm_power_f32(const float *s, uint32_t n, float *r) { float a = 0; for (uint32_t i = 0; i < n; i++) a += s[i] * s[i]; *r = a; }
inline void arm_rms_f32(const float *s, uint32_t n, float *r) { float a = 0; for (uint32_t i = 0; i < n; i++) a += s[i] * s[i]; *r = std::sqrt(a / n); }
