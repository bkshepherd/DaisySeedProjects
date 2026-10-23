#pragma once
// DSP pieces of the spring reverb (Effect-Modules/spring_reverb_module.h).
//
// A spring is dispersive: below its transition frequency fC (about 4 kHz) the waves slow down as the frequency rises, the group delay growing
// steeply toward fC (like that of a low-pass ladder, extra delay ~ 1 / sqrt(1 - (f / fC)^2)), so the lows of an echo arrive first and the
// highs last: every echo is a short upward sweep, the "boing" or "drip" of a spring reverb. (A second, much weaker family of echoes
// sweeps the other way, and is left out: Parker 2011 puts it at least 30 dB below the main one.) It is modelled as in Abel et al. (2006) and
// Valimaki, Parker and Abel (2010): a cascade of allpass filters, which delay some frequencies more than others without changing the level,
// in a feedback loop with a delay line (the transit of the wave to the end of the spring and back).
//
// The allpass sections are stretched, A(z^K) = (a + z^-K) / (1 + a z^-K) with a > 0: the delay of a section grows with the frequency to a maximum
// at fs / (2K), which is made the transition frequency (4 kHz: K = 6 at 48 kHz, K = 2 at 16 kHz), and K times larger than that of an ordinary first-order
// section, so fewer sections are needed. The response repeats and mirrors every fs / K, so the signal must be low-passed around fs / (2K), which
// a spring does anyway. The delay of a section in seconds depends on K / fs only, so the same sections give the same sweep at 16 kHz as at 48 kHz, for a
// third of the processing (the module runs its springs at a third of the sample rate: they carry nothing above 4 kHz). All sections of a chain share one
// phase index, and each needs only K samples of state: two multiply-adds per section and sample.
#include <cmath>
#include <cstddef>

namespace springdsp {

/** A cascade of up to MAX_SECTIONS stretched allpass sections, in groups of `counts[g]` sections with the coefficient `coefs[g]` (0 < a < 1).
 *  Many sections with a mild coefficient give the gentle rise of the delay over most of the band, a few with a coefficient near 1 the steep
 *  climb to the maximum at the transition frequency. */
template <int MAX_SECTIONS, int K = 6>
struct DispersionChain {
    static constexpr int kStretch = K;
    float a[MAX_SECTIONS] = {};
    float w[MAX_SECTIONS][K] = {};
    int n = 0;
    int phase = 0;

    void Reset() {
        for (int s = 0; s < MAX_SECTIONS; s++)
            for (int k = 0; k < K; k++) w[s][k] = 0.0f;
        phase = 0;
    }

    /** Sets the coefficients (the state is kept, so this can be done while playing). The coefficients are clamped to stay stable. */
    void Set(const int *counts, const float *coefs, int groups) {
        n = 0;
        for (int g = 0; g < groups; g++) {
            const float c = coefs[g] > 0.98f ? 0.98f : (coefs[g] < -0.98f ? -0.98f : coefs[g]);
            for (int k = 0; k < counts[g] && n < MAX_SECTIONS; k++) a[n++] = c;
        }
    }

    float Process(float x) {
        for (int s = 0; s < n; s++) {
            const float wp = w[s][phase]; // w[n - K]
            const float wn = x - a[s] * wp;
            x = a[s] * wn + wp;
            w[s][phase] = wn;
        }
        phase = phase + 1 == K ? 0 : phase + 1;
        return x;
    }

    /** The group delay of the chain at frequency f, in seconds */
    float GroupDelay(float f, float sampleRate) const {
        const double wk = 2.0 * 3.14159265358979 * f / sampleRate * K;
        const double c = std::cos(wk);
        double sum = 0.0;
        for (int s = 0; s < n; s++) sum += (1.0 - double(a[s]) * a[s]) / (1.0 + double(a[s]) * a[s] + 2.0 * a[s] * c);
        return float(sum * K / sampleRate);
    }
};

/** The coefficient of a section for an amount of dispersion `drip` from 0 to 1: the coefficient scaled. At 0 every section is a plain delay of K samples
 *  (no sweep); the extra delay of the highs then grows about in proportion to drip up to some 75% (0, 6, 12, 19 and 20 ms at 3.6 kHz for 0, 25, 50, 75 and 100%),
 *  and the last quarter narrows the peak at the transition frequency and raises it (54 ms at 3.9 kHz). (For the other sweep, with the lows delayed
 *  most, scaling the coefficient gave a third of the sweep at 70%: the echoes stayed sharp and sounded like a delay. That is not so here.) */
inline float DripCoefficient(float a, float drip) {
    drip = drip < 0.0f ? 0.0f : (drip > 1.0f ? 1.0f : drip);
    return a * drip;
}

/** A circular buffer read with a fractional delay (linear interpolation). */
template <size_t SIZE>
struct DelayLine {
    float buf[SIZE] = {};
    size_t pos = 0; // where the next sample is written

    void Reset() {
        for (size_t i = 0; i < SIZE; i++) buf[i] = 0.0f;
        pos = 0;
    }
    void Write(float x) {
        buf[pos] = x;
        pos = pos + 1 == SIZE ? 0 : pos + 1;
    }
    /** The sample written `delay` writes ago (1 = the last one), 1 <= delay <= SIZE - 2; read before the Write of the current sample. */
    float Read(float delay) const {
        const size_t i = static_cast<size_t>(delay);
        const float fr = delay - static_cast<float>(i);
        const size_t i0 = (pos + SIZE - i) % SIZE, i1 = (pos + SIZE - i - 1) % SIZE;
        return buf[i0] + (buf[i1] - buf[i0]) * fr;
    }
};

} // namespace springdsp
