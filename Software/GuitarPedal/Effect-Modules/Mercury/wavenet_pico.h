// The "pico" WaveNet of Mercury: a Neural Amp Modeler (NAM) WaveNet with two layer arrays of two channels, a kernel of 3 and dilations from 1 to 512 (20 layers in all, 454 weights),
// computed one sample at a time without allocation. A plain C++ rewrite of the network of RTNeural-NAM (https://github.com/jatinchowdhury18/RTNeural-NAM, BSD-3-Clause, Copyright (c)
// 2024 jatinchowdhury18) as it was used in the Mercury pedal of the FunBox project (https://github.com/GuitarML/Funbox, MIT licence, Copyright (c) 2024 Keith Bloemer), which ran it on
// RTNeural and Eigen. See Mercury/README.md. Tested against the original in tests/host/test_mercury.cpp.
//
//   layer array 1: rechannel (1 -> 2) , layers with dilations 1 2 4 8 16 32 64, head (2 -> 2), no head bias
//   layer array 2: rechannel (2 -> 2) , layers with dilations 128 256 512 1 2 4 8 16 32 64 128 256 512, head (2 -> 1) with a bias; the first array's head outputs are added to its
//                  head inputs, and the first array's last layer outputs are its inputs
//   layer: z = conv(input) + mixin(x), a = tanh(z); the head gets a added to it; the output is the input + 1x1(a)
// The input sample is also the condition of every layer. The output is the head times the head scale (the last weight).
#pragma once
#ifndef MERCURY_WAVENET_PICO_H
#define MERCURY_WAVENET_PICO_H

#include "mercury_models.h"
#include <cmath>
#include <cstddef>

