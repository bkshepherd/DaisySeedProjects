#pragma once
#ifndef FFT_SHARED_CONTEXT_H
#define FFT_SHARED_CONTEXT_H

// clang-format off
#include "./shy_fft.h"
#include "./fourier.h"
#include "./wave.h"
// clang-format on
#include "daisysp.h"
#include <cmath>
#include <cstdint>
#include <cstring>

using namespace soundmath;

using FFTProcessCallback = void (*)(const float *, float *);

constexpr size_t kFFTOrder = 9; // 512-point FFT
constexpr size_t kFFTSize = (1 << kFFTOrder);
constexpr size_t kLaps = 4;
constexpr size_t kBufferSize = 2 * kLaps * kFFTSize;

class FFTSharedContext {
  public:
    static FFTSharedContext &Instance();

    void Init(float sample_rate, FFTProcessCallback processor);

    Fourier<float, kFFTSize> *GetSTFT();
    ShyFFT<float, kFFTSize, RotationPhasor> *GetFFT();

    float *GetInputBuffer();
    float *GetMiddleBuffer();
    float *GetOutputBuffer();

    void SetUserData(void *ptr);
    template <typename T> T *GetUserDataAs() const { return static_cast<T *>(user_data_); }

  private:
    FFTSharedContext();
    ~FFTSharedContext() { delete stft_; }
    FFTSharedContext(const FFTSharedContext &) = delete;
    FFTSharedContext &operator=(const FFTSharedContext &) = delete;

    float sample_rate_;
    bool initialized_;
    void *user_data_;

    Wave<float> hann_;
    ShyFFT<float, kFFTSize, RotationPhasor> fft_;
    Fourier<float, kFFTSize> *stft_;
};

#endif // FFT_SHARED_CONTEXT_H
