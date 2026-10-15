// The parts of Balnea that are not hardware specific, adapted from the Balnea pedal of the FunBox project (https://github.com/GuitarML/Funbox, MIT licence,
// Copyright (c) 2024 Keith Bloemer): the tape wow/flutter modulator, the reverse delay line (Copyright 2021 Adam Fulford, MIT) and the MIDI synth voices.
// See Balnea/README.md for the list of changes.
#pragma once
#ifndef BALNEA_DSP_H
#define BALNEA_DSP_H

#include "daisysp.h"
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace balnea {

// Tape wow and flutter from fractal Perlin noise: smooth, natural speed variations. Left and right differ in the wow only.
class TapeModulator {
  public:
    void Init(float sample_rate) {
        for (int i = 0; i < 256; i++) perm_[i] = perm_[i + 256] = kPerm[i];
        sampleRate_ = sample_rate;
        tWow_ = tFlutter_ = 0.0f;
        tWowRight_ = 0.7f;
        speedLeft_ = speedRight_ = 0.0f;
    }

    float GetTapeSpeedLeft() const { return speedLeft_; }
    float GetTapeSpeedRight() const { return speedRight_; }

    // Advances the noise by one step of 1 / sample_rate seconds. Wow is slow (rate in Hz), flutter faster and smaller; the right channel's wow runs at rightRateRatio times the left's.
    void Process(float wowRate, float flutterRate, float wowDepth, float flutterDepth, float rightRateRatio) {
        const float wow = Fbm1D(tWow_, 2, 2.0f, 0.5f);
        const float flutter = Fbm1D(tFlutter_, 1, 2.0f, 0.5f);
        tWow_ += wowRate / sampleRate_;
        tFlutter_ += flutterRate / sampleRate_;
        if (tWow_ >= 256.0f) tWow_ -= 256.0f;
        if (tFlutter_ >= 256.0f) tFlutter_ -= 256.0f;

        const float wowRight = Fbm1D(tWowRight_, 2, 2.0f, 0.5f); // the flutter is the same on both sides (less processing)
        tWowRight_ += wowRate * rightRateRatio / sampleRate_;
        if (tWowRight_ >= 256.0f) tWowRight_ -= 256.0f;

        speedLeft_ = wowDepth * wow + (flutterDepth * 0.2f) * flutter;
        speedRight_ = wowDepth * wowRight + (flutterDepth * 0.2f) * flutter;
    }

  private:
    static float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    static float lerp(float t, float a, float b) { return a + t * (b - a); }
    static float grad(int hash, float x) { return (hash & 1) == 0 ? x : -x; }

    float Perlin1D(float x) const {
        const int X = (int)std::floor(x) & 255;
        x -= std::floor(x);
        const float u = fade(x);
        return lerp(u, grad(perm_[X], x), grad(perm_[X + 1], x - 1));
    }

    float Fbm1D(float x, int octaves, float lacunarity, float gain) const {
        float sum = 0.0f, amp = 1.0f, freq = 1.0f, maxSum = 0.0f;
        for (int i = 0; i < octaves; i++) {
            sum += Perlin1D(x * freq) * amp;
            maxSum += amp;
            freq *= lacunarity;
            amp *= gain;
        }
        return sum / maxSum;
    }

    // Ken Perlin's permutation table
    static constexpr uint8_t kPerm[256] = {
        151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225, 140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,  190, 6,   148,
        247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,  57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136, 171, 168, 68,  175,
        74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158, 231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,  55,  46,  245, 40,  244, 102, 143, 54,
        65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130, 116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,
        52,  217, 226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,  227, 47,  16,  58,  17,  182, 189, 28,  42,  223, 183, 170, 213,
        119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,   129, 22,  39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104,
        218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,  51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157,
        184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,  222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180};

    uint8_t perm_[512];
    float sampleRate_ = 48000.0f;
    float tWow_ = 0.0f, tFlutter_ = 0.0f, tWowRight_ = 0.7f;
    float speedLeft_ = 0.0f, speedRight_ = 0.0f;
};

// Reverse delay line by Adam Fulford: two read heads move backwards through the buffer, crossfading every time one reaches the write position's delay. The buffer is given
// to Init (it is in SDRAM). It holds twice the delay, as the heads move in opposite directions. Changes from the original: no modulo divisions, and the head that jumps to the write
// position at the start of a crossfade no longer underflows when the write position has just wrapped to 0 (the original then read far outside the buffer).
class DelayLineReverse {
  public:
    void Init(float *buffer, size_t size) {
        line_ = buffer;
        size_ = size;
        Reset();
    }

    void Reset() {
        delay1_ = 2400; // minimum reverse delay time
        fadetime_ = 2300;
        for (size_t i = 0; i < size_; i++) line_[i] = 0.0f;
        write_ptr_ = read_ptr1_ = read_ptr2_ = 0;
        headDiff_ = 0;
        playinghead_ = false;
        fadepos_ = 0.0f;
        fading_ = false;
    }

    void SetDelay1(float delay) {
        const int32_t intDelay = static_cast<int32_t>(delay);
        delay1_ = static_cast<size_t>(intDelay) < size_ ? static_cast<size_t>(intDelay) : size_ - 1;
    }

    void Write(float sample) {
        line_[write_ptr_] = sample;
        if (++write_ptr_ >= size_) write_ptr_ = 0;

        if (++headDiff_ >= delay1_) headDiff_ %= delay1_;

        read_ptr1_ = read_ptr1_ == 0 ? size_ - 1 : read_ptr1_ - 1;
        read_ptr2_ = read_ptr2_ == 0 ? size_ - 1 : read_ptr2_ - 1;

        if (headDiff_ > (delay1_ - fadetime_ - 1)) { // the crossfade region starts
            if (!fading_) {
                fading_ = true;
                const size_t jump = write_ptr_ == 0 ? size_ - 1 : write_ptr_ - 1;
                if (!playinghead_) read_ptr2_ = jump; // the idle head jumps to the write position
                else read_ptr1_ = jump;
            }
        }

        if (fading_) {
            if (!playinghead_) {
                fadepos_ += 1.0f / fadetime_;
                if (fadepos_ > 1.0f) {
                    fadepos_ = 1.0f;
                    fading_ = false;
                    playinghead_ = true;
                }
            } else {
                fadepos_ -= 1.0f / fadetime_;
                if (fadepos_ < 0.0f) {
                    fadepos_ = 0.0f;
                    fading_ = false;
                    playinghead_ = false;
                }
            }
        }
    }

    float ReadRev() const {
        const float scalar1 = std::sin(fadepos_ * ((float)M_PI * 0.5f));
        const float scalar2 = std::sin((1.0f - fadepos_) * ((float)M_PI * 0.5f));
        return (line_[read_ptr2_] * scalar1) + (line_[read_ptr1_] * scalar2);
    }

  private:
    float *line_ = nullptr;
    size_t size_ = 0;
    size_t write_ptr_ = 0, read_ptr1_ = 0, read_ptr2_ = 0;
    size_t delay1_ = 2400;
    size_t headDiff_ = 0;
    size_t fadetime_ = 2300;
    bool playinghead_ = false;
    float fadepos_ = 0.0f;
    bool fading_ = false;
};

// The MIDI synth that is mixed with the input: sawtooth voices with a low pass filter and an envelope
class Voice {
  public:
    void Init(float samplerate) {
        active_ = false;
        gate_ = false;
        note_ = velocity_ = 0.0f;
        osc_.Init(samplerate);
        osc_.SetAmp(0.75f);
        osc_.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_SAW);
        env_.Init(samplerate);
        env_.SetSustainLevel(0.4f);
        env_.SetTime(daisysp::ADSR_SEG_ATTACK, 0.005f);
        env_.SetTime(daisysp::ADSR_SEG_DECAY, 0.08f);
        env_.SetTime(daisysp::ADSR_SEG_RELEASE, 0.2f);
        filt_.Init(samplerate);
        filt_.SetFreq(6000.f);
        filt_.SetRes(0.6f);
        filt_.SetDrive(0.8f);
    }

    float Process() {
        if (!active_) return 0.f;
        const float amp = env_.Process(gate_);
        if (!env_.IsRunning()) active_ = false;
        const float sig = osc_.Process();
        filt_.Process(sig);
        return filt_.Low() * (velocity_ / 127.f) * amp;
    }

    void OnNoteOn(float note, float velocity) {
        note_ = note;
        velocity_ = velocity;
        osc_.SetFreq(daisysp::mtof(note_));
        active_ = true;
        gate_ = true;
    }
    void OnNoteOff() { gate_ = false; }
    void SetCutoff(float val) { filt_.SetFreq(val); }
    bool IsActive() const { return active_; }
    float GetNote() const { return note_; }

  private:
    daisysp::Oscillator osc_;
    daisysp::Svf filt_;
    daisysp::Adsr env_;
    float note_ = 0.0f, velocity_ = 0.0f;
    bool active_ = false;
    bool gate_ = false;
};

template <size_t kVoices> class VoiceManager {
  public:
    void Init(float samplerate) {
        for (auto &v : voices_) v.Init(samplerate);
    }

    float Process() {
        float sum = 0.f;
        for (auto &v : voices_) sum += v.Process();
        return sum;
    }

    bool AnyActive() const {
        for (const auto &v : voices_)
            if (v.IsActive()) return true;
        return false;
    }

    void OnNoteOn(float note, float velocity) {
        for (auto &v : voices_) {
            if (!v.IsActive()) {
                v.OnNoteOn(note, velocity);
                return;
            }
        }
    }

    void OnNoteOff(float note, float /*velocity*/) {
        for (auto &v : voices_)
            if (v.IsActive() && v.GetNote() == note) v.OnNoteOff();
    }

    void SetCutoff(float freq) {
        for (auto &v : voices_) v.SetCutoff(freq);
    }

  private:
    Voice voices_[kVoices];
};

} // namespace balnea

#endif
