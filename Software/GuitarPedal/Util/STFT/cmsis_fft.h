// cmsis_fft.h
//
// A real FFT with the interface and the spectrum layout of ShyFFT (Util/STFT/shy_fft.h), but computed by ARM's CMSIS-DSP `arm_rfft_fast_f32`, which is
// optimised for the Cortex-M7 (on a PC it is about twice as fast as ShyFFT; the gain on the chip has to be measured). Venus runs two 4096-point transforms per hop, so they dominate its CPU load.
//
// ShyFFT's layout for an N-point transform of a real signal (verified against a plain DFT in tests/host/test_venus.cpp):
//     out[k]          = Re X[k]       for k = 0 .. N/2 - 1
//     out[N/2]        = Re X[N/2]     (the Nyquist bin, which has no imaginary part)
//     out[N/2 + k]    = -Im X[k]      for k = 1 .. N/2 - 1
// CMSIS packs the same data as interleaved pairs: [Re0, ReNyquist, Re1, Im1, Re2, Im2, ...]. This class converts between the two, so the spectral processing
// (SpectralReverb) is unchanged. Inverse() returns the signal scaled by N (ShyFFT does not normalise, CMSIS does, so the result is multiplied back by N, which
// is exact: N is a power of two).
//
// Memory: an FFT makes about log2(N) passes over its buffer, in place. Venus keeps its frames in SDRAM, which is slow to access, so running the passes there made
// each transform take milliseconds (the audio blocks that ran one overran). Here the time signal is copied from / to the caller's buffer once, sequentially,
// and all the passes run in this object's own workspace, which must therefore live in fast on-chip RAM (the module makes it a file-scope static). The
// caller's input is not modified.
#pragma once
#include "arm_math.h"
#include <cstddef>
#include <cstring>

namespace venus {

template <size_t N> class CmsisRealFft {
  public:
    void Init() { arm_rfft_fast_init_f32(&inst_, (uint16_t)N); }

    /** Time signal `input` (N samples) -> spectrum `output` (N floats, ShyFFT layout). */
    void Direct(const float *input, float *output) {
        std::memcpy(work_, input, sizeof(work_)); // one sequential read of the (SDRAM) frame
        arm_rfft_fast_f32(&inst_, work_, tmp_, 0);
        // tmp_ = [Re0, ReNyquist, Re1, Im1, Re2, Im2, ...]
        output[0] = tmp_[0];
        output[N / 2] = tmp_[1];
        for (size_t k = 1; k < N / 2; k++) {
            output[k] = tmp_[2 * k];
            output[N / 2 + k] = -tmp_[2 * k + 1];
        }
    }

    /** Spectrum `input` (ShyFFT layout) -> time signal `output` scaled by N. */
    void Inverse(const float *input, float *output) {
        tmp_[0] = input[0];
        tmp_[1] = input[N / 2];
        for (size_t k = 1; k < N / 2; k++) {
            tmp_[2 * k] = input[k];
            tmp_[2 * k + 1] = -input[N / 2 + k];
        }
        arm_rfft_fast_f32(&inst_, tmp_, work_, 1);
        for (size_t i = 0; i < N; i++) output[i] = work_[i] * (float)N; // one sequential write to the (SDRAM) frame
    }

  private:
    arm_rfft_fast_instance_f32 inst_;
    float tmp_[N];  // interleaved spectrum; 16 KB for N = 4096
    float work_[N]; // the time signal the transform works on; 16 KB for N = 4096
};

} // namespace venus
