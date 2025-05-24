#pragma once
#ifndef PITCH_SHIFTER_FFT_H
#define PITCH_SHIFTER_FFT_H

#include "../Util/STFT/fft_shared_context.h"
#include "daisysp.h"
#include <cmath>
#include <cstring>
#include <stdint.h>

class PitchShifterFFT {
  public:
    PitchShifterFFT() {}
    ~PitchShifterFFT() {}

    void Init(float sample_rate) {
        sample_rate_ = sample_rate;
        ratio_ = 1.0f;
        instance_ = this;

        // Init the shared FFT with our callback
        FFTSharedContext::Instance().Init(sample_rate, &PitchShifterFFT::ProcessFrameStatic);
        FFTSharedContext::Instance().SetUserData(this);
    }

    void SetTransposition(float semitones) {
        semitone_shift_ = semitones;
        ratio_ = std::pow(2.0f, semitone_shift_ / 12.0f);
    }

    float Process(float input) {
        auto *stft = FFTSharedContext::Instance().GetSTFT();
        stft->write(input);
        return stft->read();
    }

  private:
    float sample_rate_ = 48000.0f;
    float semitone_shift_ = 0.0f;
    float ratio_ = 1.0f;

    static PitchShifterFFT *instance_;

    // Static callback passed into FFT system
    static void ProcessFrameStatic(const float *in, float *out) {
        if (instance_) {
            instance_->ProcessFrame(in, out);
        }
    }

    void ProcessFrame(const float *in, float *out) {
        float shifted[kFFTSize] = {0.0f};
        for (size_t i = 0; i < kFFTSize / 2; ++i) {
            int shifted_idx = static_cast<int>(i / ratio_);
            if (shifted_idx >= 0 && shifted_idx < kFFTSize / 2) {
                shifted[i] = in[shifted_idx];
                shifted[i + kFFTSize / 2] = in[shifted_idx + kFFTSize / 2];
            }
        }
        memcpy(out, shifted, sizeof(shifted));
    }
};

// Static member definition
PitchShifterFFT *PitchShifterFFT::instance_ = nullptr;

#endif // PITCH_SHIFTER_FFT_H
