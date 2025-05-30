#define _USE_MATH_DEFINES
#include "Biquad2.h"
#include <cmath>

namespace AudioLib {
Biquad2::Biquad2() { ClearBuffers(); }

Biquad2::Biquad2(FilterType filterType, float samplerate) {
    Type = filterType;
    SetSamplerate(samplerate);

    SetGainDb(0.0);
    Frequency = samplerate / 4;
    SetQ(0.5);
    ClearBuffers();
}

Biquad2::~Biquad2() {}

float Biquad2::GetSamplerate() { return samplerate; }

void Biquad2::SetSamplerate(float value) {
    samplerate = value;
    Update();
}

float Biquad2::GetGainDb() { return std::log10(gain) * 20; }

void Biquad2::SetGainDb(float value) { SetGain(std::pow(10, value / 20)); }

float Biquad2::GetGain() { return gain; }

void Biquad2::SetGain(float value) {
    if (value == 0)
        value = 0.001; // -60dB

    gain = value;
}

float Biquad2::GetQ() { return _q; }

void Biquad2::SetQ(float value) {
    if (value == 0)
        value = 1e-12;
    _q = value;
}

vector<float> Biquad2::GetA() { return vector<float>({1, a1, a2}); }

vector<float> Biquad2::GetB() { return vector<float>({b0, b1, b2}); }

void Biquad2::Update() {
    float omega = 2 * M_PI * Frequency / samplerate;
    float sinOmega = std::sin(omega);
    float cosOmega = std::cos(omega);

    float sqrtGain = 0.0;
    float alpha = 0.0;

    if (Type == FilterType::LowShelf || Type == FilterType::HighShelf) {
        alpha = sinOmega / 2 * std::sqrt((gain + 1 / gain) * (1 / Slope - 1) + 2);
        sqrtGain = std::sqrt(gain);
    } else {
        alpha = sinOmega / (2 * _q);
    }

    switch (Type) {
    case FilterType::LowPass:
        b0 = (1 - cosOmega) / 2;
        b1 = 1 - cosOmega;
        b2 = (1 - cosOmega) / 2;
        a0 = 1 + alpha;
        a1 = -2 * cosOmega;
        a2 = 1 - alpha;
        break;
    case FilterType::HighPass:
        b0 = (1 + cosOmega) / 2;
        b1 = -(1 + cosOmega);
        b2 = (1 + cosOmega) / 2;
        a0 = 1 + alpha;
        a1 = -2 * cosOmega;
        a2 = 1 - alpha;
        break;
    case FilterType::BandPass:
        b0 = alpha;
        b1 = 0;
        b2 = -alpha;
        a0 = 1 + alpha;
        a1 = -2 * cosOmega;
        a2 = 1 - alpha;
        break;
    case FilterType::Notch:
        b0 = 1;
        b1 = -2 * cosOmega;
        b2 = 1;
        a0 = 1 + alpha;
        a1 = -2 * cosOmega;
        a2 = 1 - alpha;
        break;
    case FilterType::Peak:
        b0 = 1 + (alpha * gain);
        b1 = -2 * cosOmega;
        b2 = 1 - (alpha * gain);
        a0 = 1 + (alpha / gain);
        a1 = -2 * cosOmega;
        a2 = 1 - (alpha / gain);
        break;
    case FilterType::LowShelf:
        b0 = gain * ((gain + 1) - (gain - 1) * cosOmega + 2 * sqrtGain * alpha);
        b1 = 2 * gain * ((gain - 1) - (gain + 1) * cosOmega);
        b2 = gain * ((gain + 1) - (gain - 1) * cosOmega - 2 * sqrtGain * alpha);
        a0 = (gain + 1) + (gain - 1) * cosOmega + 2 * sqrtGain * alpha;
        a1 = -2 * ((gain - 1) + (gain + 1) * cosOmega);
        a2 = (gain + 1) + (gain - 1) * cosOmega - 2 * sqrtGain * alpha;
        break;
    case FilterType::HighShelf:
        b0 = gain * ((gain + 1) + (gain - 1) * cosOmega + 2 * sqrtGain * alpha);
        b1 = -2 * gain * ((gain - 1) + (gain + 1) * cosOmega);
        b2 = gain * ((gain + 1) + (gain - 1) * cosOmega - 2 * sqrtGain * alpha);
        a0 = (gain + 1) - (gain - 1) * cosOmega + 2 * sqrtGain * alpha;
        a1 = 2 * ((gain - 1) - (gain + 1) * cosOmega);
        a2 = (gain + 1) - (gain - 1) * cosOmega - 2 * sqrtGain * alpha;
        break;
    }

    float g = 1 / a0;

    b0 = b0 * g;
    b1 = b1 * g;
    b2 = b2 * g;
    a1 = a1 * g;
    a2 = a2 * g;
}

float Biquad2::GetResponse(float freq) {
    float phi = std::pow((std::sin(2 * M_PI * freq / (2.0 * samplerate))), 2);
    return (std::pow(b0 + b1 + b2, 2.0) - 4.0 * (b0 * b1 + 4.0 * b0 * b2 + b1 * b2) * phi + 16.0 * b0 * b2 * phi * phi) /
           (std::pow(1.0 + a1 + a2, 2.0) - 4.0 * (a1 + 4.0 * a2 + a1 * a2) * phi + 16.0 * a2 * phi * phi);
}

void Biquad2::ClearBuffers() {
    y = 0;
    x2 = 0;
    y2 = 0;
    x1 = 0;
    y1 = 0;
}

} // namespace AudioLib