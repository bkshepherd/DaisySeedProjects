#pragma once

#include "BandShifter.h"

#include <gcem.hpp>

//=============================================================================
class OctaveGenerator {
  public:
    OctaveGenerator(float sample_rate) {
        for (int i = 0; i < 80; ++i) {
            const auto center = centerFreq(i);
            const auto bw = bandwidth(i);
            _shifters.emplace_back(center, sample_rate, bw);
        }
    }

    void update(float sample) {
        _up1 = 0;
        _down1 = 0;
        _down2 = 0;

        for (auto &shifter : _shifters) {
            shifter.update(sample);
            _up1 += shifter.up1();
            _down1 += shifter.down1();
            _down2 += shifter.down2();
        }
    }

    float up1() const { return _up1; }

    float down1() const { return _down1; }

    float down2() const { return _down2; }

  private:
    static constexpr float centerFreq(const int n) { return 480 * gcem::pow(2.0f, (0.027f * n)) - 420; }

    static constexpr float bandwidth(const int n) {
        const float f0 = centerFreq(n - 1);
        const float f1 = centerFreq(n);
        const float f2 = centerFreq(n + 1);
        const float a = (f2 - f1);
        const float b = (f1 - f0);
        return 2.0f * (a * b) / (a + b);
    }

    std::vector<BandShifter> _shifters;

    float _up1 = 0;
    float _down1 = 0;
    float _down2 = 0;
};
