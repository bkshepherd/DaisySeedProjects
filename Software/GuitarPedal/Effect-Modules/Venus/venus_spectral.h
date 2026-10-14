// venus_spectral.h
//
// The spectral reverb at the heart of Venus, adapted from the Venus pedal in the FunBox project by GuitarML:
//   https://github.com/GuitarML/Funbox  (software/Venus/venus.cpp)  Copyright (c) 2024 Keith Bloemer, MIT License (see README.md in this directory)
// whose reverb maths is in turn based on the Atlantis Reverb by Geraint Luff (https://geraintluff.github.io/jsfx/#Atlantis%20Reverb).
//
// Per frame of the short-time Fourier transform, every bin's energy is accumulated into a reverb energy that decays over time, can be morphed up or
// down an octave (shimmer) or detuned, and is turned back into a random-phase spectrum with that energy. Changes from the original:
//   * the work for one frame is split into chunks of bins so it can run a few bins per audio block (see sliced_fourier.h)
//   * the random phase comes from a lookup table instead of rand() / cos() / sin() per bin
//   * the parameters are latched once per frame, and the constants that depended on the original 32 kHz sample rate are rescaled (FrameParams)
// Header only, and free of any hardware dependency, so the host tests can use it directly.
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace venus {

constexpr size_t kOrder = 12;
constexpr size_t kN = size_t(1) << kOrder; // size of the transform
constexpr size_t kLaps = 4;                // overlapping windows
constexpr size_t kBins = kN / 2;           // bins processed per frame
constexpr size_t kChunks = 16;             // chunks per frame (kBins / kChunks bins each)
constexpr size_t kPhaseTableSize = 1024;   // random phases are drawn from a table of this many angles

// Everything the per-bin processing needs, computed from the controls by the module and latched at the start of every frame.
struct FrameParams {
    float decayFactor = 0.1f;         // fraction of the reverb energy removed per frame (1 / decay in frames)
    float damp = 0.1f;                // fraction of the spectrum (0..1 of Nyquist) above which the output rolls off as 1/f
    float energyScale = 1.0f / kLaps; // how much of each frame's energy is added to the reverb energy
    int shimmerMode = 1;              // 0 = octave down, 1 = octave up, 2 = octave up (the original's "up and down" only ever goes up)
    float shimmerDouble = 0.0f;       // fraction of energy moved an octave per frame
    float shimmerTriple = 0.0f;       // fraction of energy moved an octave plus a fifth per frame
    float shimmerRemainder = 1.0f;
    int detuneMode = 1; // 0 = down, 1 = none, 2 = up
    int detuneMultiplier = 1;
    float detuneDouble = 0.0f; // fraction of energy moved one step per frame
    float detuneRemainder = 1.0f;
    bool freeze = false; // hold the reverb energy: no new energy, no decay
};

class SpectralReverb {
  public:
    SpectralReverb() { Init(); }

    void Init() {
        for (size_t i = 0; i < kBins; i++) energy_[i] = 0.0f;
        for (size_t i = 0; i < kPhaseTableSize; i++) {
            const float phase = 2.0f * 3.14159265358979f * (float)i / (float)kPhaseTableSize;
            cos_[i] = std::cos(phase);
            sin_[i] = std::sin(phase);
        }
        rng_ = 0x12345678u;
    }

    /** Parameters for the next frame; latched when that frame starts. */
    void SetParams(const FrameParams &p) { pending_ = p; }

    // Hooks for SlicedFourier (or soundmath::Fourier for the whole frame at once, in the tests).
    static void BeginFrame(void *self) { static_cast<SpectralReverb *>(self)->frame_ = static_cast<SpectralReverb *>(self)->pending_; }
    static void ProcessChunk(void *self, const float *in, float *out, size_t chunk, size_t chunkCount) {
        const size_t perChunk = kBins / chunkCount;
        static_cast<SpectralReverb *>(self)->ProcessBins(in, out, chunk * perChunk, (chunk + 1) * perChunk);
    }

    /** Bins first .. last-1 of one frame. shy_fft packs the spectrum as [real, real, ..., imag, imag, ...]. Must be called for the bins of a frame in order. */
    void ProcessBins(const float *in, float *out, size_t first, size_t last) {
        const FrameParams &p = frame_;
        const size_t offset = kBins; // offset of the imaginary parts
        const float fftSize = (float)kBins;
        const float halfFftSize = fftSize / 2.0f;

        for (size_t i = first; i < last; i++) {
            const float fftBin = (float)(i + 1);

            float real = in[i];
            float imag = in[i + offset];
            const float energy = real * real + imag * imag;

            // Amplitude from energy
            float reverbAmp = std::sqrt(energy_[i]);
            if (fftBin / fftSize > p.damp) {
                reverbAmp *= p.damp * fftSize / fftBin; // reduce the amplitude by 1/f
            }

            // Random phase for the reverb energy
            const unsigned idx = nextRandom() >> (32 - 10);
            real = reverbAmp * cos_[idx];
            imag = reverbAmp * sin_[idx];

            // If frozen, don't add new energy or decay the reverb
            if (!p.freeze) {
                energy_[i] += energy * p.energyScale; // add the current energy to the reverb
                energy_[i] *= 1.0f - p.decayFactor;   // decay the reverb

                const float current = energy_[i];
                if (i > 0 && i < (size_t)(halfFftSize - 2)) {
                    // Morph the reverb up or down by octaves
                    if (p.shimmerMode == 1 || p.shimmerMode == 2) { // up an octave
                        energy_[2 * i - 1] += 0.123f * p.shimmerDouble * current;
                        energy_[2 * i] += 0.25f * p.shimmerDouble * current;
                        energy_[2 * i + 1] += 0.123f * p.shimmerDouble * current;
                    } else if ((p.shimmerMode == 0 || p.shimmerMode == 2) && i > 1 && !(i % 2)) { // down an octave (even bins only)
                        energy_[i / 2 - 1] += 0.75f * p.shimmerDouble * current;
                        energy_[i / 2] += 1.5f * p.shimmerDouble * current;
                        energy_[i / 2 + 1] += 0.75f * p.shimmerDouble * current;
                    }

                    // Morph the reverb up by an octave plus a fifth
                    if (3 * i + 1 < (size_t)halfFftSize) {
                        energy_[3 * i - 2] += 0.055f * p.shimmerTriple * current;
                        energy_[3 * i - 1] += 0.11f * p.shimmerTriple * current;
                        energy_[3 * i] += 0.17f * p.shimmerTriple * current;
                        energy_[3 * i + 1] += 0.11f * p.shimmerTriple * current;
                        energy_[3 * i + 2] += 0.105f * p.shimmerTriple * current;
                    }

                    // Detune up or down
                    if (i > 2 && p.detuneMode != 1) {
                        energy_[i + (3 * p.detuneMultiplier)] += 0.123f * p.detuneDouble * current;
                        energy_[i + (2 * p.detuneMultiplier)] += 0.25f * p.detuneDouble * current;
                        energy_[i + (1 * p.detuneMultiplier)] += 0.123f * p.detuneDouble * current;
                    }
                }
                energy_[i] = p.detuneRemainder * p.shimmerRemainder * current;
            }

            out[i] = real;
            out[i + offset] = imag;
        }
    }

  private:
    uint32_t nextRandom() {
        rng_ ^= rng_ << 13; // xorshift32
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return rng_;
    }

    float energy_[kBins];
    float cos_[kPhaseTableSize];
    float sin_[kPhaseTableSize];
    uint32_t rng_;
    FrameParams pending_;
    FrameParams frame_;
};
} // namespace venus
