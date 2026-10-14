// Host stand-in for the bits of CMSIS-DSP used by ImpulseResponse: a plain streaming FIR with CMSIS state semantics
// (coefficients stored time-reversed, state holds the previous numTaps-1 inputs).
#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstddef>
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

// Real FFT with CMSIS-DSP's arm_rfft_fast_f32 interface and data layout (used by Venus): a plain double-precision radix-2 FFT. The firmware uses the real
// library; tests/host/test_cmsis_fft.cpp checks the real library (built with the firmware's table configuration) against ShyFFT through the same adapter.
//   forward: p = N real samples (CMSIS destroys it, this does not)  ->  pOut = [Re0, ReNyquist, Re1, Im1, Re2, Im2, ... Re(N/2-1), Im(N/2-1)]
//   inverse: p = that packing  ->  pOut = N real samples, normalised by 1/N
typedef int arm_status;
enum { ARM_MATH_SUCCESS = 0, ARM_MATH_ARGUMENT_ERROR = -1 };
typedef struct { uint16_t fftLenRFFT; } arm_rfft_fast_instance_f32;
inline arm_status arm_rfft_fast_init_f32(arm_rfft_fast_instance_f32 *S, uint16_t len) { S->fftLenRFFT = len; return ARM_MATH_SUCCESS; }
inline void arm_rfft_fast_f32(const arm_rfft_fast_instance_f32 *S, float *p, float *pOut, uint8_t ifft) {
    const size_t N = S->fftLenRFFT;
    static std::vector<double> cs, sn; // twiddles for the last length used
    if (cs.size() != N / 2) {
        cs.resize(N / 2); sn.resize(N / 2);
        for (size_t k = 0; k < N / 2; k++) { cs[k] = std::cos(2.0 * 3.14159265358979323846 * k / N); sn[k] = std::sin(2.0 * 3.14159265358979323846 * k / N); }
    }
    std::vector<double> re(N), im(N);
    if (!ifft) {
        for (size_t i = 0; i < N; i++) { re[i] = p[i]; im[i] = 0; }
    } else { // rebuild the full conjugate-symmetric spectrum
        re[0] = p[0]; im[0] = 0; re[N / 2] = p[1]; im[N / 2] = 0;
        for (size_t k = 1; k < N / 2; k++) { re[k] = re[N - k] = p[2 * k]; im[k] = p[2 * k + 1]; im[N - k] = -p[2 * k + 1]; }
    }
    // iterative radix-2 FFT (forward: e^-i, inverse: e^+i)
    for (size_t i = 1, j = 0; i < N; i++) {
        size_t bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (size_t len = 2; len <= N; len <<= 1) {
        const size_t step = N / len;
        for (size_t i = 0; i < N; i += len)
            for (size_t k = 0; k < len / 2; k++) {
                const double wr = cs[k * step], wi = (ifft ? 1.0 : -1.0) * sn[k * step];
                const size_t a = i + k, b = i + k + len / 2;
                const double tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
            }
    }
    if (!ifft) {
        pOut[0] = (float)re[0]; pOut[1] = (float)re[N / 2];
        for (size_t k = 1; k < N / 2; k++) { pOut[2 * k] = (float)re[k]; pOut[2 * k + 1] = (float)im[k]; }
    } else {
        for (size_t i = 0; i < N; i++) pOut[i] = (float)(re[i] / (double)N);
    }
}
