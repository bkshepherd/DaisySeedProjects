#pragma once
#ifndef PITCH_SHIFTER_FFT_H
#define PITCH_SHIFTER_FFT_H

#include "../Util/STFT/fft_shared_context.h"
#include "daisysp.h"
#include <cmath>
#include <cstring>
#include <stdint.h>

// Set to false to use simple pitch shifting without vocoder processing
#define USE_VOCODER true

class PitchShifterFFT {
  public:
    PitchShifterFFT() {}
    ~PitchShifterFFT() {}

    void Init(float sample_rate) {
        sample_rate_ = sample_rate;
        ratio_ = 1.0f;
        instance_ = this;

        // Vocoder parameters initialization
        size_t bins = kFFTSize / 2;
        prev_phase_.resize(bins, 0.0f);
        sum_phase_.resize(bins, 0.0f);
        mag_.resize(bins, 0.0f);
        freq_.resize(bins, 0.0f);

        // Init the shared FFT with our callback
        FFTSharedContext::Instance().Init(sample_rate, &PitchShifterFFT::ProcessFrameStatic);
        FFTSharedContext::Instance().SetUserData(this);
    }

    void SetTransposition(float semitones) {
        semitone_shift_ = semitones;
        ratio_ = std::pow(2.0f, semitone_shift_ / 12.0f);
        // Reset output phase accumulators (critical!)
        std::fill(sum_phase_.begin(), sum_phase_.end(), 0.0f);
    }

    float Process(float input) {
        auto *stft = FFTSharedContext::Instance().GetSTFT();
        stft->write(input);

        float gain_boost = 4.0f + std::abs(semitone_shift_) * 0.25f; // empirically determined
        return stft->read() * gain_boost;
    }

  private:
    float sample_rate_ = 48000.0f;
    float semitone_shift_ = 0.0f;
    float ratio_ = 1.0f;

    // Vocoder pararmeters
    std::vector<float> prev_phase_; // previous phase for each bin
    std::vector<float> sum_phase_;  // accumulated output phase
    std::vector<float> mag_;        // magnitude per bin
    std::vector<float> freq_;       // estimated frequency per bin

    static PitchShifterFFT *instance_;

    // Static callback passed into FFT system
    static void ProcessFrameStatic(const float *in, float *out) {
        if (instance_) {
#if USE_VOCODER
            instance_->ProcessFrameWithVocoder(in, out);
#else
            instance_->ProcessFrame(in, out);
#endif
        }
    }

    void ProcessFrame(const float *in, float *out) {
        std::fill(out, out + kFFTSize, 0.0f);
        float gain = 1.0f;
        for (size_t i = 0; i < kFFTSize / 2; ++i) {
            size_t shifted_idx = static_cast<size_t>(i / ratio_);
            if (shifted_idx < kFFTSize / 2) {
                out[i] += in[shifted_idx] * gain;
                out[i + kFFTSize / 2] += in[shifted_idx + kFFTSize / 2] * gain;
            }
        }
    }

    void ProcessFrameWithVocoder(const float *in, float *out) {
        constexpr float two_pi = 2.0f * M_PI;
        constexpr float mag_threshold = 1e-5f; // empirical floor (adjust as needed)

        const float bin_freq = sample_rate_ / static_cast<float>(kFFTSize);
        const float hop_size = static_cast<float>(kFFTSize) / static_cast<float>(kLaps);
        const float expected_phase_adv = two_pi * hop_size / static_cast<float>(kFFTSize); // radians per bin

        const size_t num_bins = kFFTSize / 2;
        std::fill(out, out + kFFTSize, 0.0f);

        // Analysis: compute magnitude and true frequency
        for (size_t i = 0; i < num_bins; ++i) {
            float re = in[i];
            float im = in[i + num_bins];

            float mag = std::sqrt(re * re + im * im);

            if (mag < mag_threshold) {
                mag_[i] = 0.0f;
                freq_[i] = 0.0f;
                continue;
            }

            float phase = std::atan2(im, re);
            float delta = phase - prev_phase_[i];
            prev_phase_[i] = phase;

            // Wrap delta to [-π, π]
            delta -= expected_phase_adv * static_cast<float>(i);
            delta -= two_pi * std::floor((delta + M_PI) / two_pi);

            // Estimate true frequency
            float true_freq = (static_cast<float>(i) + delta / expected_phase_adv) * bin_freq;

            mag_[i] = mag;
            freq_[i] = true_freq;
        }

        // Resynthesis: pull from input bin corresponding to i / ratio_
        for (size_t i = 0; i < num_bins; ++i) {
            float src_bin = static_cast<float>(i) / ratio_;
            size_t i0 = static_cast<size_t>(std::floor(src_bin));
            float frac = src_bin - static_cast<float>(i0);

            if (i0 + 1 >= num_bins)
                continue;

            // Magnitude + frequency interpolation
            float mag0 = mag_[i0];
            float mag1 = mag_[i0 + 1];
            float mag = (1.0f - frac) * mag0 + frac * mag1;

            float freq0 = freq_[i0];
            float freq1 = freq_[i0 + 1];
            float freq = (1.0f - frac) * freq0 + frac * freq1;

            // Optional skip if interpolated mag too low
            if (mag < mag_threshold)
                continue;

            // Accumulate output phase
            float phase_adv = two_pi * freq * hop_size / sample_rate_;
            sum_phase_[i] += phase_adv;

            float re = mag * std::cos(sum_phase_[i]);
            float im = mag * std::sin(sum_phase_[i]);

            out[i] += re;
            out[i + num_bins] += im;
        }
    }
};

// Static member definition
PitchShifterFFT *PitchShifterFFT::instance_ = nullptr;

#endif // PITCH_SHIFTER_FFT_H