namespace mercury {

// tanh as the original used it (math_approx::tanh<3>): x (1 + 0.1834 x^2) / sqrt(1 + (x (1 + 0.1834 x^2))^2)
struct ApproxTanh {
    static float tanh(float x) {
        const float p = x * (1.0f + 0.183428244899f * x * x);
        return p / std::sqrt(p * p + 1.0f);
    }
};
struct ExactTanh {
    static float tanh(float x) { return std::tanh(x); }
};

constexpr int kChannels = 2;
constexpr int kKernel = 3;
constexpr int kLayers1 = 7;
constexpr int kLayers2 = 13;
constexpr int kDilations1[kLayers1] = {1, 2, 4, 8, 16, 32, 64};
constexpr int kDilations2[kLayers2] = {128, 256, 512, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512};

constexpr int constexprSpan(int dilation) { return (kKernel - 1) * dilation + 1; } // the samples a layer looks at

constexpr int totalHistory() {
    int n = 0;
    for (int d : kDilations1) n += constexprSpan(d);
    for (int d : kDilations2) n += constexprSpan(d);
    return n;
}
constexpr int kHistorySlots = totalHistory();                      // 4112 slots of two channels
constexpr int kReceptiveField = kHistorySlots - (kLayers1 + kLayers2) + 1; // the samples that the output depends on

template <class Maths = ApproxTanh> class PicoWavenet {
  public:
    PicoWavenet() { Reset(); }

    // The weights, kWeightCount floats in the order of the NAM export, then the head scale last
    void LoadWeights(const float *w) {
        const float *p = w;
        LoadArray(a1_, 1, 2, false, kDilations1, kLayers1, layers1_, p);
        LoadArray(a2_, 2, 1, true, kDilations2, kLayers2, layers2_, p);
        headScale_ = *p++;
        Reset();
    }

    // Clears the history
    void Reset() {
        for (float &v : history_) v = 0.0f;
        for (Layer &l : layers1_) l.pos = 0;
        for (Layer &l : layers2_) l.pos = 0;
        int offset = 0; // each layer's ring has its own part of the history
        for (int i = 0; i < kLayers1; i++) {
            layers1_[i].hist = &history_[kChannels * offset];
            offset += constexprSpan(kDilations1[i]);
        }
        for (int i = 0; i < kLayers2; i++) {
            layers2_[i].hist = &history_[kChannels * offset];
            offset += constexprSpan(kDilations2[i]);
        }
    }

    // Runs silence through the network until it has settled: the network is a finite impulse response, so after its receptive field the output is the same as after any longer time
    // (with a bias, the output for silence is not zero)
    void Prewarm() {
        Reset();
        for (int i = 0; i < kReceptiveField + 256; i++) Process(0.0f);
    }

    float Process(float x) {
        float head1[kChannels] = {0.0f, 0.0f};
        float r[kChannels];
        // layer array 1
        for (int c = 0; c < kChannels; c++) r[c] = a1_.rechannel[c][0] * x;
        for (Layer &l : layers1_) RunLayer(l, r, head1, x);
        float headOut1[kChannels];
        for (int o = 0; o < kChannels; o++) {
            float s = 0.0f;
            for (int c = 0; c < kChannels; c++) s += a1_.head[o][c] * head1[c];
            headOut1[o] = s;
        }
        // layer array 2: the first array's layer outputs are its inputs, its head outputs the head inputs
        float in2[kChannels] = {r[0], r[1]};
        for (int c = 0; c < kChannels; c++) {
            float s = 0.0f;
            for (int i = 0; i < kChannels; i++) s += a2_.rechannel[c][i] * in2[i];
            r[c] = s;
        }
        float head2[kChannels] = {headOut1[0], headOut1[1]};
        for (Layer &l : layers2_) RunLayer(l, r, head2, x);
        float out = a2_.headBias[0];
        for (int c = 0; c < kChannels; c++) out += a2_.head[0][c] * head2[c];
        return out * headScale_;
    }

  private:
    struct Layer {
        float conv[kChannels][kChannels][kKernel]; // [out][in][tap]: tap k looks (kKernel - 1 - k) * dilation samples back
        float convBias[kChannels];
        float mixin[kChannels];
        float w1[kChannels][kChannels];
        float b1[kChannels];
        int dilation = 1;
        int span = 1;
        float *hist = nullptr;
        int pos = 0;
    };
    struct Array {
        float rechannel[kChannels][2]; // [out][in] (in is 1 or 2)
        float head[2][kChannels];      // [out][in]
        float headBias[2];
    };

    static void LoadArray(Array &a, int in, int head, bool headBias, const int *dilations, int count, Layer *layers, const float *&p) {
        for (int c = 0; c < kChannels; c++)
            for (int i = 0; i < in; i++) a.rechannel[c][i] = *p++;
        for (int n = 0; n < count; n++) {
            Layer &l = layers[n];
            for (int o = 0; o < kChannels; o++)
                for (int i = 0; i < kChannels; i++)
                    for (int k = 0; k < kKernel; k++) l.conv[o][i][k] = *p++;
            for (int o = 0; o < kChannels; o++) l.convBias[o] = *p++;
            for (int o = 0; o < kChannels; o++) l.mixin[o] = *p++; // condition size 1
            for (int o = 0; o < kChannels; o++)
                for (int i = 0; i < kChannels; i++) l.w1[o][i] = *p++;
            for (int o = 0; o < kChannels; o++) l.b1[o] = *p++;
            l.dilation = dilations[n];
            l.span = constexprSpan(dilations[n]);
        }
        for (int o = 0; o < head; o++)
            for (int c = 0; c < kChannels; c++) a.head[o][c] = *p++;
        for (int o = 0; o < 2; o++) a.headBias[o] = 0.0f;
        if (headBias)
            for (int o = 0; o < head; o++) a.headBias[o] = *p++;
    }

    // One layer: r is the layer input and becomes its output, head collects the activations
    static void RunLayer(Layer &l, float *r, float *head, float x) {
        // the input goes into the history (a ring of `span` slots of two channels)
        float *cur = &l.hist[kChannels * l.pos];
        cur[0] = r[0];
        cur[1] = r[1];
        float z[kChannels];
        for (int o = 0; o < kChannels; o++) z[o] = l.convBias[o] + l.mixin[o] * x;
        for (int k = 0; k < kKernel; k++) {
            int idx = l.pos - (kKernel - 1 - k) * l.dilation;
            if (idx < 0) idx += l.span;
            const float *h = &l.hist[kChannels * idx];
            for (int o = 0; o < kChannels; o++)
                for (int i = 0; i < kChannels; i++) z[o] += l.conv[o][i][k] * h[i];
        }
        if (++l.pos >= l.span) l.pos = 0;
        float a[kChannels];
        for (int o = 0; o < kChannels; o++) {
            a[o] = Maths::tanh(z[o]);
            head[o] += a[o];
        }
        for (int o = 0; o < kChannels; o++) {
            float s = l.b1[o];
            for (int i = 0; i < kChannels; i++) s += l.w1[o][i] * a[i];
            r[o] += s;
        }
    }

    Array a1_{}, a2_{};
    Layer layers1_[kLayers1];
    Layer layers2_[kLayers2];
    float headScale_ = 0.0f;
    float history_[kChannels * kHistorySlots];
};

} // namespace mercury

#endif
