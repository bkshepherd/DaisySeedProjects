// The parts of Mars that are not hardware specific: the GRU network of the neural models and the two-tap delay line. Adapted from the Mars pedal of the FunBox project
// (https://github.com/GuitarML/Funbox, MIT licence, Copyright (c) 2024 Keith Bloemer), where the network ran on RTNeural (https://github.com/jatinchowdhury18/RTNeural, BSD-3-Clause)
// and Eigen. See Mars/README.md.
#pragma once
#ifndef MARS_DSP_H
#define MARS_DSP_H

#include "mars_models.h"
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace mars {

// tanh and the logistic function. The exact ones cost about as much as the rest of the effect together on the Daisy (27 per sample), so the module uses a rational approximation
// (the [7/6] Pade approximant of tanh, which is exactly +-1 beyond |x| = 4.97; the largest error is 3e-4, checked in tests/host/test_mars.cpp).
struct ExactMath {
    static float tanh(float x) { return std::tanh(x); }
    static float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }
};

struct FastMath {
    static float tanh(float x) {
        if (x > 4.97f) return 1.0f;
        if (x < -4.97f) return -1.0f;
        const float x2 = x * x;
        const float num = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
        const float den = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
        return num / den;
    }
    static float sigmoid(float x) { return 0.5f + 0.5f * tanh(0.5f * x); }
};

// A GRU layer with one input and 9 hidden units, followed by a dense layer with one output (RTNeural's GRULayerT with the Eigen backend, which follows Keras with reset_after):
//   z = sigmoid(Wz x + bz + Uz h + bz')    r = sigmoid(Wr x + br + Ur h + br')    c = tanh(Wc x + bc + r * (Uc h + bc'))    h = c + z * (h - c)
template <class Maths = FastMath> class Gru {
  public:
    void SetModel(const Model *model) {
        m_ = model;
        Reset();
    }
    void Reset() {
        for (float &v : h_) v = 0.0f;
    }

    float Process(float x) {
        const int H = kHidden;
        float g[3 * kHidden]; // alpha + beta: the pre-activations of z, r and the candidate's two parts
        float candidateRec[kHidden];
        for (int k = 0; k < 2 * H; k++) {
            float s = m_->w[k] * x + m_->biasIn[k] + m_->biasRec[k];
            for (int j = 0; j < H; j++) s += m_->u[k][j] * h_[j];
            g[k] = Maths::sigmoid(s);
        }
        for (int k = 0; k < H; k++) {
            float s = m_->biasRec[2 * H + k];
            for (int j = 0; j < H; j++) s += m_->u[2 * H + k][j] * h_[j];
            candidateRec[k] = s;
        }
        float out = m_->denseBias;
        for (int k = 0; k < H; k++) {
            const float z = g[k];
            const float r = g[H + k];
            const float c = Maths::tanh(m_->w[2 * H + k] * x + m_->biasIn[2 * H + k] + r * candidateRec[k]);
            h_[k] = c + z * (h_[k] - c);
            out += m_->dense[k] * h_[k];
        }
        return out;
    }

  private:
    const Model *m_ = nullptr;
    float h_[kHidden] = {};
};

// A delay line with a second tap at a fraction of the delay time after the first (2/3 for a triplet, 3/4 for a dotted eighth), by Keith Bloemer, based on DaisySP's DelayLine. The
// buffer is given to Init (it is in SDRAM) and must hold the longest delay plus the longest second tap.
class DelayLine2Tap {
  public:
    void Init(float *buffer, size_t size) {
        line_ = buffer;
        size_ = size;
        Reset();
    }
    void Reset() {
        for (size_t i = 0; i < size_; i++) line_[i] = 0.0f;
        write_ptr_ = 0;
        delay_ = 1;
        delay_secondTap_ = 1;
        frac_ = frac_secondTap_ = 0.0f;
        delay_float_ = 1.0f;
        secondTapFraction_ = 0.0f;
    }

    // The fractional length of the second tap
    void Set2ndTapFraction(float fraction) {
        secondTapFraction_ = fraction;
        UpdateSecondTap();
    }

    void SetDelay(float delay) {
        const int32_t i = static_cast<int32_t>(delay);
        frac_ = delay - static_cast<float>(i);
        delay_ = static_cast<size_t>(i) < size_ ? static_cast<size_t>(i) : size_ - 1;
        delay_float_ = delay;
        UpdateSecondTap();
    }

    void Write(float sample) {
        line_[write_ptr_] = sample;
        write_ptr_ = write_ptr_ == 0 ? size_ - 1 : write_ptr_ - 1;
    }

    float Read() const {
        const float a = line_[Wrap(write_ptr_ + delay_)];
        const float b = line_[Wrap(write_ptr_ + delay_ + 1)];
        return a + (b - a) * frac_;
    }

    float ReadSecondTap() const {
        const float c = line_[Wrap(write_ptr_ + delay_ + delay_secondTap_)];
        const float d = line_[Wrap(write_ptr_ + delay_ + delay_secondTap_ + 1)];
        return c + (d - c) * frac_secondTap_;
    }

  private:
    size_t Wrap(size_t i) const { return i >= size_ ? i % size_ : i; }
    void UpdateSecondTap() {
        const float t = delay_float_ * secondTapFraction_;
        const int32_t i = static_cast<int32_t>(t);
        frac_secondTap_ = t - static_cast<float>(i);
        delay_secondTap_ = static_cast<size_t>(i) < size_ ? static_cast<size_t>(i) : size_ - 1;
    }

    float *line_ = nullptr;
    size_t size_ = 0;
    size_t write_ptr_ = 0;
    size_t delay_ = 1, delay_secondTap_ = 1;
    float frac_ = 0.0f, frac_secondTap_ = 0.0f;
    float delay_float_ = 1.0f;
    float secondTapFraction_ = 0.0f;
};

} // namespace mars

#endif
