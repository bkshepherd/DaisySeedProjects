#pragma once
// Small DSP pieces shared by the distortion family (preamp, screamer, muff fuzz, wavefolder, exciter, bass distortion, octave fuzz, tape saturation).
// Everything is cheap on purpose: first-order filters, RBJ biquads, and clippers with first-order antiderivative anti-aliasing (ADAA), which
// removes most of the aliasing of a hard nonlinearity at the cost of one division per sample (no oversampling needed).
#include <cmath>

namespace distdsp {

constexpr float kPi = 3.14159265358979f;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }

/** Extra gain for the top of a Drive knob: 1 up to the knob position `from`, rising smoothly (zero slope at `from`) to 1 + extra at the end of
 *  the knob. The gain at and below the default position, and with it the calibrated level, is unchanged; the top of the knob reaches full
 *  saturation (about 35 dB of compression) with a hot input. */
inline float topBoost(float v, float from, float extra) {
    if (v <= from) return 1.0f;
    const float t = (v - from) / (1.0f - from);
    return 1.0f + extra * t * t;
}

/** A first-order low-pass; HP(x) = x - LP(x) (a first-order high-pass with the same corner). */
struct OnePole {
    float a = 0.1f;
    float z = 0.0f;
    void SetCutoff(float fc, float sampleRate) {
        fc = clampf(fc, 1.0f, 0.45f * sampleRate);
        a = 1.0f - std::exp(-2.0f * kPi * fc / sampleRate);
    }
    float LP(float x) {
        z += a * (x - z);
        return z;
    }
    float HP(float x) { return x - LP(x); }
    void Reset() { z = 0.0f; }
};

/** An RBJ (Audio EQ Cookbook) biquad, transposed direct form II. */
struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void Set(float nb0, float nb1, float nb2, float na0, float na1, float na2) {
        const float inv = 1.0f / na0;
        b0 = nb0 * inv; b1 = nb1 * inv; b2 = nb2 * inv;
        a1 = na1 * inv; a2 = na2 * inv;
    }
    void SetLowpass(float f, float q, float sr) {
        const float w = 2.0f * kPi * clampf(f, 5.0f, 0.45f * sr) / sr, c = std::cos(w), alpha = std::sin(w) / (2.0f * q);
        Set((1.0f - c) * 0.5f, 1.0f - c, (1.0f - c) * 0.5f, 1.0f + alpha, -2.0f * c, 1.0f - alpha);
    }
    void SetHighpass(float f, float q, float sr) {
        const float w = 2.0f * kPi * clampf(f, 5.0f, 0.45f * sr) / sr, c = std::cos(w), alpha = std::sin(w) / (2.0f * q);
        Set((1.0f + c) * 0.5f, -(1.0f + c), (1.0f + c) * 0.5f, 1.0f + alpha, -2.0f * c, 1.0f - alpha);
    }
    void SetPeaking(float f, float db, float q, float sr) {
        const float A = std::pow(10.0f, db / 40.0f), w = 2.0f * kPi * clampf(f, 5.0f, 0.45f * sr) / sr, c = std::cos(w), alpha = std::sin(w) / (2.0f * q);
        Set(1.0f + alpha * A, -2.0f * c, 1.0f - alpha * A, 1.0f + alpha / A, -2.0f * c, 1.0f - alpha / A);
    }
    void SetLowShelf(float f, float db, float sr) { // slope 1
        const float A = std::pow(10.0f, db / 40.0f), w = 2.0f * kPi * clampf(f, 5.0f, 0.45f * sr) / sr, c = std::cos(w);
        const float beta = 2.0f * std::sqrt(A) * (std::sin(w) / 1.41421356f);
        Set(A * ((A + 1.0f) - (A - 1.0f) * c + beta), 2.0f * A * ((A - 1.0f) - (A + 1.0f) * c), A * ((A + 1.0f) - (A - 1.0f) * c - beta),
            (A + 1.0f) + (A - 1.0f) * c + beta, -2.0f * ((A - 1.0f) + (A + 1.0f) * c), (A + 1.0f) + (A - 1.0f) * c - beta);
    }
    void SetHighShelf(float f, float db, float sr) { // slope 1
        const float A = std::pow(10.0f, db / 40.0f), w = 2.0f * kPi * clampf(f, 5.0f, 0.45f * sr) / sr, c = std::cos(w);
        const float beta = 2.0f * std::sqrt(A) * (std::sin(w) / 1.41421356f);
        Set(A * ((A + 1.0f) + (A - 1.0f) * c + beta), -2.0f * A * ((A - 1.0f) + (A + 1.0f) * c), A * ((A + 1.0f) + (A - 1.0f) * c - beta),
            (A + 1.0f) - (A - 1.0f) * c + beta, 2.0f * ((A - 1.0f) - (A + 1.0f) * c), (A + 1.0f) - (A - 1.0f) * c - beta);
    }
    float Process(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void Reset() { z1 = z2 = 0.0f; }
};

/** Soft clipper f(x) = 1.5x - 0.5x^3 up to |x| = 1, then +-1 (small-signal gain 1.5), with first-order antiderivative anti-aliasing. */
struct SoftClipAdaa {
    float x1 = 0.0f, F1 = 0.0f;
    static float f(float x) { return x >= 1.0f ? 1.0f : (x <= -1.0f ? -1.0f : 1.5f * x - 0.5f * x * x * x); }
    static float F(float x) { // the antiderivative (even, F(0) = 0)
        const float a = std::fabs(x);
        return a >= 1.0f ? a - 0.375f : 0.75f * x * x - 0.125f * x * x * x * x;
    }
    float Process(float x) {
        const float Fx = F(x), d = x - x1;
        const float y = std::fabs(d) < 1.0e-4f ? f(0.5f * (x + x1)) : (Fx - F1) / d;
        x1 = x; F1 = Fx;
        return y;
    }
    void Reset() { x1 = 0.0f; F1 = 0.0f; }
};

/** Hard clipper at +-1 with first-order antiderivative anti-aliasing. */
struct HardClipAdaa {
    float x1 = 0.0f, F1 = 0.0f;
    static float f(float x) { return x >= 1.0f ? 1.0f : (x <= -1.0f ? -1.0f : x); }
    static float F(float x) {
        const float a = std::fabs(x);
        return a >= 1.0f ? a - 0.5f : 0.5f * x * x;
    }
    float Process(float x) {
        const float Fx = F(x), d = x - x1;
        const float y = std::fabs(d) < 1.0e-4f ? f(0.5f * (x + x1)) : (Fx - F1) / d;
        x1 = x; F1 = Fx;
        return y;
    }
    void Reset() { x1 = 0.0f; F1 = 0.0f; }
};

} // namespace distdsp
