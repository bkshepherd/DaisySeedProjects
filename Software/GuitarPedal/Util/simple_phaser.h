#include <algorithm>
#include <cmath>
#include <q/fx/biquad.hpp>

using namespace cycfi::q;

struct SimplePhaser {
    static constexpr int stages = 4;

    allpass ap_[stages];

    float sample_rate_ = 48000.f;

    // Parameters
    float lfo_phase_ = 0.f;    // radians
    float lfo_freq_hz_ = 0.3f; // Hz
    float depth_ = 0.8f;       // 0..1
    float min_freq_ = 300.f;   // Hz
    float max_freq_ = 1500.f;  // Hz
    float feedback_ = 0.25f;   // 0..~0.7
    float fb_state_ = 0.f;

    SimplePhaser()
        : ap_{allpass{frequency{1000.0}, 48000.0f, 0.7}, allpass{frequency{1000.0}, 48000.0f, 0.7},
              allpass{frequency{1000.0}, 48000.0f, 0.7}, allpass{frequency{1000.0}, 48000.0f, 0.7}} {}

    void init(float sample_rate) {
        sample_rate_ = sample_rate;

        float fc = 0.5f * (min_freq_ + max_freq_);
        for (auto &ap : ap_)
            ap.config(frequency{fc}, sample_rate_, 0.7); // Q ~= 0.7 is a good default
    }

    void set_lfo_freq(float hz) { lfo_freq_hz_ = hz; }
    void set_depth(float d) { depth_ = std::clamp(d, 0.0f, 1.0f); }
    void set_feedback(float fb) { feedback_ = std::clamp(fb, 0.0f, 0.7f); }
    void set_range(float min_hz, float max_hz) {
        min_freq_ = std::max(1.0f, min_hz);
        max_freq_ = std::max(min_freq_ + 1.0f, max_hz);
    }

    float process(float in) {
        // --- LFO (sine in 0..1) ---
        constexpr float two_pi = 6.28318530718f;
        float phase_inc = two_pi * lfo_freq_hz_ / sample_rate_;
        lfo_phase_ += phase_inc;
        if (lfo_phase_ >= two_pi)
            lfo_phase_ -= two_pi;

        float lfo = 0.5f * (1.0f + std::sin(lfo_phase_)); // 0..1

        // Exponential-ish sweep between min_freq_ and max_freq_
        float sweep_pos = depth_ * lfo; // depth shrinks range
        float fc = min_freq_ * std::pow(max_freq_ / min_freq_, sweep_pos);

        frequency f{fc};

        // Reconfigure all stages to same center freq
        for (auto &ap : ap_)
            ap.config(f, sample_rate_, 0.7);

        // --- Allpass chain with feedback ---
        float x = in + fb_state_;
        float y = x;
        for (auto &ap : ap_)
            y = ap(y);

        fb_state_ = y * feedback_;

        // Equal wet/dry mix
        return 0.5f * (in + y);
    }
};
